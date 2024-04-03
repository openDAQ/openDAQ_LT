#ifndef _STREAMING_PACKET_H_
#define _STREAMING_PACKET_H_

#include "stream_id.h"
#include "streaming_signals.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Packet can either deliver DATA or METAINFORMATION
typedef enum { TYPE_DATA = 1, TYPE_META = 2 } type_t;

// presentation layer can either be data or metainformation
// metainformation is struct, data is just char*
typedef uint32_t meta_type_t;

typedef struct {
	meta_type_t meta_type;
	char *meta_data;
} pl_meta_t;

typedef struct {
	const void *src;
	signal_definition_t *signal_defintion;
} pl_data_t;

typedef union {
	pl_data_t data;
	pl_meta_t meta;
} tl_payload_t;

typedef struct {
	type_t packet_type;
	uint32_t signal_number;
	uint32_t payload_size;
	tl_payload_t payload;
} tl_packet_t;

// packet building functions used internally
void build_packet_meta_stream(tl_packet_t *packet, char *mpack_data, uint32_t mpack_size);
void build_packet_meta_signal(tl_packet_t *packet, char *mpack_data, uint32_t mpack_size, uint32_t signal_no);

/**
 * sends a packet through the stream. The packet is firsted serialized into a buffer on the stack
 * packets are generated with build_packet_meta_stream or build_packet_meta_signal
 *
 * @param stream the stream to send the packet through
 * @param packet the packet to send
 *
 * @return <0    error
 *         else  number of bytes written
 */
int openDAQ_streaming_send_packet(const struct stream *stream, tl_packet_t *packet);

/**
 * serializes an explicit signal into a buffer.
 *
 * @param dst pointer to destination buffer
 * @param dst_size size in bytes of destination buffer
 * @param signal pointer to the signal to serialize
 * @param src pointer to the payload data
 * @param number of signal samples to at src to serialize
 *
 * @return <0    error
 *         else  number of bytes written
 */
int openDAQ_streaming_serialize_explicit_signal(void *dst, size_t dst_size, signal_t *signal, const void *src,
                                                unsigned int num);

/**
 * serializes a constant signal into a buffer.
 *
 * @param dst pointer to destination buffer
 * @param dst_size size in bytes of destination buffer
 * @param the index of the sample to transmit
 * @param signal pointer to the signal to serialize
 * @param src pointer to the payload data
 *
 * @return <0    error
 *         else  number of bytes written
 */
int openDAQ_streaming_serialize_constant_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal,
                                                const void *src);

/**
 * serializes a linear signal into a buffer.
 *
 * @param dst pointer to destination buffer
 * @param dst_size size in bytes of destination buffer
 * @param the index of the sample to transmit
 * @param signal pointer to the signal to serialize
 * @param src pointer to the payload data
 *
 * @return <0    error
 *         else  number of bytes written
 */
int openDAQ_streaming_serialize_linear_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal,
                                              const void *src);

/**
 * serializes an implicit signal into a buffer.
 *
 * @param dst pointer to destination buffer
 * @param dst_size size in bytes of destination buffer
 * @param the index of the sample to transmit
 * @param signal pointer to the signal to serialize
 * @param src pointer to the payload data
 *
 * @return <0    error
 *         else  number of bytes written
 */
int openDAQ_streaming_serialize_implicit_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal,
                                                const void *src);

#endif
