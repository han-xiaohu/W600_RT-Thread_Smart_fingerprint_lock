#include <rtthread.h>
#include <rtdevice.h>
#include "board.h"
#include "user_def.h"

#define DBG_TAG "finger_control"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


/* 配置相关引脚 */
#define DETECT_PIN     14
#define POWERON_PIN    28


/* 信号量 */
static struct rt_semaphore detect_sem;
static struct rt_semaphore rx_sem;

extern struct rt_semaphore door_open_sem;
extern struct rt_mailbox mb_led;
extern struct rt_messagequeue mq_send_onenet;


#define SAMPLE_UART_NAME  		"uart1"
#define DATA_CMD_END      		0xF5       /* 结束位 */
#define ONE_DATA_MAXLEN   		20         /* 不定长数据的最大长度 */
static rt_device_t serial;		


/* 模块驱动代码 */
#define DATA_START			0xf5	//数据包开始
#define DATA_END			0xf5	//数据包结束
#define CMD_SEARCH  		0x0c	//1:N比对
#define BUF_N 	8

rt_uint8_t rx_buffer[ONE_DATA_MAXLEN];   //接收返回信息
rt_uint8_t tx_buffer[ONE_DATA_MAXLEN];   //发送命令或者数据

rt_uint8_t data_rx_end;       //接收返回信息结束标志
rt_uint8_t rx_flag = 1;  			 //接收标志位


//等待数据包接收完成
rt_uint8_t FPC1020_WaitData(void)
{
	rt_uint8_t i=0;
	for(i=30; i>0; i--)//等待指纹芯片返回
	{
		rt_thread_mdelay(50);
		if(data_rx_end)
		{
			break;
		}
	}

	if(i==0)return RT_FALSE;//指纹芯片没有返回
	else return RT_TRUE;
}


//计算校验值
rt_uint8_t CmdGenCHK(rt_uint8_t wLen,rt_uint8_t *ptr)
{
	rt_uint8_t i,temp = 0;
	
	for(i = 0; i < wLen; i++)
	{
		temp ^= *(ptr + i);
	}
	return temp;
}

//发送控制指纹芯片指令
void FPC1020_SendPackage(rt_uint8_t wLen ,rt_uint8_t *ptr)
{
	unsigned int i=0,len=0;
	rx_flag=1;
	tx_buffer[0] = DATA_START; //指令包
	for(i = 0; i < wLen; i++) // data in packet 
	{
		tx_buffer[1+i] = *(ptr+i);
	} 

	tx_buffer[wLen + 1] = CmdGenCHK(wLen, ptr); //Generate checkout data
	tx_buffer[wLen + 2] = DATA_END;
	len = wLen + 3;

	data_rx_end = 0;

	rt_device_write(serial, 0, tx_buffer, len);
	
}

//处理返回数据
rt_uint8_t FPC1020_CheckPackage(rt_uint8_t cmd,rt_uint8_t *q1,rt_uint8_t *q2,rt_uint8_t *q3)
{
	rt_uint8_t flag = RT_FALSE;
	rx_flag=1;
	if(!FPC1020_WaitData())
	{
		return flag; //等待接收返回信息
	}
	if(data_rx_end) 
	{
		data_rx_end = 0;//清数据包接收标志
	}
	else 
	{
		return flag;
	}

	if(rx_buffer[0] != DATA_START) return flag;
	if(rx_buffer[1] != cmd) return flag;
	
	if(cmd == CMD_SEARCH)
	{
		if((1 == rx_buffer[4])||(2 == rx_buffer[4])||(3 == rx_buffer[4]))
		{
			flag = RT_TRUE;
		}
	}
	
	if(flag == RT_TRUE)
	{
		if(q1 != RT_NULL)
			*q1 = rx_buffer[2];
		if(q2 != RT_NULL)
			*q2 = rx_buffer[3];
		if(q3 != RT_NULL)
			*q3 = rx_buffer[4];
	}
	
	
	return flag;
}

//启动检查
rt_uint8_t FPC1020_CheckStart(void)
{
	rt_uint8_t flag = RT_TRUE;
	rx_flag=1;
	if(FPC1020_WaitData()) 
	{
		if(data_rx_end)
		{
			rx_flag=1;
			data_rx_end = 0; //清数据包接收标志
		}
		if(rx_buffer[0] == DATA_START)
		{
			return RT_TRUE;
		}
	}
	else
	{
		return RT_FALSE; 
	}

	return flag;
}

//查找指纹 1:N
void FP_Search(void)
{
  rt_uint8_t buf[BUF_N];
  
  *buf = CMD_SEARCH;         
  *(buf+1) = 0x00;
  *(buf+2) = 0x00;
  *(buf+3) = 0x00;
  *(buf+4) = 0x00;

  FPC1020_SendPackage(5, buf);
}


//查找指纹 1:N
rt_uint8_t FPC1020_Search(rt_uint8_t *q1, rt_uint8_t *q2, rt_uint8_t *q3)
{

	FP_Search();
	return FPC1020_CheckPackage(CMD_SEARCH, q1, q2, q3);
}



/* 检测引脚中断回调函数 */
void detect_pin_ind(void *args)
{
	if(rt_pin_read(DETECT_PIN)){
		LOG_D("finger detected!\n");
		rt_pin_write(POWERON_PIN, PIN_HIGH);
		rt_sem_release(&detect_sem);
	}
}


/* 接收数据回调函数 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    /* 串口接收到数据后产生中断，调用此回调函数，然后发送接收信号量 */
    if (size > 0)
    {
        rt_sem_release(&rx_sem);
    }
    return RT_EOK;
}

static char uart_sample_get_char(void)
{
    char ch;

    while (rt_device_read(serial, 0, &ch, 1) == 0)
    {
        rt_sem_control(&rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&rx_sem, RT_WAITING_FOREVER);
    }
    return ch;
}

/* 数据解析线程 */
static void data_parsing(void)
{
    rt_uint8_t ch;
    static rt_uint8_t i = 0;

    while (1)
    {
        ch = uart_sample_get_char();
        if(rx_flag == 1 && ch == DATA_CMD_END)
        {
			i=0;
			rx_flag=0;
			data_rx_end = 0;
        }
		if(!rx_flag)
		{
			rx_buffer[i++] = ch;
		}

		if(i == 8 && !rx_flag) 
		{
			data_rx_end = 0xFF;
			i=0;
		}
    }
}


/* 指纹识别线程 */
static void finger_identification_entry(void)
{
	rt_uint8_t uid[2] = {0};
	struct onenet_msg mqbuf = {0};
	rt_tick_t last_tick = 0;
	while(1)
	{
		rt_sem_control(&detect_sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&detect_sem, RT_WAITING_FOREVER);
		
		if(FPC1020_WaitData())
		{
			rt_mb_send(&mb_led, LED_RED);
			LOG_D("SCANING...");
			if(FPC1020_Search(&uid[0], &uid[1], RT_NULL))
			{
				rt_sem_release(&door_open_sem);
				LOG_D("SUCCESS!\n");
				
				//避免频繁发送数据
				if(rt_tick_get() - last_tick > FINGER_UP_WAIT)
				{
					last_tick = rt_tick_get();
					/* 发送消息到消息队列中 */
					mqbuf.type = FP_MSG;
					mqbuf.id = (rt_uint32_t)((uid[0] << 8) | uid[1]);
					rt_mq_send(&mq_send_onenet, &mqbuf, sizeof(mqbuf));
				}
			}
			else
			{
				rt_mb_send(&mb_led, LED_BLUE);
				LOG_D("FAIL\n");
			}
		}
		rt_pin_write(POWERON_PIN, PIN_LOW);
	}
}



rt_err_t fingerprint_control_init(void)
{
    rt_err_t ret = RT_EOK;

    /* 查找系统中的串口设备 */
    serial = rt_device_find(SAMPLE_UART_NAME);
    if (!serial)
    {
        LOG_D("find %s failed!\n", SAMPLE_UART_NAME);
        return RT_ERROR;
    }

    /* 初始化信号量 */
    rt_sem_init(&rx_sem, "rx_sem", 0, RT_IPC_FLAG_FIFO);
	rt_sem_init(&detect_sem, "detect_sem", 0, RT_IPC_FLAG_FIFO);

	
    /* 以中断接收及轮询发送模式打开串口设备 */
    rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    /* 设置接收回调函数 */
    rt_device_set_rx_indicate(serial, uart_rx_ind);
	

	/* 手指检测引脚为输入模式 */
    rt_pin_mode(DETECT_PIN, PIN_MODE_INPUT_PULLDOWN);
    /* 绑定中断，下降沿模式，回调函数名为beep_on */
    rt_pin_attach_irq(DETECT_PIN, PIN_IRQ_MODE_RISING, detect_pin_ind, RT_NULL);
    /* 使能中断 */
    rt_pin_irq_enable(DETECT_PIN, PIN_IRQ_ENABLE);
	
	
	/* 指纹模块电源控制输出模式 */
	rt_pin_mode(POWERON_PIN, PIN_MODE_OUTPUT);
	rt_pin_write(POWERON_PIN, PIN_LOW);
	
	
    /* 创建 serial 线程 */
    rt_thread_t thread = rt_thread_create("finger_parsing", (void (*)(void *parameter))data_parsing, RT_NULL, 512, 5, 10); //stack_size 256
    /* 创建成功则启动线程 */
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }
	
	thread = rt_thread_create("finger_identification", (void (*)(void *parameter))finger_identification_entry, RT_NULL, 1024, 20, 10); //stack_size 512
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

