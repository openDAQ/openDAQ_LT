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

#include "streaming_packet.h"
#include "IP_WEBSOCKET.h"
#include "SEGGER_UTIL.h"
#include "mpack.h"
#include "streaming_signals.h"
#include <stdint.h>

#define METAINFORMATION_MSGPACK (2)

#define SIGNAL_NUMBER_MASK (0x000fffff)
#define SIGNAL_NUMBER_SHIFT (0)
#define TYPE_MASK (0x30000000)
#define TYPE_SHIFT (28)
#define SIZE_MASK (0x0ff00000)
#define SIZE_SHIFT (20)

static void build_packet_meta(tl_packet_t *packet, uint32_t signal_no, char *mpack_data, uint32_t mpack_size);
static int tl_serialize_packet(tl_packet_t *packet, unsigned char *dst, size_t buff_size);

/**
 * this function serializes the streaming transport layer header as well as the websocket header
 *
 * In a clean architecture both would be handled seperatly. However, when we serilize the streaming packet
 * we cannot tell how much pre-space we should leave in the destination buffer, because the size
 * of the websocket header varies, and the size of the streaming header varies as well. For the sake of usability
 * we allow ourselves this design choice, as it enables serializing consecutive packets in the same buffer.
 *
 * @param packet: the packet to serialize its header
 * @param dst: destination buffer
 * @param buff_size: size of the buffer in bytes
 * @return: <0     error: e.g. buffer too small
            else   number of bytes written
 */
static int serialize_header(tl_packet_t *packet, unsigned char *dst, size_t buff_size)
{
	const uint32_t signal_no = SIGNAL_NUMBER_MASK & (packet->signal_number << SIGNAL_NUMBER_SHIFT);
	const uint32_t packet_type = TYPE_MASK & (packet->packet_type << TYPE_SHIFT);
	const uint32_t size = SIZE_MASK & (packet->payload_size << SIZE_SHIFT);

	// first calculate the required header sizes
	// the streaming header size depends on the payload size
	size_t tl_header_size = (packet->payload_size) > UINT8_MAX ? 8 : 4;

#ifdef WEBSOCKET_STREAMING
	// the websocket header size depends on the size of its payload, which contains the streaming header
	size_t websocket_payload_size = tl_header_size + packet->payload_size;
	size_t websocket_header_size = websocket_payload_size < 126 ? 2 : 4;
	size_t header_size = websocket_header_size + tl_header_size;
#else
	size_t header_size = tl_header_size;
#endif

	if (buff_size < header_size) {
		// not enough space for the header
		return -1;
	}

#ifdef WEBSOCKET_STREAMING
	// serialize the websocket header
	*dst++ = 0x80 + IP_WEBSOCKET_FRAME_TYPE_BINARY; // FIN and binary packet
	if (websocket_payload_size < 126) {
		*dst++ = websocket_payload_size; // no mask bit set
	} else {
		*dst++ = 126; // no mask bit set
		*dst++ = websocket_payload_size >> 8;
		*dst++ = websocket_payload_size & 0xff;
	}
#endif

	// serialize the streaming header
	// if payload-size is >255 Bytes, size in header needs to be 0,
	// so that payload_size gets serialized as data_byte_count
	if (packet->payload_size > UINT8_MAX) {
		SEGGER_WrU32LE(dst, signal_no | packet_type | 0);
		SEGGER_WrU32LE(dst + 4, packet->payload_size);
	} else {
		SEGGER_WrU32LE(dst, signal_no | packet_type | size);
	}

	// return the number of bytes written
	return header_size;
}

int openDAQ_streaming_send_packet(const struct stream *stream, tl_packet_t *packet)
{
	char buff[packet->payload_size + 16]; // 16 bytes should be enough for all headers
	int packet_size = tl_serialize_packet(packet, (unsigned char *)buff, sizeof(buff));

	return packet_size < 0 ? packet_size : stream->stream(stream, buff, packet_size);
}

/**
 * copies from src to dst and swaps to little endian
 */
static inline void openDAQ_copy_sample_le(signal_data_type_e datatype, unsigned char *dst, const void *src)
{
	switch (datatype) {
	case signal_type_int8:
	case signal_type_uint8:
		*dst = *(const U8 *)src;
		break;
	case signal_type_int16:
	case signal_type_uint16:
		SEGGER_WrU16LE(dst, *(const U16 *)src);
		break;
	case signal_type_int32:
	case signal_type_uint32:
	case signal_type_real32:
		SEGGER_WrU32LE(dst, *(const U32 *)src);
		break;
	case signal_type_int64:
	case signal_type_uint64:
	case signal_type_real64:
		SEGGER_WrU64LE(dst, *(const U64 *)src);
		break;
	case signal_type_complex32:
		const U32 *ptr32 = (const U32 *)src;
		SEGGER_WrU32LE(dst, *ptr32++);
		SEGGER_WrU32LE(dst + sizeof(U32), *ptr32++);
		break;
	case signal_type_complex64:
		const U64 *ptr64 = (const U64 *)src;
		SEGGER_WrU64LE(dst, *ptr64++);
		SEGGER_WrU64LE(dst + sizeof(U64), *ptr64++);
		break;
	case signal_type_int128:
	case signal_type_uint128:
		// TODO
		break;
	default:
		break;
	}
}

static inline int openDAQ_get_sample_size(signal_data_type_e datatype)
{
	switch (datatype) {
	case signal_type_int8:
	case signal_type_uint8:
		return sizeof(U8);
	case signal_type_int16:
	case signal_type_uint16:
		return sizeof(U16);
	case signal_type_int32:
	case signal_type_uint32:
	case signal_type_real32:
		return sizeof(U32);
	case signal_type_int64:
	case signal_type_uint64:
	case signal_type_real64:
		return sizeof(U64);
	case signal_type_complex32:
		return sizeof(U32) * 2;
	case signal_type_complex64:
		return sizeof(U64) * 2;
	case signal_type_int128:
	case signal_type_uint128:
		return 16;
	}
	return 0;
}

/**
 * copies from src to dst and swaps to little endian. bytecount bytes are copied.
 */
static inline void memcpy_le(void *dst, const void *src, signal_data_type_e type, unsigned int bytecount)
{
	unsigned char *dst_ptr = dst;
	const unsigned char *src_ptr = src;
	size_t sample_size = openDAQ_get_sample_size(type);

	for (unsigned int i = 0; i < bytecount; i += sample_size) {
		// copy sample for sample and swap to little endian
		openDAQ_copy_sample_le(type, dst_ptr, src_ptr);
		src_ptr += sample_size;
		dst_ptr += sample_size;
	}
}

/**
 * serialize payload of data packets.
 * explicit signals contain arrays of datatype
 * implicit signals contain one uint64 and one sample of datatype
 */
static inline void tl_serialize_data_payload(unsigned char *dst, pl_data_t *data, size_t bytecount)
{
	signal_definition_t *def = data->signal_defintion;
	if (def->rule == signal_explicit_rule) {
		memcpy_le(dst, data->src, def->datatype, bytecount);
	} else {
		const uint64_t *ptr = (const uint64_t *)data->src;
		SEGGER_WrU64LE(dst, *ptr++);
		openDAQ_copy_sample_le(def->datatype, dst + sizeof(uint64_t), ptr);
	}
}

/**
 * serialize payload of meta packets.
 * first 4 bytes define the type of data encoding (msgpack)
 * rest contains the meta data
 */
static inline void tl_serialize_meta_payload(unsigned char *dst, pl_meta_t *meta, size_t bytecount)
{
	SEGGER_WrU32LE(dst, meta->meta_type);
	// this is messagepack, so memcpy will work fine
	memcpy(dst + 4, meta->meta_data, bytecount - 4);
}

/***
 * serializes the transport layer packet with header
 */
static int tl_serialize_packet(tl_packet_t *packet, unsigned char *dst, size_t buff_size)
{
	int header_len = serialize_header(packet, dst, buff_size);
	size_t payload_len = packet->payload_size;

	if (header_len < 0) {
		// some error when serializing the header
		return header_len;
	}

	// payload data larger than remaining buffer
	if (payload_len + header_len > buff_size) {
		return -1;
	}

	dst += header_len;
	switch (packet->packet_type) {
	case TYPE_DATA:
		tl_serialize_data_payload(dst, &packet->payload.data, payload_len);
		break;
	case TYPE_META:
		tl_serialize_meta_payload(dst, &packet->payload.meta, payload_len);
		break;
	default:
		return -2;
	}

	return header_len + payload_len;
}

void build_packet_meta_signal(tl_packet_t *packet, char *mpack_data, uint32_t mpack_size, uint32_t signal_num)
{
	build_packet_meta(packet, signal_num, mpack_data, mpack_size);
}
void build_packet_meta_stream(tl_packet_t *packet, char *mpack_data, uint32_t mpack_size)
{
	build_packet_meta(packet, 0, mpack_data, mpack_size);
}

/**
 * fill the packet structure for a meta packet
 */
static void build_packet_meta(tl_packet_t *packet, uint32_t signal_no, char *mpack_data, uint32_t mpack_size)
{
	packet->packet_type = TYPE_META;
	packet->signal_number = signal_no;
	packet->payload.meta.meta_data = mpack_data;
	packet->payload.meta.meta_type = METAINFORMATION_MSGPACK;
	// tl_payload_size is length of mpack payload + 4 Bytes meta_type
	packet->payload_size = mpack_size + 4;
}

/**
 * fill the packet structure for a data packet
 */
static void build_packet_data(tl_packet_t *packet, const char *data, signal_t *signal, size_t data_size)
{
	packet->packet_type = TYPE_DATA;
	packet->signal_number = signal_get_signal_no(signal);
	packet->payload.data.signal_defintion = signal->definition;
	packet->payload.data.src = data;
	packet->payload_size = data_size;
}

int openDAQ_streaming_serialize_constant_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal,
                                                const void *src)
{
	return openDAQ_streaming_serialize_implicit_signal(dst, dst_size, index, signal, src);
}
int openDAQ_streaming_serialize_linear_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal,
                                              const void *src)
{
	return openDAQ_streaming_serialize_implicit_signal(dst, dst_size, index, signal, src);
}

int openDAQ_streaming_serialize_implicit_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal,
                                                const void *src)
{
	tl_packet_t packet = {0};
	size_t sample_size = openDAQ_get_sample_size(signal->definition->datatype);

	uint64_t buf[3]; // large enough buffer to hold everything
	memcpy(&buf[0], &index, sizeof(index));
	memcpy(&buf[1], src, sample_size);

	build_packet_data(&packet, (const char *)buf, signal, sample_size + sizeof(uint64_t));
	return tl_serialize_packet(&packet, dst, dst_size);
}

int openDAQ_streaming_serialize_explicit_signal(void *dst, size_t dst_size, signal_t *signal, const void *src,
                                                unsigned int num)
{
	tl_packet_t packet = {0};
	size_t sample_size = openDAQ_get_sample_size(signal->definition->datatype);
	build_packet_data(&packet, src, signal, sample_size * num);
	return tl_serialize_packet(&packet, dst, dst_size);
}