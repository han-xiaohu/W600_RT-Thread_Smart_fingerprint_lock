#include <rtthread.h>
#include <rtdevice.h>
#include "board.h"
#include "infrared.h"
#include "user_def.h"

#define DBG_TAG "infrared_control"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


/* 配置相关引脚 */
#define IR_INT_PIN  20


/* 信号量 */
extern struct rt_semaphore door_open_sem;
extern struct rt_messagequeue mq_send_onenet;


/* 红外检测线程 */
static void infrared_identification_entry(void)
{
	struct infrared_decoder_data infrared_data;
	struct onenet_msg mqbuf = {0};
	rt_tick_t last_tick = 0;
	
	while(1)
	{
		if(infrared_read("nec",&infrared_data) == RT_EOK)  
        {
			if(infrared_data.data.nec.key == 0xA8)
			{
				rt_sem_release(&door_open_sem);
				if(rt_tick_get() - last_tick > INFRARED_UP_WAIT)
				{
					last_tick = rt_tick_get();
					
					/* 发送消息到消息队列中 */
					mqbuf.type = IR_MSG;
					mqbuf.id = infrared_data.data.nec.key;
					rt_mq_send(&mq_send_onenet, &mqbuf, sizeof(mqbuf));
				}
			}
			LOG_I("RECEIVE OK: addr:0x%02X key:0x%02X repeat:%d",
                infrared_data.data.nec.addr, infrared_data.data.nec.key, infrared_data.data.nec.repeat);
			while(infrared_read("nec",&infrared_data) == RT_EOK){rt_thread_mdelay(10);}
        }  	
		rt_thread_mdelay(100);
	}
}


rt_err_t infrared_control_init(void)
{
	rt_err_t ret = RT_EOK;
	
	rt_pin_mode(IR_INT_PIN,PIN_MODE_INPUT);
	
	 /* 选择 NEC 解码器 */
    ir_select_decoder("nec");
	
	 /* 创建 serial 线程 */
    rt_thread_t thread = rt_thread_create("infrared", (void (*)(void *parameter))infrared_identification_entry, RT_NULL, 1024, 15, 10);  //stack_size 512
    /* 创建成功则启动线程 */
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }
	
	return ret;
}

