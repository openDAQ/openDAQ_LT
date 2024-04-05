/*
 * Copyright (C) 2021 HBK – Hottinger Brüel & Kjær
 * Skodsborgvej 307
 * DK-2850 Nærum
 * Denmark
 * http://www.hbkworld.com
 * All rights reserved
 *
 * The copyright to the computer program(s) herein is the property of
 * HBK – Hottinger Brüel & Kjær (HBK), Denmark. The program(s)
 * may be used and/or copied only with the written permission of HBM
 * or in accordance with the terms and conditions stipulated in the
 * agreement/contract under which the program(s) have been supplied.
 * This copyright notice must not be removed.
 *
 * This Software is licenced by the
 * "General supply and license conditions for software"
 * which is part of the standard terms and conditions of sale from HBM.
 */

#ifndef _STREAMING_CONFIG_H_
#define _STREAMING_CONFIG_H_

#ifndef JSONRPC_PORT
	#define JSONRPC_PORT "http"
#endif

#ifndef JSONRPC_HTTPVERSION
	#define JSONRPC_HTTPVERSION "1.1"
#endif

#ifndef JSONRPC_METHOD
	#define JSONRPC_METHOD "POST"
#endif

#ifndef JSONRPC_PATH
	#define JSONRPC_PATH "/streaming_jsonrpc"
#endif

#ifndef JSONRPC_BUF_SIZE
	#define JSONRPC_BUF_SIZE 256
#endif

#ifndef MSGPACK_BUF_SIZE
	#define MSGPACK_BUF_SIZE 256
#endif

#ifndef STREAMING_MAX_SIGNALS
	#define STREAMING_MAX_SIGNALS 12
#endif

#ifndef STREAMING_MAX_TABLES
	#define STREAMING_MAX_TABLES 4
#endif

#ifndef STREAMING_SIGNAL_NAME_LENGTH
	#define STREAMING_SIGNAL_NAME_LENGTH 32
#endif

#ifndef STREAMING_INCLUDE_CONFIG_CHANNEL
	#define STREAMING_INCLUDE_CONFIG_CHANNEL 1
#endif

#ifndef STREAMING_WEBSOCKET_URI
	#define STREAMING_WEBSOCKET_URI "/stream"
#endif

#ifndef STREAMING_TCP_PORT
	#define STREAMING_TCP_PORT 7412
#endif

#endif