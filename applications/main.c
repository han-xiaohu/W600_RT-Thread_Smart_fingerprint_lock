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

