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

#include "streaming_jsonrpc.h"
#include "IP_Webserver.h"
#include "mjson/src/mjson.h"
#include "mpack.h"
#include "streaming_signals.h"
#include <stdio.h>

static struct jsonrpc_ctx ctx;
static WEBS_METHOD_HOOK streaming_hook;

static int rpc_sender(const char *data_to_send, int data_len, void *privdata)
{
	IP_WEBS_SendMem(privdata, data_to_send, data_len);
	return data_len;
}

static void rpc_cb_subscribe(struct jsonrpc_request *req)
{
	char signal_id[STREAMING_SIGNAL_NAME_LENGTH];
	char path[14];
	bool success = true;

	for (int i = 0;;) {
		snprintf(path, sizeof(path), "$[%d]", i++);
		if (mjson_get_string(req->params, req->params_len, path, signal_id, sizeof(signal_id)) >= 1) {
			success = signals_subscribe(req->userdata, signal_id) == 0;
		} else {
			break;
		}
	}

	if (success) {
		jsonrpc_return_success(req, "true");
	} else {
		jsonrpc_return_error(req, -32602, "Invalid params", NULL);
	}
}

static void rpc_cb_unsubscribe(struct jsonrpc_request *req)
{
	char signal_id[STREAMING_SIGNAL_NAME_LENGTH];
	char path[14];
	bool success = true;

	for (int i = 0;;) {
		snprintf(path, sizeof(path), "$[%d]", i++);
		if (mjson_get_string(req->params, req->params_len, path, signal_id, sizeof(signal_id)) >= 1) {
			success = signals_unsubscribe(req->userdata, signal_id) == 0;
		} else {
			break;
		}
	}

	if (success) {
		jsonrpc_return_success(req, "true");
	} else {
		jsonrpc_return_error(req, -32602, "Invalid params", NULL);
	}
}

static int streaming_jsonrpc_callback(void *pContext, WEBS_OUTPUT *pOutput, const char *sMethod, const char *sAccept,
                                      const char *sContentType, const char *sResource, U32 ContentLen)
{
	(void)sMethod;
	(void)sAccept;
	(void)sContentType;
	(void)sResource;

	// buffer must be big enough to hold the complete JSON RPC.
	char buf[JSONRPC_BUF_SIZE + 1]; // place a null terminator
	int len = IP_WEBS_METHOD_CopyData(pContext, buf, ContentLen < JSONRPC_BUF_SIZE ? ContentLen : JSONRPC_BUF_SIZE);
	buf[len < 0 ? 0 : len] = '\0';
	IP_WEBS_SendHeaderEx(pOutput, NULL, "application/json", 1);
	jsonrpc_ctx_process(&ctx, buf, (len > 0) ? len : 0, rpc_sender, pOutput, &single_stream);
	IP_WEBS_Flush(pOutput);
	return 0;
}

void streaming_jsonrpc_init(char stream_id[9])
{
	jsonrpc_ctx_init(&ctx, NULL, NULL);
	static char subscribe_method[19];
	static char unsubscribe_method[21];
	snprintf(subscribe_method, sizeof(subscribe_method), "%s.subscribe", stream_id);
	snprintf(unsubscribe_method, sizeof(unsubscribe_method), "%s.unsubscribe", stream_id);
	jsonrpc_ctx_export(&ctx, subscribe_method, rpc_cb_subscribe);
	jsonrpc_ctx_export(&ctx, unsubscribe_method, rpc_cb_unsubscribe);
	IP_WEBS_METHOD_AddHook_SingleMethod(&streaming_hook, streaming_jsonrpc_callback, JSONRPC_PATH, JSONRPC_METHOD);
}
