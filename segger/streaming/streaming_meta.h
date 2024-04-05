#ifndef _STREAMING_META_H
#define _STREAMING_META_H

#include "streaming_packet.h"
#include "streaming_signals.h"

// Signal Related Meta-Messages
int build_mpack_meta_signal_subscribed(char *dst, int size, const char *id);
int build_mpack_meta_signal_unsubscribed(char *dst, int size);
int build_mpack_meta_signal(char *dst, int size, signal_t *meta, uint64_t valueIndex);

// Stream related Meta-Messages
int build_mpack_meta_stream_avail(char *dst, int size, signal_t **signals, int num_signals);
int build_mpack_meta_stream_unavail(char *dst, int size, signal_t **signals, int num_signals);
int build_mpack_meta_stream_version(char *dst, int size);
int build_mpack_meta_stream_init(char *dst, int size, const char *id);

#define MPACK_KEY_METHOD "method"
#define MPACK_KEY_PARAMS "params"

// stream related meta information
#define META_METHOD_APIVERSION "apiVersion"
#define VERSION "version"
#define STREAMING_VERSION "1.0.1"
#define META_METHOD_INIT "init"
#define META_METHOD_AVAILABLE "available"
#define META_METHOD_UNAVAILABLE "unavailable"

// signal related meta information
#define META_METHOD_ALIVE "alive"

#define META_STREAMID "streamId"

#define META_SIGNALID "signalId"

#define META_SIGNALIDS "signalIds"

#define META_TABLEID "tableId"
#define META_RELATEDSIGNALS "relatedSignals"

#define META_METHOD_SUBSCRIBE "subscribe"
#define META_METHOD_UNSUBSCRIBE "unsubscribe"

#define META_DATATYPE "dataType"

#define META_RULE "rule"
#define META_RULETYPE_EXPLICIT "explicit"
#define META_RULETYPE_LINEAR "linear"
#define META_RULETYPE_CONSTANT "constant"
#define META_NAME "name"

#define META_FILLLEVEL "fillLevel"
#define META_START "start"
#define META_DELTA "delta"

#endif
