#include <Arduino.h>
#include <ArduinoJson.h>
#include "epd_driver.h"
#include "epd_highlevel.h"
#include "DSEG14_Classic_Regular100.h"
#include "SailtrackModule.h"

#define BATTERY_ADC_PIN 36
#define BATTERY_ADC_MULTIPLIER 1.7

#define MMS_TO_KNOTS_MULTIPLIER 0.00194384

#define WAVEFORM EPD_BUILTIN_WAVEFORM

struct DisplaySlot {
    char topicName[20];
    char metricName[10];
    int cursor_x;
    int cursor_y;
    EpdRect area;
    double multiplier;
} displaySlots[] = {
    {"sensor/gps0", "speed", 500, 227, {713, 0, 227, 540}, MMS_TO_KNOTS_MULTIPLIER}
};

EpdFontProperties font_props = epd_font_properties_default();
EpdRotation orientation = EPD_ROT_PORTRAIT;
EpdiyHighlevelState hl;
int temperature = 20;
uint8_t *fb;
unsigned long displayReady = millis();
unsigned long displayClear = 200;

void updateSlot(DisplaySlot slot, int value) {
    if (millis() < displayReady) return;
    if (!displayClear--) {
        epd_clear();
        displayClear = 200;
    }
    char digits[3];
    int cursor_x = slot.cursor_x;
    int cursor_y = slot.cursor_y;
    sprintf(digits, "%d", value);
    epd_hl_set_all_white(&hl);
    epd_write_string(&DSEG14_Classic_Regular100, digits, &cursor_x, &cursor_y, fb, &font_props);
    epd_hl_update_screen(&hl, MODE_GL16, temperature);
    displayReady = millis() + 500;
}

class ModuleCallbacks: public SailtrackModuleCallbacks {
	void onWifiConnectionBegin() {
		// TODO: Notify user
	}
	
	void onWifiConnectionResult(wl_status_t status) {
		// TODO: Notify user
	}

	DynamicJsonDocument getStatus() {
		DynamicJsonDocument payload(300);
		JsonObject battery = payload.createNestedObject("battery");
		JsonObject cpu = payload.createNestedObject("cpu");
		battery["voltage"] = analogRead(BATTERY_ADC_PIN) * BATTERY_ADC_MULTIPLIER / 1000;
		cpu["temperature"] = temperatureRead();
		return payload;
	}

    void onMqttMessage(const char * topic, const char * message) {
        DynamicJsonDocument payload(500);
        deserializeJson(payload, message);
        for (auto slot : displaySlots) {
            if (!strcmp(topic, slot.topicName)) {
                int value = round(payload[slot.metricName].as<int>() * slot.multiplier);
                updateSlot(slot, value);
            }
        }
    }
};

void beginEPD() {
    font_props.flags = EPD_DRAW_ALIGN_RIGHT;
    epd_init(EPD_OPTIONS_DEFAULT);
    hl = epd_hl_init(WAVEFORM);
    epd_set_rotation(orientation);
    fb = epd_hl_get_framebuffer(&hl);
    epd_clear();
    epd_poweron();
}

void setup() {
    beginEPD();
    STModule.begin("monitor", "sailtrack-monitor", IPAddress(192,168,42,103));
    STModule.setCallbacks(new ModuleCallbacks());
    for (auto slot : displaySlots)
        STModule.subscribe(slot.topicName);
}

void loop() { }
