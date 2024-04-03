# README

## External Requirements
- SEGGER embOS
- SEGGER emNet
- SEGGER emWeb
- mjson [https://github.com/cesanta/mjson](https://github.com/cesanta/mjson)
- mpack [https://github.com/ludocode/mpack](https://github.com/ludocode/mpack)

## Restrictions
The implementation contains the following restrictions towards the specification:
- Only support one (1) streaming connection open at a time.
- The signals and their definition have to be known at startup time. No dynamic appearing/disappearing of signals is supported.
- The websocket connection upgrade is handled by emWeb. emWeb is case sensitive on HTTP header fields.
- The custom websocket RX implementation cannot handle fragmented websocket frames.
- No support for structure and bitfield data types.

## Usage

### Initialisation
```
struct streaming_callbacks {
	connect_callback *on_connect;
	subscribe_callback *on_subscribe;
	unsubscribe_callback *on_unsubscribe;
};
void streaming_init(struct streaming_callbacks *streaming_cb);
```
Initialises the openDAQ streaming framework and sets up all relevant data structures. A list of callbacks is provided. If streaming is compiled without `STREAMING_INCLUDE_CONFIG_CHANNEL` the `on_subscribe` and `on_unsubscribe` callbacks can be `NULL`. `on_subscribe` expects as return value the "valueIndex" for this signals meta information paket.

Afterwards signals can be added to the streaming module
```
typedef struct {
	const char *name;
	signal_rule_e rule;
	signal_data_type_e datatype;
	signal_type_e signaltype;
	bool hidden;
	uint64_t delta;
	const time_object_t *time;
} signal_definition_t;
signal_table_t *signals_add_table(signal_definition_t *def, unsigned int count, const char *table_name);
```
Pass an Array of signal_definition_t to the function which all belong to the same table. The number of signal definitions is the parameter count. A table name must be given as a string. The signal definition contains:
- `name`: a string used as a name for the signal
- `rule`: linear, constant, or explicit.
- `datatype`: the data type of the signal
- `signaltype`: value, time, or status. This information is used to automatically determin relations between signals in a table. A table cannot contain more than one time signal, nor can it contain more than one status signal.
- `hidden`: whether this signal is advertised for subscription. Usually time signals and measurement status signals are hidden.
- `delta`: delta value for linear signals. Ignored on other rules.
- `time`: pointer to a time object.

### Startup
```
void streaming_start(void);
```
starts the streaming server. It needs to be executed from its own task and never returns.

### Data Serialzation Functions
Four functions can be used to serialize signal data into a buffer:
 ```
int openDAQ_streaming_serialize_explicit_signal(void *dst, size_t dst_size, signal_t *signal, const void *src, unsigned int num);
int openDAQ_streaming_serialize_constant_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal, const void *src);
int openDAQ_streaming_serialize_linear_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal, const void *src);
int openDAQ_streaming_serialize_implicit_signal(void *dst, size_t dst_size, uint64_t index, signal_t *signal, const void *src);
 ```

The buffer to serialize into must be supplied by the user. It is explicitly allowed to serialize multiple signals consecutivly into a buffer and send them out afterwards in one go. It is also possible to use zero-copy TCP Packets and serialise the signals therein. In this case it might be difficult to predict the exact size of the TCP packet required to hold all the data. A valid workaround is to allocate packets through `IP_TCP_Alloc` with a maximum size and later call `IP_UDP_ReducePayloadLen` on this packet to reduce it to the correct size. Although the name of the function `IP_UDP_ReducePayloadLen` suggests it only works with UDP packets, it can also be used on TCP packets.

### Data Transmittion
Two functions can be used to send out serialized data. One is intended for raw buffers, the other for zero-copy TCP packets.
```
int stream->stream(const struct stream *s, const char *pBuffer, size_t NumBytes);
int stream->streamp(const struct stream *s, void *pPacket);
 ```
The first functions is a wrapper around the `send` of Berkley sockets and can therefore block indefinitly. The return values are:
- <0: Error (SOCKET_ERROR).
- ≥0: OK, Number of bytes accepted by the stack and ready to be sent. This can only be the full number of bytes, since the function would otherwise block.

The second function is a wrapper around `IP_TCP_SendAndFree`. It does not block. The return values are:
- =0: The packet was sent successfully.
- <0: The packet was not accepted by the stack.
- \>0: The packet has been accepted and queued on the socket but has not yet been transmitted.

The packet is automatically freed after processing independent from the success of the send operation.
