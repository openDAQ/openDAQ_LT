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

#include "IP.h"

// define your strings
#define DEVICE_NAME "testdevice"
#define MODEL_NAME "openDAQdevice"
#define SERIAL_NUMBER "12345"
#define TIME_TO_LIVE 1200

static const IP_DNS_SERVER_SD_CONFIG SDConfig[] = {
    {
        .Type = IP_DNS_SERVER_TYPE_PTR,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config =
            {
                .PTR =
                    {
                        .sName = "_streaming-ws._tcp.local",
                        .sDomainName = DEVICE_NAME"._streaming-ws._tcp.local",
                    },
            },
    },
    {
        .Type = IP_DNS_SERVER_TYPE_SRV,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config =
            {
                .SRV =
                    {
                        .sName = DEVICE_NAME"._streaming-ws._tcp.local",
                        .Priority = 0,
                        .Weight = 0,
                        .Port = 80,
                        .sTarget = DEVICE_NAME".local",
                    },
            },
    },
    {
        .Type = IP_DNS_SERVER_TYPE_A,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config = {.A =
                       {
                           .sName = DEVICE_NAME".local",
                           .IPAddr = 0,
                       }},
    },
    {
        .Type = IP_DNS_SERVER_TYPE_TXT,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config =
            {
                .TXT =
                    {
                        .sName = DEVICE_NAME"._streaming-ws._tcp.local",
                        .sTXT = "path=/stream",
                    },
            },
    },
    {
        .Type = IP_DNS_SERVER_TYPE_TXT,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config =
            {
                .TXT =
                    {
                        .sName = DEVICE_NAME"._streaming-ws._tcp.local",
                        .sTXT = "caps=WS",
                    },
            },
    },
    {
        .Type = IP_DNS_SERVER_TYPE_TXT,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config =
            {
                .TXT =
                    {
                        .sName = DEVICE_NAME"._streaming-ws._tcp.local",
                        .sTXT = "name="DEVICE_NAME,
                    },
            },
    },
    {
        .Type = IP_DNS_SERVER_TYPE_TXT,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config =
            {
                .TXT =
                    {
                        .sName = DEVICE_NAME"._streaming-ws._tcp.local",
                        .sTXT = "model="MODEL_NAME,
                    },
            },
    },
    {
        .Type = IP_DNS_SERVER_TYPE_TXT,
        .Flags = IP_DNS_SERVER_FLAG_FLUSH,
        .TTL = 0,
        .Config =
            {
                .TXT =
                    {
                        .sName = DEVICE_NAME"._streaming-ws._tcp.local",
                        .sTXT = "serialNumber="SERIAL_NUMBER,
                    },
            },
    },
};

static IP_DNS_SERVER_CONFIG mdns_server_config = {DEVICE_NAME".local", TIME_TO_LIVE, sizeof(SDConfig) / sizeof(SDConfig[0]), SDConfig};

int openDAQ_discovery_start(void)
{
	return IP_MDNS_SERVER_Start(&mdns_server_config); 
}

int openDAQ_discovery_stop(void)
{
	return IP_MDNS_SERVER_Stop(); 
}

