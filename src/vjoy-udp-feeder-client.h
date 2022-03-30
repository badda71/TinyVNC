#include <3ds.h>

#ifndef SGN
#define SGN(x) \
	({ __typeof__ (x) _a = (x); \
	((__typeof__ (x))0 < _a) - (_a < (__typeof__ (x))0);})
#endif

#define VJOY_UDP_ERRMSG_LENGTH 128

struct vjoy_udp_coeffs {
	int cp_deadzone;	// C-Pad
	int cp_max;
	int cp_threshold;	// threshold is used to reduce network traffic. Only values that changed more than threshhold since last sent are transferred to the server
	int cs_deadzone;	// C-Stick
	int cs_max;
	int cs_threshold;
	int acc_deadzone;	// Accelerometer
	int acc_max;
	int acc_threshold;
	int gy_deadzone;	// Gyro
	int gy_max;
	int gy_threshold;
};

struct vjoy_udp_client {
	int socket;
	struct sockaddr_in addr1;	// for button / joy / touch data
	struct sockaddr_in addr2;	// for motion data
	char lasterrmsg[VJOY_UDP_ERRMSG_LENGTH];
	int lasterrno;
	struct vjoy_udp_coeffs coeffs;
	u32 metabutton; // KEY_SELECT per default
	int mouse_relative; // 0 for absolute, 1 for relative mouse movements (default 0)
	int mouse_sensitivity; // mouse sensitivity for relative movements (default 10)
	int interval; // in which intervals (ms) should we send new data to the server? (default 75)
};

extern int vjoy_udp_client_init(struct vjoy_udp_client *client, char *hostname, int port, int motionport);
extern int vjoy_udp_client_update(
	struct vjoy_udp_client *client,
	u32 buttons,
	circlePosition *posCp,
	circlePosition *posStk,
	touchPosition *touch,
	accelVector *accel,
	angularRate *gyro,
	float slider);
extern int vjoy_udp_client_shutdown(struct vjoy_udp_client *client);
