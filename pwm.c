/*
 * 程序清单：这是一个 PWM 设备使用例程
 * 例程导出了 pwm_led_sample 命令到控制终端
 * 命令调用格式：pwm_led_sample
 * 程序功能：通过 PWM 设备控制 LED 灯的亮度，可以看到LED不停的由暗变到亮，然后又从亮变到暗。
*/

#include <rtthread.h>
#include <rtdevice.h>

#define PWM_DEV_NAME        "pwm"  /* PWM设备名称 */
#define PWM_DEV_CHANNEL     1       /* PWM通道 */

struct rt_device_pwm *pwm_dev;      /* PWM设备句柄 */

static int pwm_led_sample(int argc, char *argv[])
{
 

    while (1)
    {
        rt_thread_mdelay(500);
//        if (dir)
//        {
//            pulse += 5000;      /* 从0值开始每次增加5000ns */
//        }
//        else
//        {
//            pulse -= 5000;      /* 从最大值开始每次减少5000ns */
//        }
//        if (pulse >= period)
//        {
//            dir = 0;
//        }
//        if (0 == pulse)
//        {
//            dir = 1;
//        }

//        /* 设置PWM周期和脉冲宽度 */
//        rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, period, pulse);
    }
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(pwm_led_sample, pwm sample);