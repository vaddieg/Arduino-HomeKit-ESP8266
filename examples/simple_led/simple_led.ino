/*
 * simple_led.ino
 *
 * This accessory contains a builtin-led on NodeMCU and a "virtual" occupancy sensor.
 * Setup code: 111-11-111
 * The Flash-Button(D3, GPIO0) on NodeMCU:
 * 		single-click: turn on/off the builtin-led (D4, GPIO2)
 * 		double-click: toggle the occupancy sensor state
 * 		long-click: reset the homekit server (remove the saved pairing)
 *
 *  Created on: 2020-02-08
 *      Author: Mixiaoxiao (Wang Bin)
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <arduino_homekit_server.h>
#include "ButtonDebounce.h"
#include "ButtonHandler.h"

#define PL(s) Serial.println(s)
#define P(s) Serial.print(s)

//D0 16 //led
//D3  0 //flash button
//D4  2 //led

#define PIN_LED 16//D0

const char *ssid = "your-ssid";
const char *password = "your-password";

void blink_led(int interval, int count) {
	for (int i = 0; i < count; i++) {
		builtinledSetStatus(true);
		delay(interval);
		builtinledSetStatus(false);
		delay(interval);
	}
}

void setup() {
	Serial.begin(115200);
	Serial.setRxBufferSize(32);
	Serial.setDebugOutput(false);

	pinMode(PIN_LED, OUTPUT);
	WiFi.mode(WIFI_STA);
	WiFi.persistent(false);
	WiFi.disconnect(false);
	WiFi.setAutoReconnect(true);
	WiFi.begin(ssid, password);

	printf("\n");
	printf("SketchSize: %d B\n", ESP.getSketchSize());
	printf("FreeSketchSpace: %d B\n", ESP.getFreeSketchSpace());
	printf("FlashChipSize: %d B\n", ESP.getFlashChipSize());
	printf("FlashChipRealSize: %d B\n", ESP.getFlashChipRealSize());
	printf("FlashChipSpeed: %d\n", ESP.getFlashChipSpeed());
	printf("SdkVersion: %s\n", ESP.getSdkVersion());
	printf("FullVersion: %s\n", ESP.getFullVersion().c_str());
	printf("CpuFreq: %dMHz\n", ESP.getCpuFreqMHz());
	printf("FreeHeap: %d B\n", ESP.getFreeHeap());
	printf("ResetInfo: %s\n", ESP.getResetInfo().c_str());
	printf("ResetReason: %s\n", ESP.getResetReason().c_str());
	DEBUG_HEAP();
	homekit_setup();
	DEBUG_HEAP();
	blink_led(200, 3);
}

void loop() {
	homekit_loop();
	delay(5);
}

void builtinledSetStatus(bool on) {
	digitalWrite(PIN_LED, on ? LOW : HIGH);
}

//==============================
// Homekit setup and loop
//==============================

extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t name;
extern "C" void occupancy_toggle();
extern "C" void led_toggle();
extern "C" void accessory_init();

ButtonDebounce btn(0, INPUT_PULLUP, LOW);
ButtonHandler btnHandler;

void IRAM_ATTR btnInterrupt() {
	btn.update();
}

uint32_t next_heap_millis = 0;

void homekit_setup() {
	accessory_init();
	uint8_t mac[WL_MAC_ADDR_LENGTH];
	WiFi.macAddress(mac);
	int name_len = snprintf(NULL, 0, "%s_%02X%02X%02X",
			name.value.string_value, mac[3], mac[4], mac[5]);
	char *name_value = (char*)malloc(name_len + 1);
	snprintf(name_value, name_len + 1, "%s_%02X%02X%02X",
			name.value.string_value, mac[3], mac[4], mac[5]);
	name.value = HOMEKIT_STRING_CPP(name_value);

	arduino_homekit_setup(&config);

	btn.setCallback([](const bool down) {
		btnHandler.handleChange(down);
	});
	btn.setInterrupt(btnInterrupt);

	btnHandler.setIsDownFunction([](void) {
		return btn.checkIsDown();
	});
	btnHandler.setCallback([](const button_event e) {
		P(F("Button Event: "));
		switch (e) {
		case BUTTON_EVENT_SINGLECLICK:
		PL(F("SINGLECLICK"));
		led_toggle();
		break;
	case BUTTON_EVENT_DOUBLECLICK:
		PL(F("DOUBLECLICK"));
		occupancy_toggle();
		break;
	case BUTTON_EVENT_LONGCLICK:
		PL(F("LONGCLICK"));
		homekit_storage_reset();
		system_restart();
		break;
	}
}	);
}

void homekit_loop() {
	btnHandler.loop();
	arduino_homekit_loop();
	uint32_t time = millis();
	if (time > next_heap_millis) {
		INFO("heap: %d, sockets: %d",
				ESP.getFreeHeap(), arduino_homekit_connected_clients_count());
		next_heap_millis = time + 5000;
	}
}
