/*
https://v1993.github.io/cemuhook-protocol/ 
https://github.com/yuk27/BetterJoyForDolphin/blob/master/BetterJoyForDolphin/UpdServer.cs
*/

#define DSU_ERRMSG_LENGTH 128

struct dsu_coeffs {
	int cp_deadzone;	// C-Pad
	int cp_max;
	int cp_threshold;	// threshold is used to reduce network traffic. Only values that changed more than threshhold since last sent are transferred to the server
	int cs_deadzone;	// C-Stick
	int cs_max;
	int cs_threshold;
	int acc_deadzone;	// Accelerometer
	int acc_threshold;
	int gy_deadzone;	// Gyro
	int gy_threshold;
	float gy_raw2dps;
};

struct dsu_client {
	u64 timestamp;
	u32 id;
	struct sockaddr_in addr;
	struct dsu_client *next;
	struct dsu_client *prev;
	u32 packet_count;
	u8 oldpacket[80];
};

struct dsu_server {
	int socket;
	char ipstr[INET_ADDRSTRLEN];
	int port;
	char lasterrmsg[DSU_ERRMSG_LENGTH];
	int lasterrno;
	struct dsu_client *cli_first;
	struct dsu_client *cli_last;
	struct dsu_coeffs coeffs;
	u32 button_meta; // KEY_SELECT per default
	u32 button_plus; // KEY_DUP per default
	u32 button_minus; // KEY_DDOWN per default
	u32 button_LSTCK_PUSH; // KEY_DLEFT per default
	u32 button_RSTCK_PUSH; // KEY_DRIGHT per default
};

extern int dsu_server_init(struct dsu_server *server, int port);
extern int dsu_server_shutdown(struct dsu_server *server);
extern int dsu_server_run(struct dsu_server *server);
extern int dsu_server_update(	// all parameters can be NULL except server
	struct dsu_server *server,
	u32 buttons,
	circlePosition *posCp,
	circlePosition *posStk,
	touchPosition *touch,
	accelVector *accel,
	angularRate *gyro);
