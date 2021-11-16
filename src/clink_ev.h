#ifndef CAMEO_EVENT_H
#define CAMEO_EVENT_H

#define MAX_PAYLOAD_LEN		128
#define MAX_EVENT_LEN		sizeof(Event_t) + sizeof(struct msghdr)

struct Event
{
	unsigned int id;		// the event id
	unsigned int length;		// the length of payload
	char payload[MAX_PAYLOAD_LEN];	// the payload for extra information
};

typedef struct Event Event_t;
typedef Event_t* Event_ptr_t;

enum EVENT_LIST {
	RESERVE         = 0,
	UPDATE_WIFI_LED,
	LINK_WAN_ST,
	RX_WAKE_UP,
	UPDATE_WAN_STATUS,


	MAX_APP_EVENT   = 64,
};

#endif
