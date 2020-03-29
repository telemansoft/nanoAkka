#ifndef MQTTSERIAL_H
#define MQTTSERIAL_H

#include <string>
#include <NanoAkka.h>
#include <Hardware.h>
#include <Mqtt.h>
#ifdef ESP_IDF
#include "driver/uart.h"
#endif

#define QOS 0
#define TIMEOUT 10000L
#define TIMER_KEEP_ALIVE 1
#define TIMER_CONNECT 2
#define TIMER_SERIAL 3

class MqttSerial : public Mqtt, public Sink<TimerMsg, 3>
{

	StaticJsonDocument<3000> _jsonBuffer;
	NanoString _clientId;
	NanoString _address;
	NanoString _lwt_topic;
	NanoString _lwt_message;
#ifndef ARDUINO
	UART &_uart;
#endif

private:
	StaticJsonDocument<256> txd;
	StaticJsonDocument<256> rxd;
	NanoString _rxdString;
	NanoString _loopbackTopic;
	uint64_t _loopbackReceived;
	NanoString _hostPrefix;

	enum
	{
		CMD_SUBSCRIBE = 0,
		CMD_PUBLISH
	};

	void handleSerialByte(uint8_t);
	void rxdSerial(NanoString &);
	void txdSerial(JsonDocument &);
	void publish(NanoString &topic, NanoString message);
	void subscribe(NanoString &topic);

public:
	static void onRxd(void *);

	ValueSource<bool> connected;
	TimerSource keepAliveTimer;
	TimerSource connectTimer;
	MqttSerial(Thread &thr);
	~MqttSerial();
	void init();

	void mqttPublish(const char *topic, const char *message);
	void mqttSubscribe(const char *topic);
	void mqttConnect();
	void mqttDisconnect();

	bool handleMqttMessage(const char *message);
	void observeOn(Thread &);

	void on(const TimerMsg &);
	void on(const MqttMessage &);
	void request();
};

#endif // MQTTSERIAL_H
