#include <rtthread.h>
#include <rtdevice.h>
#include <easyflash.h>
#include <fal.h>
#include "user_def.h"


#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


int main(void)
{
	
    fal_init();
    easyflash_init();

	wdt_init();
	door_control_init();
	infrared_control_init();
	fingerprint_control_init();
	onenet_control_init();
	

	return 0;

}

