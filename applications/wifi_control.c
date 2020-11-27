#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <wlan_mgnt.h>
#include <wlan_prot.h>
#include <wlan_cfg.h>
#include <easyflash.h>
#include "wifi_config.h"
#include "oneshot.h"
#include "user_def.h"


#define DBG_TAG "wifi_control"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


/* 配置相关引脚 */
#define WIFI_CFG_PIN     27


/* 信号量 */
static rt_sem_t net_ready = RT_NULL;
struct rt_semaphore  web_sem;


/* 邮箱控制块 */
extern struct rt_mailbox mb_net;



#define WLAN_SSID               "HUAWEI-316"
#define WLAN_PASSWORD           "hndxwlwgc"
#define NET_READY_TIME_OUT       (rt_tick_from_millisecond(15 * 1000))
#define NET_CFG_TIME_OUT       (rt_tick_from_millisecond(180 * 1000))


static void
wifi_ready_callback(int event, struct rt_wlan_buff *buff, void *parameter)
{
	rt_mb_send(&mb_net, NET_OK);
    rt_kprintf("%s\n", __FUNCTION__);
    rt_sem_release(net_ready);
}

static void
wifi_connect_callback(int event, struct rt_wlan_buff *buff, void *parameter)
{
	rt_mb_send(&mb_net, NET_OK);
    rt_kprintf("%s\n", __FUNCTION__);
    if ((buff != RT_NULL) && (buff->len == sizeof(struct rt_wlan_info)))
    {
        rt_kprintf("ssid : %s \n", ((struct rt_wlan_info *)buff->data)->ssid.val);
    }
}

static void
wifi_disconnect_callback(int event, struct rt_wlan_buff *buff, void *parameter)
{
	rt_mb_send(&mb_net, NET_ERR);
    rt_kprintf("%s\n", __FUNCTION__);
	
    if ((buff != RT_NULL) && (buff->len == sizeof(struct rt_wlan_info)))
    {
        rt_kprintf("ssid : %s \n", ((struct rt_wlan_info *)buff->data)->ssid.val);
    }
}

static void
wifi_connect_fail_callback(int event, struct rt_wlan_buff *buff, void *parameter)
{
	rt_mb_send(&mb_net, NET_ERR);
    rt_kprintf("%s\n", __FUNCTION__);
    if ((buff != RT_NULL) && (buff->len == sizeof(struct rt_wlan_info)))
    {
        rt_kprintf("ssid : %s \n", ((struct rt_wlan_info *)buff->data)->ssid.val);
    }
}


rt_err_t wifi_save_config(char *ssid, char* passwd)
{
	rt_err_t result = RT_EOK;
	EfErrCode err = EF_NO_ERR;
	
	err = ef_set_and_save_env("wifi_ssid", ssid);
    if (err != EF_NO_ERR)
    {
        LOG_E("save wifi_ssid failed!");
		result = RT_ERROR;
    }
	
	if(passwd != RT_NULL)
	{
		err = ef_set_and_save_env("wifi_passwd", passwd);
		if (err != EF_NO_ERR)
		{
			LOG_E("save wifi_passwd failed!");
			result = RT_ERROR;
		}
	}
	
	err = ef_set_and_save_env("wifi_config_flag", "ok");
    if (err != EF_NO_ERR)
    {
        LOG_E("save wifi_config_flag failed!");
		result = RT_ERROR;
    }
	
	return result;
}


rt_err_t wifi_get_config(char *ssid, char* passwd)
{
	char *info = RT_NULL;
	rt_err_t result = RT_EOK;
	
    info = ef_get_env("wifi_ssid");
    if (info != RT_NULL)
    {
		rt_strncpy(ssid,info,rt_strlen(info));
        LOG_E(ssid);
    }
	else
	{
		result = RT_ERROR;
	}
	
	info = ef_get_env("wifi_passwd");
    if (info != RT_NULL)
    {
		rt_strncpy(passwd,info,rt_strlen(info));
        LOG_E(passwd);
    }
	
	return result;
}



static void oneshot_result_cb(int state, unsigned char *ssid_i, unsigned char *passwd_i)
{
    char *ssid_temp = (char *)ssid_i;
    char *passwd_temp = (char *)passwd_i;

    if (RT_TRUE == rt_wlan_ap_is_active())
        rt_wlan_ap_stop();

    /* 配网回调超时返回 */
    if (state != 0)
    {
        LOG_E("Receive wifi info timeout(%d). exit!", state);
        return;
    }
    if (ssid_temp == RT_NULL)
    {
        LOG_E("SSID is NULL. exit!");
        return;
    }
    LOG_D("Receive ssid:%s passwd:%s", ssid_temp == RT_NULL ? "" : ssid_temp, passwd_temp == RT_NULL ? "" : passwd_temp);

	wifi_save_config(ssid_temp,passwd_temp);
	
    /* 通知 ssid 与 key 接收完成 */
    rt_sem_release(&web_sem);
}


rt_err_t wifi_oneshot_config(void)
{
    rt_err_t result = RT_EOK;
	
    /* 配置 wifi 工作模式 */
    rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);
    rt_wlan_set_mode(RT_WLAN_DEVICE_AP_NAME, RT_WLAN_AP);
    rt_sem_init(&web_sem, "web_sem", 0, RT_IPC_FLAG_FIFO);

    /* 启动 AP 热点 */
    rt_wlan_start_ap("door_wifi_config", NULL);

    rt_thread_mdelay(2000);
	
    /* 一键配网：APWEB 模式 */
    result = wm_oneshot_start(WM_APWEB, oneshot_result_cb);
    if (result != 0)
    {
        LOG_E("web config wifi start failed");
        return result;
    }

    LOG_D("web oneshot config wifi start...");
	rt_mb_send(&mb_net, NET_SET);
    rt_sem_take(&web_sem, RT_WAITING_FOREVER);

    /* 配网结束，关闭 AP 热点 */
    if (RT_TRUE == rt_wlan_ap_is_active())
        rt_wlan_ap_stop();

    return result;
}





rt_err_t wifi_connect(void)
{
    rt_err_t result = RT_EOK;
	char *info = RT_NULL;
	
	char ssid[20] = {0};
	char passwd[20] = {0};


	/* WIFI配置引脚为输入模式 */
    rt_pin_mode(WIFI_CFG_PIN, PIN_MODE_INPUT_PULLUP);
	if(rt_pin_read(WIFI_CFG_PIN) == PIN_LOW)
	{
		wifi_oneshot_config();
	}
	
	
    info = ef_get_env("wifi_config_flag");
	if(!(info != RT_NULL && rt_strstr((const char *)info, "ok") != RT_NULL))
    {
		wifi_oneshot_config();
    }
	
	if(wifi_get_config(ssid,passwd) != RT_EOK)
	{
		rt_strncpy(ssid,WLAN_SSID,rt_strlen(WLAN_SSID));
		rt_strncpy(passwd,WLAN_PASSWORD,rt_strlen(WLAN_PASSWORD));
		wifi_save_config(WLAN_SSID,WLAN_PASSWORD);
	}

	
    /* Configuring WLAN device working mode */
    rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);
    /* station connect */
    rt_kprintf("start to connect ap ...\n");
    net_ready = rt_sem_create("net_ready", 0, RT_IPC_FLAG_FIFO);
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY,
            wifi_ready_callback, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_CONNECTED,
            wifi_connect_callback, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED,
            wifi_disconnect_callback, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_CONNECTED_FAIL,
            wifi_connect_fail_callback, RT_NULL);

    /* connect wifi */
    result = rt_wlan_connect(ssid, passwd);
	
    if (result == RT_EOK)
    {
        /* waiting for IP to be got successfully  */
        result = rt_sem_take(net_ready, NET_READY_TIME_OUT);
        if (result == RT_EOK)
        {
            rt_kprintf("networking ready!\n");
        }
        else
        {
            rt_kprintf("wait ip got timeout!\n");
        }
        rt_wlan_unregister_event_handler(RT_WLAN_EVT_READY);
    }
    else
    {
        rt_kprintf("connect failed!\n");
    }

    return result;
}
