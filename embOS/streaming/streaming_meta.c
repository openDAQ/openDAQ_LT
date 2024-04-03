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

#include "streaming_meta.h"
#include "mpack.h"
#include "streaming_config.h"
#include "streaming_packet.h"
#include "streaming_signals.h"

static int mpack_write_finally(mpack_writer_t *w)
{
	int bytes_written = mpack_writer_buffer_used(w);
	if (mpack_writer_destroy(w) != mpack_ok) {
		return -1;
	}
	return bytes_written;
}

static const char *signal_rule_to_string(signal_rule_e rule)
{
	switch (rule) {
	case signal_constant_rule:
		return "constant";
	case signal_linear_rule:
		return "linear";
	case signal_explicit_rule:
		return "explicit";
	}
	return "unknown";
}

static const char *signal_type_to_string(signal_type_e type)
{
	switch (type) {
	case signal_type_time:
		return "time";
	case signal_type_status:
		return "status";
	case signal_type_value:
		return "value";
	}
	return "unknown";
}

static const char *signal_data_type_to_string(signal_data_type_e datatype)
{
	switch (datatype) {
	case signal_type_int8:
		return "int8";
	case signal_type_uint8:
		return "uint8";
	case signal_type_int16:
		return "int16";
	case signal_type_uint16:
		return "uint16";
	case signal_type_int32:
		return "int32";
	case signal_type_uint32:
		return "uint32";
	case signal_type_int64:
		return "int64";
	case signal_type_uint64:
		return "uint64";
	case signal_type_int128:
		return "int128";
	case signal_type_uint128:
		return "uint128";
	case signal_type_real32:
		return "real32";
	case signal_type_real64:
		return "real64";
	case signal_type_complex32:
		return "complex32";
	case signal_type_complex64:
		return "complex64";
	}
	return "unknown";
}

static void build_mpack_meta_signal_time(mpack_writer_t *w, const time_object_t *time)
{
	mpack_write_cstr(w, "time");
	mpack_start_map(w, time->epoch ? 2 : 1);
	mpack_write_cstr(w, "timeFamily");
	mpack_start_array(w, time->num_of_exponents);
	for (uint8_t i = 0; i < time->num_of_exponents; i++) {
		mpack_write_u8(w, time->time_family_exp[i]);
	}
	mpack_finish_array(w);
	if (time->epoch) {
		mpack_write_cstr(w, "epoch");
		mpack_write_cstr(w, time->epoch);
	}
	mpack_finish_map(w);
}

static void build_mpack_meta_signal_time_opendaq(mpack_writer_t *w, const time_object_t *time)
{
	unsigned int primes[] = {2, 3, 5, 7, 11, 13, 17, 19};
	uint32_t p = 1;
	for (uint8_t i = 0; i < time->num_of_exponents; i++) {
		for (uint8_t j = 0; j < time->time_family_exp[i]; j++) {
			p *= primes[i];
		}
	}
	mpack_write_cstr(w, "resolution");
	mpack_start_map(w, 2);
	mpack_write_cstr(w, "num");
	mpack_write_u64(w, 1ULL);
	mpack_write_cstr(w, "denom");
	mpack_write_u64(w, (uint64_t)p);
	mpack_finish_map(w);
	mpack_write_cstr(w, "absoluteReference");
	mpack_write_cstr_or_nil(w, time->epoch);
	mpack_write_cstr(w, "unit");
	mpack_start_map(w, 3);
	mpack_write_cstr(w, "displayName");
	mpack_write_cstr(w, "s");
	mpack_write_cstr(w, "unitId");
	mpack_write_int(w, 5457219); // seconds
	mpack_write_cstr(w, "quantity");
	mpack_write_cstr(w, "time");
	mpack_finish_map(w);
}

static void build_mpack_meta_signal_definition(mpack_writer_t *w, signal_definition_t *def)
{
	// at least name, ruleType and dataType have to be present
	uint8_t definition_map_elements = 3;
	bool is_linear_rule = def->rule == signal_linear_rule;
	bool is_time_signal = def->time != NULL;

	if (is_time_signal) {
		definition_map_elements += 3; // only with build_mpack_meta_signal_time_opendaq
		// definition_map_elements++; // only with build_mpack_meta_signal_time
	}
	if (is_linear_rule) {
		definition_map_elements++;
	}

	mpack_start_map(w, definition_map_elements);
	mpack_write_cstr(w, "name");
	mpack_write_cstr(w, def->name);
	mpack_write_cstr(w, "rule");
	mpack_write_cstr(w, signal_rule_to_string(def->rule));
	mpack_write_cstr(w, "dataType");
	mpack_write_cstr(w, signal_data_type_to_string(def->datatype));
	if (is_linear_rule) {
		mpack_write_cstr(w, "linear");
		mpack_start_map(w, 1);
		mpack_write_cstr(w, "delta");
		mpack_write_u64(w, def->delta);
		mpack_finish_map(w);
	}
	if (is_time_signal) {
		build_mpack_meta_signal_time_opendaq(w, def->time);
		// build_mpack_meta_signal_time(w, def->time);
	}
	mpack_finish_map(w);
}

static int count_related_signals(signal_t *signal)
{
	int count = 0;
	for (unsigned int i = 0; i < signal->table->signal_counter; i++) {
		signal_t *sig = &signal->table->signals[i];
		if (sig->definition->signaltype != signal_type_value && signal->definition->signaltype == signal_type_value) {
			count++;
		}
	}
	return count;
}

int build_mpack_meta_signal(char *dst, int size, signal_t *signal, uint64_t valueIndex)
{
	mpack_writer_t writer;
	mpack_writer_init(&writer, dst, size);
	mpack_start_map(&writer, 2);
	mpack_write_cstr(&writer, MPACK_KEY_METHOD);
	mpack_write_cstr(&writer, "signal");
	mpack_write_cstr(&writer, MPACK_KEY_PARAMS);
	mpack_start_map(&writer, valueIndex == 0 ? 3 : 4);
	mpack_write_cstr(&writer, "tableId");
	mpack_write_cstr(&writer, signal->table->tableId);
	if (valueIndex != 0) {
		mpack_write_cstr(&writer, "valueIndex");
		mpack_write_u64(&writer, valueIndex);
	}
	mpack_write_cstr(&writer, "relatedSignals");
	mpack_start_array(&writer, count_related_signals(signal));
	for (unsigned int i = 0; i < signal->table->signal_counter; i++) {
		signal_t *sig = &signal->table->signals[i];
		if (sig->definition->signaltype != signal_type_value && signal->definition->signaltype == signal_type_value) {
			mpack_start_map(&writer, 2);
			mpack_write_cstr(&writer, "type");
			mpack_write_cstr(&writer, signal_type_to_string(sig->definition->signaltype));
			mpack_write_cstr(&writer, "signalId");
			mpack_write_cstr(&writer, sig->definition->name);
			mpack_finish_map(&writer);
		}
	}
	mpack_finish_array(&writer);
	mpack_write_cstr(&writer, "definition");
	build_mpack_meta_signal_definition(&writer, signal->definition);
	mpack_finish_map(&writer);
	mpack_finish_map(&writer);
	return mpack_write_finally(&writer);
}

int build_mpack_meta_stream_init(char *dst, int size, const char *id)
{
	mpack_writer_t writer;
	mpack_writer_init(&writer, dst, size);
	mpack_start_map(&writer, 2);
	mpack_write_cstr(&writer, MPACK_KEY_METHOD);
	mpack_write_cstr(&writer, META_METHOD_INIT);

	mpack_write_cstr(&writer, MPACK_KEY_PARAMS);
	mpack_start_map(&writer, 3);

	mpack_write_cstr(&writer, "streamId");
	mpack_write_cstr(&writer, id);

	mpack_write_cstr(&writer, "supported");
	mpack_start_map(&writer, 0);
	mpack_finish_map(&writer);

	mpack_write_cstr(&writer, "commandInterfaces");
#if STREAMING_INCLUDE_CONFIG_CHANNEL
	mpack_start_map(&writer, 1);
	mpack_write_cstr(&writer, "jsonrpc-http");
	mpack_start_map(&writer, 5);
	// Command Interface
	mpack_write_cstr(&writer, "port");
	mpack_write_cstr(&writer, JSONRPC_PORT);
	mpack_write_cstr(&writer, META_METHOD_APIVERSION);
	mpack_write_i8(&writer, 1);
	mpack_write_cstr(&writer, "httpMethod");
	mpack_write_cstr(&writer, JSONRPC_METHOD);
	mpack_write_cstr(&writer, "httpVersion");
	mpack_write_cstr(&writer, JSONRPC_HTTPVERSION);
	mpack_write_cstr(&writer, "httpPath");
	mpack_write_cstr(&writer, JSONRPC_PATH);
	mpack_finish_map(&writer);
#else
	mpack_start_map(&writer, 0);
#endif
	mpack_finish_map(&writer);
	mpack_finish_map(&writer);
	mpack_finish_map(&writer);
	return mpack_write_finally(&writer);
}

int build_mpack_meta_stream_version(char *dst, int size)
{
	mpack_writer_t writer;
	mpack_writer_init(&writer, dst, size);
	mpack_start_map(&writer, 2);
	mpack_write_cstr(&writer, MPACK_KEY_METHOD);
	mpack_write_cstr(&writer, META_METHOD_APIVERSION);
	mpack_write_cstr(&writer, MPACK_KEY_PARAMS);
	mpack_start_map(&writer, 1);
	mpack_write_cstr(&writer, VERSION);
	mpack_write_cstr(&writer, STREAMING_VERSION);
	mpack_finish_map(&writer);
	mpack_finish_map(&writer);
	return mpack_write_finally(&writer);
}

int build_mpack_meta_signal_subscribed(char *dst, int size, const char *id)
{
	mpack_writer_t writer;
	mpack_writer_init(&writer, dst, size);
	mpack_start_map(&writer, 2);
	mpack_write_cstr(&writer, MPACK_KEY_METHOD);
	mpack_write_cstr(&writer, META_METHOD_SUBSCRIBE);
	mpack_write_cstr(&writer, MPACK_KEY_PARAMS);
	mpack_start_map(&writer, 1);
	mpack_write_cstr(&writer, META_SIGNALID);
	mpack_write_cstr(&writer, id);
	mpack_finish_map(&writer);
	mpack_finish_map(&writer);
	return mpack_write_finally(&writer);
}

int build_mpack_meta_signal_unsubscribed(char *dst, int size)
{
	mpack_writer_t writer;
	mpack_writer_init(&writer, dst, size);
	mpack_start_map(&writer, 1);
	mpack_write_cstr(&writer, MPACK_KEY_METHOD);
	mpack_write_cstr(&writer, META_METHOD_UNSUBSCRIBE);
	mpack_finish_map(&writer);
	return mpack_write_finally(&writer);
}

int build_mpack_meta_stream_avail(char *dst, int size, signal_t **signals, int num_signals)
{
	mpack_writer_t writer;
	mpack_writer_init(&writer, dst, size);
	mpack_start_map(&writer, 2);
	mpack_write_cstr(&writer, MPACK_KEY_METHOD);
	mpack_write_cstr(&writer, META_METHOD_AVAILABLE);
	mpack_write_cstr(&writer, MPACK_KEY_PARAMS);
	mpack_start_map(&writer, 1);
	mpack_write_cstr(&writer, META_SIGNALIDS);
	mpack_start_array(&writer, num_signals);
	for (int i = 0; i < num_signals; i++) {
		if (signals[i]->available && !signal_has_subscription(signals[i])) {
			mpack_write_cstr(&writer, signals[i]->definition->name);
		}
	}
	mpack_finish_array(&writer);
	mpack_finish_map(&writer);
	mpack_finish_map(&writer);
	return mpack_write_finally(&writer);
}

int build_mpack_meta_stream_unavail(char *dst, int size, signal_t **signals, int num_signals)
{
	mpack_writer_t writer;
	mpack_writer_init(&writer, dst, size);
	mpack_start_map(&writer, 2);
	mpack_write_cstr(&writer, MPACK_KEY_METHOD);
	mpack_write_cstr(&writer, META_METHOD_UNAVAILABLE);
	mpack_write_cstr(&writer, MPACK_KEY_PARAMS);
	mpack_start_map(&writer, 1);
	mpack_write_cstr(&writer, META_SIGNALIDS);
	mpack_start_array(&writer, num_signals);
	for (int i = 0; i < num_signals; i++) {
		if (signals[i]->available && signal_has_subscription(signals[i])) {
			mpack_write_cstr(&writer, signals[i]->definition->name);
		}
	}
	mpack_finish_array(&writer);
	mpack_finish_map(&writer);
	mpack_finish_map(&writer);
	return mpack_write_finally(&writer);
}
