#ifndef _STREAMING_WEBSOCKET_RX_H_
#define _STREAMING_WEBSOCKET_RX_H_

#include "IP.h"

int streaming_rx_callback(long Socket, IP_PACKET *pPacket, int code);

#endif
