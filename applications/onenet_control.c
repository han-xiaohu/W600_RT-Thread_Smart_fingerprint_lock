/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-09-01     ZYLX         first implementation
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <onenet.h>
#include <string.h>
#include <easyflash.h>
#include "cJSON_util.h"
#include "user_def.h"

#define DBG_TAG "onenet_control"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


/* 信号量 */
extern struct rt_semaphore door_open_sem;


/* 消息队列 */
extern struct rt_messagequeue mq_send_onenet;


static void onenet_cmd_rsp_cb(uint8_t *recv_data, size_t recv_size, uint8_t **resp_data, size_t *resp_size)
{
	static struct onenet_msg mqbuf = {0};
	static rt_tick_t last_tick = 0;
	
	char res_buf[30] = {0};
	int open_id = 0;
	
	LOG_D("recv data is %.*s", recv_size, recv_data);
	
	cJSON *root = cJSON_Parse((char *)recv_data); 
	cJSON_item_get_number(root,"opendoor",&open_id);
  
    if (open_id != 0)
    {
        rt_sem_release(&door_open_sem);
		LOG_D("the door is open!");
		if(rt_tick_get() - last_tick > REMOTE_UP_WAIT)
		{
			last_tick = rt_tick_get();
			
			/* 发送消息到消息队列中 */
			mqbuf.type = REMOTE_MSG;
			mqbuf.id = open_id;
			rt_mq_send(&mq_send_onenet, &mqbuf, sizeof(mqbuf));
		}
				
        rt_snprintf(res_buf, sizeof(res_buf), "the door is open!");
    }

    /* 开发者必须使用 ONENET_MALLOC 为响应数据申请内存 */
    *resp_data = (uint8_t *)ONENET_MALLOC(strlen(res_buf) + 1);

    rt_strncpy((char*)*resp_data, res_buf, strlen(res_buf));

    *resp_size = strlen(res_buf);
	
	if (root) 
		cJSON_Delete(root);
}



static void onenet_upload_entry(void *parameter)
{
	struct onenet_msg mqbuf = {0};
	char ds_json[50];
	
	while (1)
    {
        /* 从消息队列中接收消息 */
        if (rt_mq_recv(&mq_send_onenet, &mqbuf, sizeof(mqbuf), RT_WAITING_FOREVER) == RT_EOK)
        {
            LOG_D("recv msg from msg queue, the type:%d\n", mqbuf.type);
			
			if(mqbuf.type == DOOR_STATE_MSG)
			{
				if (onenet_mqtt_upload_digit("door_status", mqbuf.id) < 0)
				{
					LOG_E("upload has an error, stop uploading");
				}
			}
			else
			{
				rt_memset(ds_json,0,sizeof(ds_json));
				rt_sprintf(ds_json,"{\"type\":%d,\"id\":%d}",mqbuf.type,mqbuf.id);
				if (onenet_mqtt_upload_string("door_function", ds_json) < 0)
				{
					LOG_E("upload has an error, stop uploading");
				}
			}
		}
		rt_thread_mdelay(100);
    }
}


static void network_state_confirm_entry(void *parameter)
{
	rt_uint32_t err_count = 0;
	while (1)
    {
		LOG_D("rt_wlan_is_connected : %d",rt_wlan_is_connected());
		LOG_D("rt_wlan_is_ready : %d",rt_wlan_is_ready());
		if(!(rt_wlan_is_connected() && rt_wlan_is_ready()))
		{
			if(err_count++ >= 6)
			{
				wdg_reset();
			}
		}
		else
		{
			err_count = 0;
		}
		rt_thread_mdelay(10000);
    }
}


rt_err_t onenet_control_init(void)
{
	rt_err_t ret = RT_EOK;
	rt_uint32_t err_count = 0;
	
	while(wifi_connect() != RT_EOK)
	{
		rt_thread_mdelay(5000); //连接失败5s后重连
		if(err_count++ >= 3)
		{
			wdg_reset();
		}
		
	}
	
	rt_wlan_config_autoreconnect(1);
	LOG_D("wifi connected!");
	
	
	if(onenet_mqtt_init() == 0)
	{
		LOG_D("onenet mqtt inited!");
		onenet_set_cmd_rsp_cb(onenet_cmd_rsp_cb); //设置 onenet 回调响应函数 
	}
	
	/* 创建线程 */
    rt_thread_t thread = rt_thread_create("network_state", network_state_confirm_entry, RT_NULL, 512, 28, 50); //stack_size 256
    if (thread)
    {
        rt_thread_startup(thread);
    }
	
	/* 创建线程 */
    thread = rt_thread_create("onenet_send", onenet_upload_entry, RT_NULL, 2 * 1024, 26, 50); //stack_size 1024
    if (thread)
    {
        rt_thread_startup(thread);
    }
	
	return ret;
}


rt_err_t onenet_port_save_device_info(char *dev_id, char *api_key)
{
    EfErrCode err = EF_NO_ERR;
    /* 保存设备 ID */
    err = ef_set_and_save_env("dev_id", dev_id);
    if (err != EF_NO_ERR)
    {
        LOG_E("save device info(dev_id : %s) failed!", dev_id);
        return -RT_ERROR;
    }

    /* 保存设备 api_key */
    err = ef_set_and_save_env("api_key", api_key);
    if (err != EF_NO_ERR)
    {
        LOG_E("save device info(api_key : %s) failed!", api_key);
        return -RT_ERROR;
    }

    /* 保存环境变量：已经注册 */
    err = ef_set_and_save_env("already_register", "1");
    if (err != EF_NO_ERR)
    {
        LOG_E("save already_register failed!");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t onenet_port_get_register_info(char *dev_name, char *auth_info)
{
    rt_uint32_t udid[2] = {0}; /* 唯一设备标识 */
    EfErrCode err = EF_NO_ERR;

    /* 获得 MAC 地址 */
    if (rt_wlan_get_mac((rt_uint8_t *)udid) != RT_EOK)
    {
        LOG_E("get mac addr err!! exit");
        return -RT_ERROR;
    }

    /* 设置设备名和鉴权信息 */
    rt_snprintf(dev_name, ONENET_INFO_AUTH_LEN, "%d%d", udid[0], udid[1]);
    rt_snprintf(auth_info, ONENET_INFO_AUTH_LEN, "%d%d", udid[0], udid[1]);

    /* 保存设备鉴权信息 */
    err = ef_set_and_save_env("auth_info", auth_info);
    if (err != EF_NO_ERR)
    {
        LOG_E("save auth_info failed!");
        return -RT_ERROR;
    }

    return RT_EOK;
}

rt_err_t onenet_port_get_device_info(char *dev_id, char *api_key, char *auth_info)
{
    char *info = RT_NULL;

    /* 获取设备 ID */
    info = ef_get_env("dev_id");
    if (info == RT_NULL)
    {
        LOG_E("read dev_id failed!");
        return -RT_ERROR;
    }
    else
    {
        rt_snprintf(dev_id, ONENET_INFO_AUTH_LEN, "%s", info);
    }

    /* 获取 api_key */
    info = ef_get_env("api_key");
    if (info == RT_NULL)
    {
        LOG_E("read api_key failed!");
        return -RT_ERROR;
    }
    else
    {
        rt_snprintf(api_key, ONENET_INFO_AUTH_LEN, "%s", info);
    }

    /* 获取设备鉴权信息 */
    info = ef_get_env("auth_info");
    if (info == RT_NULL)
    {
        LOG_E("read auth_info failed!");
        return -RT_ERROR;
    }
    else
    {
        rt_snprintf(auth_info, ONENET_INFO_AUTH_LEN, "%s", info);

    }

    return RT_EOK;
}

rt_bool_t onenet_port_is_registed(void)
{
    char *already_register = RT_NULL;

    /* 检查设备是否已经注册 */
    already_register = ef_get_env("already_register");
    if (already_register == RT_NULL)
    {
        return RT_FALSE;
    }

    return already_register[0] == '1' ? RT_TRUE : RT_FALSE;
}

