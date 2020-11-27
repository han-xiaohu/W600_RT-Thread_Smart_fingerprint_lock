#ifndef USER_TYPE_H_
#define USER_TYPE_H_
#include <rtthread.h>

enum {IR_MSG = 1, FP_MSG, REMOTE_MSG, DOOR_STATE_MSG};
enum {LED_RED = 1, LED_GREEN, LED_BLUE, NET_OK, NET_ERR, NET_SET};

struct onenet_msg {
	uint32_t type;
	uint32_t id;
};


#define INFRARED_UP_WAIT       (rt_tick_from_millisecond(3 * 1000))
#define FINGER_UP_WAIT       (rt_tick_from_millisecond(3 * 1000))
#define REMOTE_UP_WAIT       (rt_tick_from_millisecond(3 * 1000))
#define DOOR_STATE_UP_WAIT       (rt_tick_from_millisecond(300))




rt_err_t wdt_init(void);
rt_err_t wifi_connect(void);

rt_err_t door_control_init(void);
rt_err_t onenet_control_init(void);
rt_err_t infrared_control_init(void);
rt_err_t fingerprint_control_init(void);




#endif

