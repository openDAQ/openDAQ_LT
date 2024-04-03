/*
 * Copyright (C) 2023 openDAQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "streaming_handler.h"
#include "IP.h"
#include "IP_Webserver.h"
#include "IP_WEBSOCKET.h"
#include "libs/mjson/src/mjson.h"
#include "stream_id.h"
#include "streaming_jsonrpc.h"
#include "streaming_meta.h"
#include "streaming_packet.h"
#include "streaming_signals.h"
#include "streaming_websocket_rx.h"
#include <stdio.h>

struct streaming_callbacks *streaming_cbs;
static char stream_id[9];

#if WEBSOCKET_STREAMING
#define IP_WEBSOCKET_CLOSE_CODE_TRY_AGAIN_LATER 1013

static int websocket_acceptKey_generator(WEBS_OUTPUT *pOutput, void *pSecWebSocketKey, int SecWebSocketKeyLen,
                                          void *pBuffer, int BufferSize)
{
	WEBS_USE_PARA(pOutput);
	return IP_WEBSOCKET_GenerateAcceptKey(pSecWebSocketKey, SecWebSocketKeyLen, pBuffer, BufferSize);
}

static void streaming_dispatch_handle(WEBS_OUTPUT *pOutput, void *pConnection)
{
	WEBS_USE_PARA(pOutput);
	long handle = (long)pConnection;
	if (OS_MAILBOX_Put(&mb, &handle)) {
		// there is already one streaming connection
		// close this one gracefully
		const char packet[] = {
		    0x80 + IP_WEBSOCKET_FRAME_TYPE_CLOSE, // fin and close
		    2,                                    // size of payload (error code)
		    IP_WEBSOCKET_CLOSE_CODE_TRY_AGAIN_LATER >> 8,
		    IP_WEBSOCKET_CLOSE_CODE_TRY_AGAIN_LATER & 0xff,
		};
		send(handle, packet, sizeof(packet), 0);
		closesocket(handle);
	}
}

static OS_MAILBOX mb; // Mailbox to hand over connection handle from webserver task to streaming task
static long buff;
static webSocketHook;
static const IP_WEBS_WEBSOCKET_API StreamingWebSocketApi = {websocket_acceptKey_generator,
                                                            streaming_dispatch_handle};
#endif

void streaming_init(struct streaming_callbacks *streaming_cb)
{
	signals_init();
	snprintf(stream_id, sizeof(stream_id), "%08X", (rand() << 16) + rand());
#if STREAMING_INCLUDE_CONFIG_CHANNEL
	streaming_jsonrpc_init(stream_id);
#endif
	streaming_cbs = streaming_cb;
#if WEBSOCKET_STREAMING
	OS_MAILBOX_Create(&mb, sizeof(buff), 1, &buff);
	IP_WEBS_WEBSOCKET_AddHook(&webSocketHook, &StreamingWebSocketApi, STREAMING_WEBSOCKET_URI, "");
#endif
}

void streaming_start(void)
{
	long handle;

#if WEBSOCKET_STREAMING
	// nothing to do here
#else
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr = {
	    .sin_family = AF_INET,
	    .sin_port = htons(STREAMING_PORT),
	    .sin_addr.s_addr = htonl(ADDR_ANY),
	};
	bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	listen(sock, 1);
#endif

	while (true) {
#if WEBSOCKET_STREAMING
		long *ptr;
		OS_MAILBOX_GetPtrBlocked(&mb, (void **)&ptr);
		handle = *ptr;
#else
		handle = accept(sock, NULL, 0);
#endif

		setsockopt(handle, SOL_SOCKET, SO_CALLBACK, (void *)streaming_rx_callback, 0);
		struct stream *stream = stream_malloc(handle, stream_id);
		streaming_send_meta_stream(stream);
		signals_send_all_avail(stream);
		if (streaming_cbs->on_connect != NULL)
			streaming_cbs->on_connect(stream);

		// the next line will through a WARNING before we leave the loop
		// there is no easier way to check the socket for errors
		while (!IP_SOCKET_GetErrorCode(handle)) {
			OS_Delay(10);
		}

		// Error might indicate we ran out of network buffers or the socket is closed
		signals_purge_stream(stream);
		stream_free(stream);
#ifdef WEBSOCKET_STREAMING
		OS_MAILBOX_Purge(&mb);
#endif
	}

	OS_TASK_Terminate(NULL);
}

int streaming_send_avail(const struct stream *stream, signal_t **signalz, int num_signals)
{
	tl_packet_t packet = {0};
	char mpack_buff[MSGPACK_BUF_SIZE];
	int mpack_size = build_mpack_meta_stream_avail(mpack_buff, sizeof(mpack_buff), signalz, num_signals);
	build_packet_meta_stream(&packet, mpack_buff, mpack_size);
	return openDAQ_streaming_send_packet(stream, &packet);
}

int streaming_send_subscribed(const struct stream *stream, signal_t *signal)
{
	tl_packet_t packet = {0};
	char mpack_buff[MSGPACK_BUF_SIZE];
	int mpack_size = build_mpack_meta_signal_subscribed(mpack_buff, sizeof(mpack_buff), signal->definition->name);
	build_packet_meta_signal(&packet, mpack_buff, mpack_size, signal_get_signal_no(signal));
	return openDAQ_streaming_send_packet(stream, &packet);
}

int streaming_send_unsubscribed(const struct stream *stream, signal_t *signal)
{
	tl_packet_t packet = {0};
	char mpack_buff[MSGPACK_BUF_SIZE];
	int mpack_size = build_mpack_meta_signal_unsubscribed(mpack_buff, sizeof(mpack_buff));
	build_packet_meta_signal(&packet, mpack_buff, mpack_size, signal_get_signal_no(signal));
	return openDAQ_streaming_send_packet(stream, &packet);
}

int streaming_send_meta_signal(const struct stream *stream, signal_t *signal, uint64_t valueIndex)
{
	tl_packet_t packet = {0};
	char mpack_buff[MSGPACK_BUF_SIZE];
	int mpack_size = build_mpack_meta_signal(mpack_buff, sizeof(mpack_buff), signal, valueIndex);
	build_packet_meta_signal(&packet, mpack_buff, mpack_size, signal_get_signal_no(signal));
	return openDAQ_streaming_send_packet(stream, &packet);
}

int streaming_send_meta_stream(struct stream *stream)
{
	tl_packet_t packet = {0};
	char mpack_buff[MSGPACK_BUF_SIZE];
	int mpack_size = build_mpack_meta_stream_version(mpack_buff, sizeof(mpack_buff));
	build_packet_meta_stream(&packet, mpack_buff, mpack_size);
	int ret = openDAQ_streaming_send_packet(stream, &packet);
	if (ret < 0) {
		return ret;
	}

	mpack_size = build_mpack_meta_stream_init(mpack_buff, sizeof(mpack_buff), stream->id);
	build_packet_meta_stream(&packet, mpack_buff, mpack_size);
	return openDAQ_streaming_send_packet(stream, &packet);
}
