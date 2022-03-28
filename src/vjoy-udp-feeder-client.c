/*
Client to vJoy-UDP-Feeder for N3DS
https://github.com/klach/vjoy-udp-feeder
(c) 2022 Sebastian Weber
*/
#include <3ds.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include "vjoy-udp-feeder-client.h"
#include "utilities.h"

struct vjoy_packet {
	// Valid value for Axis/Slider/Dial members are in range 0x0001 – 0x8000
	// lButtons: Valid value is range 0x00000000 (all 32 buttons are unset) to 0xFFFFFFFF (all buttons are set). The least-significant-bit representing the lower-number button (e.g. button #1).
	// contPov: Valid value is either 0xFFFFFFFF (neutral) or in the range of 0 to 35999
	// discPovs: The lowest nibble is used for switch #1, the second nibble for switch #2, the third nibble for switch #3 and the highest nibble for switch #4. Each nibble supports one of the following values: 0x0 North (forward), 0x1 East (right), 0x2 South (backwards), 0x3 West (Left), 0xF Neutral
	u32 wAxisX;		// posCp.dx
	u32 wAxisY;		// posCp.dy
	u32 wAxisZ;		
	u32 wAxisXRot;	// posStk.dx
	u32 wAxisYRot;	// posStk.dy
	u32 wAxisZRot;
	u32 wSlider;	// touch.px
	u32 wDial;		// touch.py
	u32 lButtons;	// kHeld
	u32 contPov;	// calculated continuous C-Stick Position
	u32 discPovs;	// discrete C-Stick Position
	// below custom values - not passed to vjoy by vJoyUDPFeeder, might be used by different UDP2Joy implementation
	u32 wAxisXAccel;
	u32 wAxisYAccel;
	u32 wAxisZAccel;
	u32 wAxisXGyro;
	u32 wAxisYGyro;
	u32 wAxisZGyro;
};

int vjoy_udp_client_init(struct vjoy_udp_client *client, char *hostname, int port)
{
	bzero(client, sizeof(*client));
	client->socket = -1;

	// resolve hostname and connect a UDP socket
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;
	char buf[10];

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */

	s = getaddrinfo(hostname, itoa(port, buf, 10), &hints, &result);
	if (s != 0) {
		snprintf(client->lasterrmsg, VJOY_UDP_ERRMSG_LENGTH, "getaddrinfo: %s", gai_strerror(s));
		client->lasterrno = s;
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

		close(sfd);
	}

	if (rp == NULL) {               /* No address succeeded */
		snprintf(client->lasterrmsg, VJOY_UDP_ERRMSG_LENGTH, "Could not connect");
		client->lasterrno = errno;
		return -1;
	}
	client->socket = sfd;
	freeaddrinfo(result);           /* No longer needed */
	
	// default meta button is select
	client->metabutton = KEY_SELECT;

	// mouse setup
	client->mouse_relative = 0; // 0 for absolute, 1 for relative mouse movements (default 0)
	client->mouse_sensitivity = 10; // mouse sensitivity for relative movements (default 10)

	// other default values
	client->interval = 75; // 75ms

	// set default coeffs
	client->coeffs.cp_deadzone = 20;
	client->coeffs.cp_max = 150;
	client->coeffs.cp_threshold = 0;

	client->coeffs.cs_deadzone = 10;
	client->coeffs.cs_max = 100;
	client->coeffs.cs_threshold = 0;	

	client->coeffs.acc_deadzone = 20;
	client->coeffs.acc_max= 900;
	client->coeffs.acc_threshold = 10;

	client->coeffs.gy_deadzone = 20;
	client->coeffs.gy_max= 900;
	client->coeffs.gy_threshold = 10;

	return 0;
}

static int vjoy_udp_client_sendUDP(struct vjoy_udp_client *client, void *data, int size)
{
	int ret;
	if (client->socket < 0) {
		return -1;
	}
	ret=send(client->socket, data, size, 0);
	return ret;
}

int vjoy_udp_client_shutdown(struct vjoy_udp_client *client)
{
	if (client->socket >= 0) close(client->socket);
	client->socket = -1;
	return 0;
}

static const u32 buttons3ds[] = {
	KEY_A,
	KEY_B,
	KEY_X,
	KEY_Y,
	KEY_L,
	KEY_R,
	KEY_ZL,
	KEY_ZR,
	KEY_SELECT,
	KEY_START,
	KEY_DUP,
	KEY_DDOWN,
	KEY_DLEFT,
	KEY_DRIGHT,
	KEY_TOUCH,
//	KEY_CPAD_UP,
//	KEY_CPAD_DOWN,
//	KEY_CPAD_LEFT,
//	KEY_CPAD_RIGHT,
//	KEY_CSTICK_UP,
//	KEY_CSTICK_DOWN,
//	KEY_CSTICK_LEFT,
//	KEY_CSTICK_RIGHT,
	0
};

static u32 vjoy_udp_client_transform(int a, int dead, int max, u32 thresh, u32 maxnew, int keepdead)
{
	int mymax = keepdead?max:max-dead;
	if (thresh) a = a - (a % thresh) * (a < 0 ? -1 : 1); // quantize down to threshold quant
	if (abs(a) < dead) a=0;	// set dead zone to zero
	else if (!keepdead) a = a > 0 ? a - dead : a + dead; // remove dead zone if applicable
	if (a > mymax) a = mymax;	// clamp between -max and max
	if (a < -mymax) a= -mymax;
	a = a + mymax;	// move to positive region (vJoy only accepts positive axis values)
	a = (a * (maxnew/2)) / mymax; // rescale to vJoy numer range
	return a;
}

int vjoy_udp_client_update(	// all parameters can be NULL except client
	struct vjoy_udp_client *client,
	u32 buttons,
	circlePosition *posCp,
	circlePosition *posStk,
	touchPosition *touch,
	accelVector *accel,
	angularRate *gyro)
{
	static struct vjoy_packet oldp = {0};
	static u64 lastupdate = 0;
	static int count = 0;
	static struct {
		u32 buttons;
		int posCp_x;
		int posCp_y;
		int posStk_x;
		int posStk_y;
		int touch_x;
		int touch_y;
		int touch_dx;
		int touch_dy;
		int gyro_x;
		int gyro_y;
		int gyro_z;
		int accel_x;
		int accel_y;
		int accel_z;
	} data = {0};
	struct vjoy_packet newp = {0}, sendp = {0};

	// collect data until we trigger an update
	data.buttons |= buttons;
	if (posCp) {
		data.posCp_x += posCp->dx;
		data.posCp_y += posCp->dy;
	}
	if (posStk) {
		data.posStk_x += posStk->dx;
		data.posStk_y += posStk->dy;
	}
	if (touch) {
		static int oldx=0, oldy=0, oldstate=0;
		if (buttons & KEY_TOUCH) {
			if (!oldstate) {
				oldx = touch->px;
				oldy = touch->py;
			}
			data.touch_x += touch->px;
			data.touch_y += touch->py;
			data.touch_dx += touch->px - oldx;
			data.touch_dy += touch->py - oldy;
			oldx = touch->px;
			oldy = touch->py;
		} else {
			data.touch_x += oldx;
			data.touch_y += oldy;
		}
		oldstate = buttons & KEY_TOUCH;
	}
	if (gyro) {
		data.gyro_x += gyro->x;
		data.gyro_y += gyro->y;
		data.gyro_z += gyro->z;
	}
	if (accel) {
		data.accel_x += accel->x;
		data.accel_y += accel->y;
		data.accel_z += accel->z;
	}
	++count;

	u64 now = getmicrotime();
	if (!lastupdate) lastupdate = now;
	if (now - lastupdate > client->interval) { // done collecting data - now send!
		lastupdate = now;

		// C-Pad
		if (posCp) {
			data.buttons &= 0x0FFFFFFF; // blank out the C-Pad buttons, we are sending real coordinates
			newp.wAxisX = vjoy_udp_client_transform(data.posCp_x / count, client->coeffs.cp_deadzone, client->coeffs.cp_max, client->coeffs.cp_threshold, 0x8000, 0);
			newp.wAxisY = vjoy_udp_client_transform(data.posCp_y / count, client->coeffs.cp_deadzone, client->coeffs.cp_max, client->coeffs.cp_threshold, 0x8000, 0);
		} else {
			if (data.buttons & KEY_CPAD_UP) newp.wAxisY=0x8000;
			else if (data.buttons & KEY_CPAD_DOWN) newp.wAxisY=0;
			else newp.wAxisY=0x4000;
			if (data.buttons & KEY_CPAD_RIGHT) newp.wAxisX=0x8000;
			else if (data.buttons & KEY_CPAD_LEFT) newp.wAxisX=0;
			else newp.wAxisX=0x4000;
		}
		// C-Stick
		int x=0,y=0;
		if (posStk) {
			data.buttons &= 0xF0FFFFFF; // blank out the C-Stick buttons, we are sending real coordinates
			newp.wAxisXRot= vjoy_udp_client_transform(data.posStk_x / count, client->coeffs.cs_deadzone, client->coeffs.cs_max, client->coeffs.cs_threshold, 0x8000, 0);
			newp.wAxisYRot= vjoy_udp_client_transform(data.posStk_y / count, client->coeffs.cs_deadzone, client->coeffs.cs_max, client->coeffs.cs_threshold, 0x8000, 0);
			x=posStk->dx; y=posStk->dy;
		} else {
			if (data.buttons & KEY_CSTICK_UP) newp.wAxisYRot=0x8000;
			else if (data.buttons & KEY_CSTICK_DOWN) newp.wAxisYRot=0;
			else newp.wAxisYRot=0x4000;
			if (data.buttons & KEY_CSTICK_RIGHT) newp.wAxisXRot=0x8000;
			else if (data.buttons & KEY_CSTICK_LEFT) newp.wAxisXRot=0;
			else newp.wAxisXRot=0x4000;
			x=(newp.wAxisXRot>>8) - 0x40; y=(newp.wAxisYRot>>8) - 0x40;
		}
		// C-Stick on Pov hat #1
		if (x * x + y * y > 40 * 40) {
			newp.contPov  = ((int)(atan2(x, y) * 18000.0 * M_1_PI) + 36000) % 36000;
			newp.discPovs = ((int)((newp.contPov + 4500) / 9000) % 4) | 0xFFFFFF00;
		} else {
			newp.contPov  = 0xFFFFFFFF;
			newp.discPovs = 0xFFFFFF0F;
		}
		//D-Pad on Pov hat #2 (discrete only)
		if (data.buttons & KEY_DRIGHT) newp.discPovs |= 0x10;
		else if (data.buttons & KEY_DLEFT) newp.discPovs |= 0x30;
		else if (data.buttons & KEY_DDOWN) newp.discPovs |= 0x20;
		else if (!(data.buttons & KEY_DUP)) newp.discPovs |= 0xF0;

		// Touch (relative or absolute)
		if (touch) {
			if (client->mouse_relative) {
				newp.wAxisZ =    LIMIT((data.touch_dx * 0x4000 * client->mouse_sensitivity) / (320*10) + 0x4000, 0, 0x8000);
				newp.wAxisZRot = LIMIT((data.touch_dy * 0x4000 * client->mouse_sensitivity) / (240*10) + 0x4000, 0, 0x8000);
			} else {
				newp.wAxisZ =    (data.touch_x / count) * 0x8000 / 320;
				newp.wAxisZRot = (data.touch_y / count) * 0x8000 / 240;
			}
		}

		// Accelerometer
		if (accel) {
			newp.wAxisXAccel = vjoy_udp_client_transform(data.accel_x / count, client->coeffs.acc_deadzone, client->coeffs.acc_max, client->coeffs.acc_threshold, 0x8000, 1);
			newp.wAxisYAccel = vjoy_udp_client_transform(data.accel_y / count, client->coeffs.acc_deadzone, client->coeffs.acc_max, client->coeffs.acc_threshold, 0x8000, 1);
			newp.wAxisZAccel = vjoy_udp_client_transform(data.accel_z / count, client->coeffs.acc_deadzone, client->coeffs.acc_max, client->coeffs.acc_threshold, 0x8000, 1);
		}

		// Gyro
		if (gyro) {
			newp.wAxisXGyro = vjoy_udp_client_transform(data.gyro_x / count, client->coeffs.gy_deadzone, client->coeffs.gy_max, client->coeffs.gy_threshold, 0x8000, 1);
			newp.wAxisYGyro = vjoy_udp_client_transform(data.gyro_y / count, client->coeffs.gy_deadzone, client->coeffs.gy_max, client->coeffs.gy_threshold, 0x8000, 1);
			newp.wAxisZGyro = vjoy_udp_client_transform(data.gyro_z / count, client->coeffs.gy_deadzone, client->coeffs.gy_max, client->coeffs.gy_threshold, 0x8000, 1);
		}

		// Buttons
		u32 nbut = 0;
		// re-order buttons according to buttons3ds array, remove meta button
		for (int i1=0, i2=0; buttons3ds[i1] && i2<32; ++i1) {
			if (buttons3ds[i1] == client->metabutton) continue;
			if ((data.buttons & buttons3ds[i1]) != 0) nbut |= BIT(i2);
			i2++;
		}
		//  bitshift 16 if meta button is pressed
		if (client->metabutton) {
			if ((data.buttons & client->metabutton) != 0) nbut = nbut << 16;
			else nbut &= 0x0000FFFF;
		}
		newp.lButtons = nbut;

		count = 0;
		bzero(&data, sizeof(data));

		// do we have anythign new to send?
		if (memcmp(&newp, &oldp, sizeof(newp))) {
			memcpy(&oldp, &newp, sizeof(newp));
			// send
			int *i1 = (int*)&newp; int *i2 = (int*)&sendp;
			for (int i=0; i < sizeof(newp)/sizeof(int); i++) {
				i2[i]=htonl(i1[i]);
			}
			return vjoy_udp_client_sendUDP(client, &sendp, sizeof(sendp));
		}
	}
	return 0;
}
