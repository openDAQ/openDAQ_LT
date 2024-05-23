#ifndef _STREAMING_SIGNALS_H_
#define _STREAMING_SIGNALS_H_

#include "stream_id.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
	// ISO8601 format YYY-MM-DDThh:mm:ss.f, e.g. 1970-01-01 for UNIX Epoch, optional
	const char *epoch;
	uint8_t num_of_exponents;
	uint8_t time_family_exp[];
} time_object_t;

typedef struct {
        double low;
        double high;
} range_object_t;

typedef struct {
        double offset;
        double scale;
} postScaling_object_t;

typedef enum {
	signal_linear_rule,
	signal_constant_rule,
	signal_explicit_rule,
} signal_rule_e;

typedef enum {
	signal_type_value,
	signal_type_time,
	signal_type_status,
} signal_type_e;

typedef enum {
	signal_type_int8,
	signal_type_uint8,
	signal_type_int16,
	signal_type_uint16,
	signal_type_int32,
	signal_type_uint32,
	signal_type_int64,
	signal_type_uint64,
	signal_type_int128,
	signal_type_uint128,
	signal_type_real32,
	signal_type_real64,
	signal_type_complex32,
	signal_type_complex64,
} signal_data_type_e;

typedef struct {
	const char *name;
	signal_rule_e rule;
	signal_data_type_e datatype;
	signal_type_e signaltype;
	bool hidden;
	uint64_t delta;
	const time_object_t *time;
        const range_object_t *range;
        const postScaling_object_t *postScaling;
} signal_definition_t;

typedef struct signal_table_t signal_table_t;

typedef struct signal_t {
	bool available;
	const struct stream *stream;
	struct signal_table_t *table;
	signal_definition_t *definition;
	bool subscribed;
} signal_t;

struct signal_table_t {
	unsigned int signal_counter;
	const char *tableId;
	struct signal_t *signals;
	unsigned int subscribed_value_signal_count;
};

void signals_init(void);
void signals_send_all_avail(const struct stream *stream);
int signals_subscribe(const struct stream *stream, const char *signalId);
int signals_unsubscribe(const struct stream *stream, const char *signalId);
signal_t *signals_add_signal(signal_definition_t *def, signal_table_t *table);
signal_table_t *signals_add_table(signal_definition_t *def, unsigned int count, const char *table_name);
bool signal_has_subscription(signal_t *signal);
unsigned int signal_get_signal_no(signal_t *signal);
void signals_purge_stream(const struct stream *stream);

#endif
