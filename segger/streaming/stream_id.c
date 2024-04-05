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

#include "stream_id.h"
#include "IP.h"

struct stream single_stream;

static int socket_send(const struct stream *s, const char *buf, size_t len)
{
	return send(s->socket_handle, buf, len, 0);
}

static int socket_send_packet(const struct stream *s, void *p)
{
	return IP_TCP_SendAndFree(s->socket_handle, (IP_PACKET *)p);
}

void stream_free(struct stream *s)
{
	// socket handle closed elsewhere
	s->socket_handle = 0;
}

struct stream *stream_malloc(int socket, const char *id)
{
	single_stream.socket_handle = socket;
	single_stream.stream = socket_send;
	single_stream.streamp = socket_send_packet;
	single_stream.id = id;
	return &single_stream;
}

void streaming_streams_init(void)
{
	for (int i = 0; i < NUM_STREAMS_MAX; i++) {
		single_stream.socket_handle = 0;
	}
}