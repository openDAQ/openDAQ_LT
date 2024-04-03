#ifndef _STREAMING_HANDLER_H
#define _STREAMING_HANDLER_H

#include "stream_id.h"
#include "streaming_config.h"
#include "streaming_signals.h"

typedef uint64_t subscribe_callback(const struct stream *stream, signal_t *signal);
typedef void unsubscribe_callback(const struct stream *stream, signal_t *signal);
typedef void connect_callback(const struct stream *stream);

struct streaming_callbacks {
	connect_callback *on_connect;
	subscribe_callback *on_subscribe;
	unsubscribe_callback *on_unsubscribe;
};

void streaming_init(struct streaming_callbacks *streaming_cb);
void streaming_start(void);

int streaming_send_avail(const struct stream *stream, signal_t **signals, int num_signals);
int streaming_send_unavail(const struct stream *stream, signal_t **signals, int num_signals);
int streaming_send_subscribed(const struct stream *stream, signal_t *signal);
int streaming_send_unsubscribed(const struct stream *stream, signal_t *signal);
int streaming_send_meta_stream(struct stream *stream);
int streaming_send_meta_signal(const struct stream *stream, signal_t *signal, uint64_t valueIndex);

#endif
