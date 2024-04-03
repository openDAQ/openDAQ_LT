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

#include "streaming_websocket_rx.h"
#ifdef WEBSOCKET_STREAMING
	#include "IP_WEBSOCKET.h"
#endif
#include <stdbool.h>
#include <stdint.h>

static void close_delayed(const void *handle)
{
	closesocket((int)handle);
}

static void remove_cb(IP_EXEC_DELAYED *delayed, void *handle)
{
	(void)delayed;
	(void)handle;
}

static IP_EXEC_DELAYED exec_delayed;
int streaming_rx_callback(long Socket, IP_PACKET *pPacket, int code)
{
	if (code < 0) {
		goto CloseSocket;
	}

#ifdef WEBSOCKET_STREAMING
	unsigned char *ptr = pPacket->pData;
	size_t paket_len = pPacket->NumBytes;
	U16 head = (ptr[0] << 8) + ptr[1];
	ptr += 2;
	unsigned int expected_len = 2;

	uint64_t payload_len = head & 0x7F;
	uint8_t masking_key[4];
	U8 opcode = (head >> 8) & 0xf;
	bool masked = (head >> 7) & 1;
	U8 rsvd = (head >> 8) & 0x70;

	if (payload_len == 126) {
		payload_len = (ptr[0] << 8) + ptr[1];
		ptr += 2;
		expected_len += 2;
	} else if (payload_len == 127) {
		memcpy(&payload_len, ptr, 8);
		ptr += 8;
		expected_len += 8;
		payload_len = __builtin_bswap64(payload_len);
	}

	if (masked == 0 || rsvd != 0) {
		// receiving unmasked packets or reserved bit is an error and we must fail the websocket connection
		goto CloseSocket;
	} else {
		memcpy(&masking_key, ptr, sizeof(masking_key));
		ptr += sizeof(masking_key);
		expected_len += sizeof(masking_key);
	}

	// special cases our websocket implementation cannot handle: fragmentation
	// 1. We want to have the whole websocket header in the packet
	// 2. We want to have the whole websocket frame in the packet, not more or less data
	//    otherwise the parsing of the next packet will become complicated!
	// be aware that control frames are never fragmented on websocket level!
	if (paket_len < expected_len || paket_len != expected_len + payload_len) {
		pPacket->pData[0] = (1 << 7) + IP_WEBSOCKET_FRAME_TYPE_CLOSE;
		pPacket->pData[1] = 7; // close code + "sorry"
		pPacket->pData[2] = (IP_WEBSOCKET_CLOSE_CODE_ABNORMAL_CLOSURE >> 8) & 0xff;
		pPacket->pData[3] = IP_WEBSOCKET_CLOSE_CODE_ABNORMAL_CLOSURE & 0xff;
		memcpy(&pPacket->pData[4], "sorry", 5);
		pPacket->NumBytes = 9; // websocket header + payload
		IP_TCP_SendAndFree(Socket, pPacket);
		setsockopt(Socket, SOL_SOCKET, SO_CALLBACK, NULL, 0);
		IP_ExecDelayed(&exec_delayed, close_delayed, (void *)Socket, NULL, remove_cb);
		return IP_OK_KEEP_PACKET;
	}

	// text and binary frames are simply ignored
	// CONTINUE frames are binary or text frames fragmented on websocket level
	// we dont do any plausibility and format checks beyond the fragementation checks earlier.
	if (opcode == IP_WEBSOCKET_FRAME_TYPE_CONTINUE || opcode == IP_WEBSOCKET_FRAME_TYPE_TEXT ||
	    opcode == IP_WEBSOCKET_FRAME_TYPE_BINARY) {
		return IP_OK;
	}

	// do the unmasking late, only if we really have to
	for (size_t i = 0; i < paket_len - (ptr - pPacket->pData); i++) {
		ptr[i - 4] = ptr[i] ^ masking_key[i % sizeof(masking_key)];
	}

	// a ping frame is directly answered by sending the packet back as a pong
	if (opcode == IP_WEBSOCKET_FRAME_TYPE_PING) {
		pPacket->pData[0] = (1 << 7) /* FIN */ + IP_WEBSOCKET_FRAME_TYPE_PONG;
		pPacket->pData[1] &= 0x7F; // clear mask bit, keep payload length
		pPacket->NumBytes -= ptr - pPacket->pData - sizeof(head);
		IP_TCP_SendAndFree(Socket, pPacket);
		return IP_OK_KEEP_PACKET;
	}

	// close frames are handeled similar to ping frames
	if (opcode == IP_WEBSOCKET_FRAME_TYPE_CLOSE) {
		pPacket->pData[0] = (1 << 7) /* FIN */ + IP_WEBSOCKET_FRAME_TYPE_CLOSE;
		pPacket->pData[1] &= 0x7F; // clear mask bit, keep payload length
		pPacket->NumBytes -= ptr - pPacket->pData - sizeof(head);
		IP_TCP_SendAndFree(Socket, pPacket);
		setsockopt(Socket, SOL_SOCKET, SO_CALLBACK, NULL, 0);
		IP_ExecDelayed(&exec_delayed, close_delayed, (void *)Socket, NULL, remove_cb);
		return IP_OK_KEEP_PACKET;
	}
#else
	return IP_OK;
#endif

	// fallthrough for unknown opcodes. connection will be closed
CloseSocket:
	setsockopt(Socket, SOL_SOCKET, SO_CALLBACK, NULL, 0);
	IP_ExecDelayed(&exec_delayed, close_delayed, (void *)Socket, NULL, remove_cb);
	return IP_OK;
}