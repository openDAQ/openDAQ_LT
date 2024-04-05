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

#include "streaming_signals.h"
#include "RTOS.h"
#include "streaming_config.h"
#include "streaming_handler.h"

static OS_MUTEX signal_mutex;
static uint32_t signal_counter = 0;
static uint32_t table_counter = 0;
static signal_t signals[STREAMING_MAX_SIGNALS];
static signal_table_t signal_tables[STREAMING_MAX_TABLES];
extern struct streaming_callbacks *streaming_cbs;

void signals_send_all_avail(const struct stream *stream)
{
	signal_t *signals_available[STREAMING_MAX_SIGNALS];
	uint8_t num_avail = 0;
	for (unsigned int i = 0; i < signal_counter; i++) {
		if (signals[i].available && !signal_has_subscription(&signals[i])) {
			signals_available[num_avail++] = &signals[i];
		}
	}

	streaming_send_avail(stream, signals_available, num_avail);
}

static signal_t *get_signal_by_id(const char *signalId)
{
	for (unsigned int i = 0; i < signal_counter; i++) {
		if (!strcmp(signalId, signals[i].definition->name)) {
			return &signals[i];
		}
	}
	return NULL;
}

static signal_t *signals_add_signal(signal_definition_t *def, signal_table_t *table)
{
	OS_MUTEX_LockBlocked(&signal_mutex);
	// check if we can handle more signals
	if (signal_counter >= STREAMING_MAX_SIGNALS) {
		return NULL;
	}

	signal_t *signal = &signals[signal_counter++];
	signal->stream = NULL;
	signal->available = !def->hidden;
	signal->definition = def;
	signal->table = table;
	OS_MUTEX_Unlock(&signal_mutex);
	return signal;
}

signal_table_t *signals_add_table(signal_definition_t *def, unsigned int count, const char *table_name)
{
	if (count == 0) {
		return NULL;
	}

	OS_MUTEX_LockBlocked(&signal_mutex);

	if (table_counter >= STREAMING_MAX_TABLES || signal_counter + count > STREAMING_MAX_SIGNALS) {
		OS_MUTEX_Unlock(&signal_mutex);
		return NULL;
	}

	signal_table_t *table = &signal_tables[table_counter++];
	table->signals = signals_add_signal(&def[0], table);
	for (unsigned int i = 1; i < count; i++) {
		signals_add_signal(&def[i], table);
	}
	table->signal_counter = count;
	table->subscribed_value_signal_count = 0;
	table->tableId = table_name;

	OS_MUTEX_Unlock(&signal_mutex);
	return table;
}

bool signal_has_subscription(signal_t *signal)
{
	return signal->stream != NULL;
}

static int _signal_subscribe(const struct stream *stream, signal_t *signal, uint64_t valueIndex)
{
	if (signal->stream != NULL) {
		return -1;
	}

	signal->stream = stream;
	signal->subscribed = true;
	streaming_send_subscribed(stream, signal);
	streaming_send_meta_signal(stream, signal, valueIndex);
	return 0;
}

static int _signal_unsubscribe(const struct stream *stream, signal_t *signal)
{
	if (signal->stream != stream) {
		return -1;
	}

	signal->subscribed = false;
	signal->stream = NULL;
	streaming_send_unsubscribed(stream, signal);
	return 0;
}

int signals_subscribe(const struct stream *stream, const char *signalId)
{
	OS_MUTEX_LockBlocked(&signal_mutex);
	signal_t *signal = get_signal_by_id(signalId);

	if (signal == NULL) {
		OS_MUTEX_Unlock(&signal_mutex);
		return -1;
	}

	signal_table_t *table = signal->table;

	if (table != NULL) {
		signal_t *related_signal = table->signals;
		for (unsigned int i = 0; i < table->signal_counter; i++) {
			if (&related_signal[i] == signal) {
				// ignore the signal itself in the table
				continue;
			}
			if (related_signal[i].definition->signaltype == signal_type_value) {
				// the signal is a value signal. We dont automatically subscribe to more value signals
				continue;
			}
			if (related_signal[i].subscribed) {
				// the signal is already subscribed, therefore ignore here
				continue;
			}
			// otherwise we subscribe to this signal
			streaming_cbs->on_subscribe(stream, &related_signal[i]);
			_signal_subscribe(stream, &related_signal[i], 0); // valueIndex is fixed to 0 and gets ignored
		}
		if (!signal->subscribed && signal->definition.signaltype == signal_type_value) {
			table->subscribed_value_signal_count++;
		}
	}

	uint64_t valueIndex = streaming_cbs->on_subscribe(stream, signal);
	int ret = _signal_subscribe(stream, signal, valueIndex);
	OS_MUTEX_Unlock(&signal_mutex);
	return ret;
}

void signals_init(void)
{
	OS_MUTEX_Create(&signal_mutex);
}

int signals_unsubscribe(const struct stream *stream, const char *signalId)
{
	OS_MUTEX_LockBlocked(&signal_mutex);
	signal_t *signal = get_signal_by_id(signalId);

	if (signal == NULL || !signal->subscribed) {
		OS_MUTEX_Unlock(&signal_mutex);
		return -1;
	}

	signal_table_t *table = signal->table;

	if (table != NULL && signal->definition->signaltype == signal_type_value) {
		table->subscribed_value_signal_count--;
		if (table->subscribed_value_signal_count == 0) {
			// if this is the last value signal to unsubscribe from in this table
			signal_t *related_signal = table->signals;
			for (unsigned int i = 0; i < table->signal_counter; i++) {
				if (&related_signal[i] == signal) {
					// ignore the signal itself in the table
					continue;
				}
				if (related_signal[i].definition->signaltype == signal_type_value) {
					// the signal is a value signal. We dont automatically unsubscribe from more value signals
					continue;
				}
				if (!related_signal[i].subscribed) {
					// the signal is not subscribed, therefore ignore here
					continue;
				}
				_signal_unsubscribe(stream, &related_signal[i]);
				streaming_cbs->on_unsubscribe(stream, &related_signal[i]);
			}
		}
	}

	int ret = _signal_unsubscribe(stream, signal);
	streaming_cbs->on_unsubscribe(stream, signal);
	OS_MUTEX_Unlock(&signal_mutex);
	return ret;
}

void signals_purge_stream(const struct stream *stream)
{
	for (uint32_t i = 0; i < signal_counter; i++) {
		if (signals[i].stream == stream) {
			signals[i].stream = NULL;
			signals[i].subscribed = false;
			if (signals[i].table != NULL) {
				signals[i].table->subscribed_value_signal_count = 0;
			}
		}
	}
}

unsigned int signal_get_signal_no(signal_t *signal)
{
	// use index + 1  as signal_number, since signal_number cannot be 0
	return (signal - signals) + 1;
}