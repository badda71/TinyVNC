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
	char lasterrmsg[VJOY_UDP_ERRMSG_LENGTH];
	int lasterrno;
	struct vjoy_udp_coeffs coeffs;
	u32 metabutton; // KEY_SELECT per default
};

extern int vjoy_udp_client_init(struct vjoy_udp_client *client, char *hostname, int port);
extern int vjoy_udp_client_update(
	struct vjoy_udp_client *client,
	u32 buttons,
	circlePosition *posCp,
	circlePosition *posStk,
	touchPosition *touch,
	accelVector *accel,
	angularRate *gyro);
extern int vjoy_udp_client_shutdown(struct vjoy_udp_client *client);