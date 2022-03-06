/*
https://v1993.github.io/cemuhook-protocol/ 
https://github.com/yuk27/BetterJoyForDolphin/blob/master/BetterJoyForDolphin/UpdServer.cs
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
#include "dsu-server.h"
#include "utilities.h"

#define BUFSIZE 4096
#define MAX_VERSION 1001
#define DSU_DEFAULT_PORT 26760
#define CLIENT_TIMEOUT 5

static u32 serverID=0;

int dsu_server_init(struct dsu_server *server, int port)
{
	if (port==0) port=DSU_DEFAULT_PORT;

	bzero(server, sizeof(*server));
	struct sockaddr_in servaddr;

	// Creating socket file descriptor
	if ( (server->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		snprintf(server->lasterrmsg, DSU_ERRMSG_LENGTH, "socket creation failed");
		server->lasterrno = errno;
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family    = AF_INET; // IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

	// Bind the socket with the server address
	if ( bind(server->socket, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) {
		snprintf(server->lasterrmsg, DSU_ERRMSG_LENGTH, "bind failed");
		server->lasterrno = errno;
		return -1;
	}
	if (!serverID) {
		srand(time(NULL));
		while (!serverID) serverID= ((rand() & 0x0000FFFF) | (rand() << 16));
	}
	struct in_addr sin_addr;
	sin_addr.s_addr = gethostid();
	inet_ntop( AF_INET, &sin_addr, server->ipstr, INET_ADDRSTRLEN );
	server->port=port;
	
	// default button map settings (dsu server supports more button than the 3ds
	// has, so we need to map the other buttons with the help of a meta button that
	// must be held down at the same time)
	server->button_meta = KEY_SELECT;
	server->button_plus = KEY_DUP;
	server->button_minus = KEY_DDOWN;
	server->button_LSTCK_PUSH = KEY_DLEFT;
	server->button_RSTCK_PUSH = KEY_DRIGHT;

	// set default coeffs
	server->coeffs.cp_deadzone = 20;
	server->coeffs.cp_max = 150;
	server->coeffs.cp_threshold = 0;

	server->coeffs.cs_deadzone = 10;
	server->coeffs.cs_max = 100;
	server->coeffs.cs_threshold = 0;	

	server->coeffs.acc_deadzone = 20;
	server->coeffs.acc_threshold = 10;

	server->coeffs.gy_deadzone = 20;
	server->coeffs.gy_threshold = 10;
	HIDUSER_GetGyroscopeRawToDpsCoefficient(&(server->coeffs.gy_raw2dps));

	return 0;
}

int dsu_server_shutdown(struct dsu_server *server)
{
	if (server->socket >= 0) close(server->socket);
	server->socket = -1;
	return 0;
}

unsigned int crc32calc(unsigned char *message, int msglen) {
   int i, j;
   unsigned int byte, crc, mask;
   static unsigned int table[256];

   /* Set up the table, if necessary. */
   if (table[1] == 0) {
      for (byte = 0; byte <= 255; byte++) {
         crc = byte;
         for (j = 7; j >= 0; j--) {    // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
         }
         table[byte] = crc;
      }
   }

   /* Through with table setup, now calculate the CRC. */
   i = 0;
   crc = 0xFFFFFFFF;
   while (i < msglen) {
      crc = (crc >> 8) ^ table[(crc ^ message[i]) & 0xFF];
      i = i + 1;
   }
   return ~crc;
}

static void dsu_fill_controller_info(u8 nr, u8 *buf)
{
	buf[0] = nr; // slot nr
	if (nr==0) {
		buf[1] = 2; // Slot state (2: connected)
		buf[2] = 2; // Device model (2: full gyro)
		
		// only check batt every 5 seconds
		static u8 batt, charge;
		static u64 batttime = 0;
		u64 now=time(NULL);
		if (now -batttime > 5) {
			PTMU_GetBatteryLevel (&batt);
			PTMU_GetBatteryChargeState (&charge);
			batttime = now;
		}
		if (charge) {
			buf[10] = batt==5 ? 0xEF: 0xEE;
		} else {
			buf[10] = batt < 3 ? batt + 1: batt;
		}
	}
}

static int dsu_send_packet(struct dsu_server *server, struct sockaddr *cli_addr, u8 *buf, int len)
{
	buf[0]='D';
	buf[1]='S';
	buf[2]='U';
	buf[3]='S';

	*((u16*)(buf+4)) = 1001;
	*((u16*)(buf+6)) = len - 16;
	*((u32*)(buf+12)) = serverID;
	*((u32*)(buf+8)) = 0;
	*((u32*)(buf+8)) = crc32calc(buf, len);
	socklen_t clilen = sizeof(struct sockaddr_in);
	int n=sendto(server->socket, buf, len, 0, cli_addr, clilen);
//hex_dump((char*)buf,len,"packet sent");
	if (n != len) {
		if (n < 0) {
			snprintf(server->lasterrmsg, DSU_ERRMSG_LENGTH, "sendto: %s", strerror(errno));
			server->lasterrno = errno;
		} else {
			snprintf(server->lasterrmsg, DSU_ERRMSG_LENGTH, "Packet sent incomplete: %d/%d bytes", n, len);
			server->lasterrno = -1;
		}
		//log_citra(server->lasterrmsg);
		return -1;
	}
	return 0;
}

static int dsu_process_message(struct dsu_server *server, u8 *buf, int len, struct sockaddr *cli_addr)
{
	int idx=0;
//log_citra("processing message len %d", len);

	// drop messages that are too short (less than 20 bytes)
	if ( len < 20 ) {
		//log_citra("  dropping - too short");
		return 0;
	}

	// Magic String
	if ( buf[0] != 'D' || buf[1] != 'S' || buf[2] != 'U' || buf[3] != 'C') {
		//log_citra("  dropping - magic string wrong");
		return 0;
	}
	idx += 4;

	// Version
	u16 version = *((u16*)(buf+idx));
	if (version > MAX_VERSION) {
		//log_citra("  dropping - max version exceeded %d", version);
		return 0;
	}
	idx += 2;

	// Packet Length
	u16 length = *((u16*)(buf+idx));
	length += 16;
	if (len < length) { // Drop packet if it’s too short
		//log_citra("  dropping - packet shorter than length+16 %d < %d", len, length);
		return 0;
	}
	len = length; // truncate if it’s too long
	idx += 2;

	// CRC32
	u32 crc32a = *((u32*)(buf+idx));
	//zero out the crc32 in the packet once we got it since that's whats needed for calculation
	buf[idx++] = 0;
	buf[idx++] = 0;
	buf[idx++] = 0;
	buf[idx++] = 0;
	u32 crc32b = crc32calc(buf, len);
	if (crc32a != crc32b) {
		//log_citra("  dropping - crc32 check failed %p != %p", crc32a, crc32b);
		return 0;
	}

	// client ID
	u32 clientID = *((u32*)(buf+idx));
	idx += 4;

	// messageType
	u32 mType = *((u32*)(buf+idx));
	idx += 4;

//log_citra("mType = %p", mType);

	// Protocol version information
	if (mType == 0x100000) {
		u8 sbuf[22]; // header 16 + 6 body
		*((u32*)(sbuf+16)) = 0x100000;
		*((u16*)(sbuf + 20)) = MAX_VERSION;
		dsu_send_packet(server, cli_addr, sbuf, 22);
//log_citra("received version request");

	// Information about connected controllers
	} else if (mType == 0x100001) {

		// get number of pad requests
		int numPadRequests = *((int*)(buf+idx));
		idx += 4;
		if (numPadRequests < 1 || numPadRequests > 4) {
			//log_citra("  dropping - numPadRequests %d", numPadRequests);
			return 0;
		}

		// check the requested pad numbers
		for (int i=0; i < numPadRequests; i++) {
			u8 currRequest = buf[idx + i];
			if (currRequest < 0 || currRequest > 3) {
				//log_citra("  dropping - requested Pad %d: %d", i, currRequest);				
				return 0;
			}
		}

		// send a package for each requested pad number
		u8 sbuf[32]; // 16 header + 16 body
		for (u8 i = 0; i < numPadRequests; i++) {
			bzero(sbuf, 32);
			*((u32*)(sbuf+16)) = 0x100001;
			dsu_fill_controller_info(buf[idx + i], sbuf+20);
			dsu_send_packet(server, cli_addr, sbuf, 32);
		}
//log_citra("received info request for %d slots", numPadRequests);

	// Actual controllers data
	} else if (mType == 0x100002) {
		// get bitmask of actions you should take
		u8 regFlags = buf[idx++];
		if (regFlags == 2) { // we only support slot based registration
			//log_citra("  dropping - MAC-based registration requested");
			return 0;
		}
		u8 idToReg = buf[idx++];
		if (regFlags == 0 && idToReg != 0) { // only slot 0 supported
			//log_citra("  dropping - slot %d requested", idToReg);
			return 0;
		}
		
		// check if client is already in list
		struct dsu_client *c;

		for(c = server->cli_first; c != NULL; c = c->next)
			if (c->id == clientID) break;
		
		// client is not in list, add ...
		if (!c) {
			c = malloc(sizeof(*c));
			bzero(c, sizeof(*c));
			c->id = clientID;
			if (!server->cli_first) {
				server->cli_first = c;
			} else {
				server->cli_last->next = c;
			}	
			c->prev = server->cli_last;
			server->cli_last = c;
		}
		// update timestamp and address
		c->timestamp = time(NULL);
		c->addr = *((struct sockaddr_in *)cli_addr);
//log_citra("received controler data feed request");
	}
	return 0;
}

int dsu_server_run(struct dsu_server *server)
{
	int n;
	static u8 buffer[BUFSIZE];	

	if (server->socket < 0) return -1;

	// check if we have incoming packets
	struct timeval t = {0};
	fd_set read_fds;
	FD_ZERO(&read_fds);
	FD_SET(server->socket, &read_fds);
	while (1) {
		if (select(server->socket+1, &read_fds, 0, 0, &t) < 0) {
			snprintf(server->lasterrmsg, DSU_ERRMSG_LENGTH, "select failed");
			server->lasterrno = errno;
			return -1;
		}
		if(!FD_ISSET(server->socket, &read_fds)) break;
			
		// receive packet and work on it
		struct sockaddr_in cli_addr;
		socklen_t clilen = sizeof(cli_addr);
		n = recvfrom(server->socket, buffer, BUFSIZE, 0, (struct sockaddr*)&cli_addr, &clilen);
		if (n<0) {
			snprintf(server->lasterrmsg, DSU_ERRMSG_LENGTH, "recvfrom failed");
			server->lasterrno = errno;
			return -1;
		}
//hex_dump((char*)buffer,n,"packet received");
		dsu_process_message(server, buffer, n, (struct sockaddr*)&cli_addr);
	}
	return 0;
}

static u32 dsu_server_transform(int a, int dead, int max, u32 thresh)
{
	if (thresh) a = a - (a % thresh) * (a < 0 ? -1 : 1); // quantize down to threshold quant
	if (abs(a) < dead) a=0;	// set dead zone to zero
	if (max && a > max) a = max;	// clamp between -max and max
	if (max && a < -max) a= -max;
	return a;
}

// all parameters can be NULL except server
int dsu_server_update(struct dsu_server *server, u32 but, circlePosition *posCp, circlePosition *posStk, touchPosition *touch, accelVector *accel, angularRate *gyro)
{
	static u8 oldacc[12]={0};
	static u64 accts = 0;
	
	u8 sbuf[100] = {0};
	int i;
	
	*((u32*)(sbuf+16)) = 0x100002;
	dsu_fill_controller_info(0, sbuf + 20);
	
	sbuf[31] = 1;	// Is controller connected (1 if connected, 0 if not)
	//*((u32*)(sbuf+32)) = 0; // packet number for this client - done below in client loop
	// DPAD_LEFT, DPAD_DOWN, DPAD_RIGHT, DPAD_UP, +, RSTCK_PUSH, LSTCK_PUSH, -
	if (but & server->button_meta) sbuf[36] =
		(but & server->button_plus ? 0x08 : 0) |
		(but & server->button_RSTCK_PUSH ? 0x04 : 0) |
		(but & server->button_LSTCK_PUSH ? 0x02 : 0) |
		(but & server->button_minus ? 0x01 : 0);
	else sbuf[36] =
		(but & KEY_DLEFT ? 0x80 : 0) |
		(but & KEY_DDOWN ? 0x40 : 0) |
		(but & KEY_DRIGHT ? 0x20 : 0) |
		(but & KEY_DUP ? 0x10 : 0);
	sbuf[37] =	// Y, B, A, X, R, L, ZR, ZL
		(but & KEY_Y ? 0x80 : 0) |
		(but & KEY_B ? 0x40 : 0) |
		(but & KEY_A ? 0x20 : 0) |
		(but & KEY_X ? 0x10 : 0) |
		(but & KEY_R ? 0x08 : 0) |
		(but & KEY_L ? 0x04 : 0) |
		(but & KEY_ZR ? 0x02 : 0) |
		(but & KEY_ZL ? 0x01 : 0);
	sbuf[38] = // HOME Button (0 or 1)
		but & KEY_SELECT && but & KEY_START ? 1 : 0;
	sbuf[39] = // Touch Button (0 or 1)
		but & KEY_TOUCH ? 1 : 0;
	if (posCp) {
		// Left stick X (plus rightward, 128=neutral)
		i=dsu_server_transform(posCp->dx, server->coeffs.cp_deadzone, server->coeffs.cp_max, server->coeffs.cp_threshold);
		i=((i+server->coeffs.cp_max) * 128) / server->coeffs.cp_max;
		sbuf[40] = i>255?255:i;
		// Left stick Y (plus upward, 128=neutral)
		i=dsu_server_transform(posCp->dy, server->coeffs.cp_deadzone, server->coeffs.cp_max, server->coeffs.cp_threshold);
		i=((i+server->coeffs.cp_max) * 128) / server->coeffs.cp_max;
		sbuf[41] = i>255?255:i;
	}
	if (posStk) {
		// Right stick X (plus rightward, 128=neutral)
		i=dsu_server_transform(posStk->dx, server->coeffs.cs_deadzone, server->coeffs.cs_max, server->coeffs.cs_threshold);
		i=((i+server->coeffs.cs_max) * 128) / server->coeffs.cs_max;
		sbuf[42] = i>255?255:i;
		// Right stick Y (plus upward, 128=neutral)
		i=dsu_server_transform(posStk->dy, server->coeffs.cs_deadzone, server->coeffs.cs_max, server->coeffs.cs_threshold);
		i=((i+server->coeffs.cs_max) * 128) / server->coeffs.cs_max;
		sbuf[43] = i>255?255:i;
	}
	sbuf[44] = but & KEY_DLEFT ? 255 : 0;	// Analog D-Pad Left (0 or 255)
	sbuf[45] = but & KEY_DDOWN ? 255 : 0;	// Analog D-Pad Down (0 or 255)
	sbuf[46] = but & KEY_DRIGHT ? 255 : 0;	// Analog D-Pad Right (0 or 255)
	sbuf[47] = but & KEY_DUP ? 255 : 0;		// Analog D-Pad Up (0 or 255)
	sbuf[48] = but & KEY_Y ? 255 : 0;		// Analog Y (0 or 255)
	sbuf[49] = but & KEY_B ? 255 : 0;		// Analog B (0 or 255)
	sbuf[50] = but & KEY_A ? 255 : 0;		// Analog A (0 or 255)
	sbuf[51] = but & KEY_X ? 255 : 0;		// Analog X (0 or 255)
	sbuf[52] = but & KEY_R ? 255 : 0;		// Analog R (0 or 255)
	sbuf[53] = but & KEY_L ? 255 : 0;		// Analog L (0 or 255)
	sbuf[54] = but & KEY_ZR ? 255 : 0;		// Analog ZR (0 or 255)
	sbuf[55] = but & KEY_ZL ? 255 : 0;		// Analog ZL (0 or 255)
	sbuf[56] = but & KEY_TOUCH ? 1 : 0;		// first touch? Is touch active (1 if active, else 0)
	if (touch) {
		// Touch id (should be the same for one continious touch)
		static u8 laststate=0, lastid=0;
		const u8 state=sbuf[56];
		if (state && !laststate) do {++lastid;} while (!lastid);
		laststate = state;
		sbuf[57] = state ? lastid : 0;

		*((u16*)(sbuf+58)) = touch->px;	// Touch X position
		*((u16*)(sbuf+60)) = touch->py;	// Touch Y position
	}
	// sbuf[62]-sbuf[67]: second touch (n/a)

	if (accel) {
		// Accelerometer X axis (in Gs) (/520)
		i=dsu_server_transform(accel->x, server->coeffs.acc_deadzone, 0, server->coeffs.acc_threshold);
		*((float*)(sbuf+76)) = -(float)i / 510.0f;
		// Accelerometer Y axis (in Gs)
		i=dsu_server_transform(accel->y, server->coeffs.acc_deadzone, 0, server->coeffs.acc_threshold);		
		*((float*)(sbuf+80)) = (float)i / 510.0f;
		// Accelerometer Z axis (in Gs)
		i=dsu_server_transform(accel->z, server->coeffs.acc_deadzone, 0, server->coeffs.acc_threshold);
		*((float*)(sbuf+84)) = -(float)i / 510.0f;
		// Motion data timestamp in microseconds, update only with accelerometer (but not gyro only) changes
		if (memcmp(oldacc, sbuf+76, 12)) {
			memcpy(oldacc, sbuf+76, 12);
			accts = getmicrotime();
		}
		*((u64*)(sbuf+68)) = accts;
	}

	if (gyro) {
		// Gyroscope pitch (in deg/s)
		i=dsu_server_transform(gyro->x, server->coeffs.gy_deadzone, 0, server->coeffs.gy_threshold);
		*((float*)(sbuf+88)) = -(float)i / server->coeffs.gy_raw2dps;
		// Gyroscope yaw (in deg/s)
		i=dsu_server_transform(gyro->z, server->coeffs.gy_deadzone, 0, server->coeffs.gy_threshold);
		*((float*)(sbuf+92)) = -(float)i / server->coeffs.gy_raw2dps;
		// Gyroscope roll (in deg/s)
		i=dsu_server_transform(gyro->y, server->coeffs.gy_deadzone, 0, server->coeffs.gy_threshold);
		*((float*)(sbuf+96)) = (float)i / server->coeffs.gy_raw2dps;
	}

	// send to all clients
	struct dsu_client *c,*c1;
	u64 tim = time(NULL);
	for(c = server->cli_first; c != NULL; ) {
		
		// check if we have a client timeout
		if (tim - c->timestamp > CLIENT_TIMEOUT) {
			// remove client and move to next
			if (server->cli_first == c) server->cli_first = c->next;
			if (server->cli_last == c) server->cli_last = c->next;
			if (c->prev) c->prev->next = c->next;
			if (c->next) c->next->prev = c->prev;
			c1 = c->next;
			free(c);
			c=c1;
			continue;
		}
			
		// check if this packet was already sent
		*((u32*)(sbuf+32)) = 0;
		if (memcmp(sbuf+20, c->oldpacket, 80)) {
			memcpy(c->oldpacket, sbuf+20, 80);
			// update packet number and send packet
			*((u32*)(sbuf+32)) = ++(c->packet_count); // packet number for this client
			dsu_send_packet(server, (struct sockaddr*)&c->addr, sbuf, 100);
//hex_dump(sbuf,100,"controler update");
		}
		c = c->next;
	}
	return 0;
}
