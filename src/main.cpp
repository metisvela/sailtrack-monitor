#include <Arduino.h>
#include <ArduinoJson.h>
#include "epd_driver.h"
#include "epd_highlevel.h"
#include "DSEG14Classic_Regular_100.h"
#include "BebasNeue_Regular_40.h"
#include "BebasNeue_Regular_60.h"
#include "SailtrackLogo.h"
#include "MetisLogo.h"
#include "SailtrackModule.h"

#define BATTERY_ADC_PIN 36
#define BATTERY_ADC_MULTIPLIER 1.7

#define IDENTITY_MULTIPLIER 1
#define MMS_TO_KNOTS_MULTIPLIER 0.00194384
#define UDEGREE_TO_DEGREE_MULTIPLIER 1e-5

#define MONITOR_UPDATE_RATE 2
#define MONITOR_UPDATE_PERIOD_MS 1000 / MONITOR_UPDATE_RATE

#define MONITOR_SLOT_0 { 470, 227 }
#define MONITOR_SLOT_1 { 470, 454 }
#define MONITOR_SLOT_2 { 470, 681 }
#define MONITOR_SLOT_3 { 470, 908 }

#define WAVEFORM EPD_BUILTIN_WAVEFORM

enum MetricType { SPEED, ANGLE };

struct MonitorSlot {
    int cursorX;
    int cursorY;
};

struct MonitorMetric {
    float value;
    char topic[20];
    char name[10];
    double multiplier;
    MetricType type;
    MonitorSlot slot;
} monitorMetrics[] = {
    {0, "sensor/gps0", "speed", MMS_TO_KNOTS_MULTIPLIER, SPEED, MONITOR_SLOT_0},
    {0, "sensor/gps0", "heading", UDEGREE_TO_DEGREE_MULTIPLIER, ANGLE, MONITOR_SLOT_1},
    {0, "metric/boat", "sog", IDENTITY_MULTIPLIER, SPEED, MONITOR_SLOT_2},
    {0, "metric/boat", "cog", IDENTITY_MULTIPLIER, ANGLE, MONITOR_SLOT_3}
};

EpdFontProperties fontProps = epd_font_properties_default();
EpdRotation orientation = EPD_ROT_PORTRAIT;
EpdiyHighlevelState hl;
int temperature = 20;
uint8_t *fb;

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
        for (int i = 0; i < sizeof(monitorMetrics); i++) {
            MonitorMetric & metric = monitorMetrics[i];
            if (!strcmp(topic, metric.topic) && payload.containsKey(metric.name))
                metric.value = payload[metric.name].as<float>() * metric.multiplier;
        }   
    }
};

void monitorTask(void * pvArguments) {
    while (true) {
        epd_hl_set_all_white(&hl);
        for (auto metric : monitorMetrics) {
            char digits[5];
            char unit[3];
            int cursorX = metric.slot.cursorX;
            int cursorY = metric.slot.cursorY;
            if (metric.type == SPEED) sprintf(digits, "%.1f", metric.value);
            else if (metric.type == ANGLE) sprintf(digits, "%d", (int)round(metric.value));
            fontProps.flags = EPD_DRAW_ALIGN_RIGHT;
            epd_write_string(&DSEG14Classic_Regular_100, digits, &cursorX, &cursorY, fb, &fontProps);
            fontProps.flags = EPD_DRAW_ALIGN_LEFT;
            cursorX = metric.slot.cursorX + 5;
            if (metric.type == SPEED) {
                cursorY = metric.slot.cursorY - 130;
                sprintf(unit, "kt");
                epd_write_string(&BebasNeue_Regular_40, unit, &cursorX, &cursorY, fb, &fontProps);
            } else if (metric.type == ANGLE) {
                cursorY = metric.slot.cursorY - 110;
                sprintf(unit, "°");
                epd_write_string(&BebasNeue_Regular_60, unit, &cursorX, &cursorY, fb, &fontProps);
            }
        }
        epd_hl_update_screen(&hl, MODE_GL16, temperature);
        delay(MONITOR_UPDATE_PERIOD_MS);
    }
}

void beginEPD() {
    epd_init(EPD_OPTIONS_DEFAULT);
    hl = epd_hl_init(WAVEFORM);
    epd_set_rotation(orientation);
    fb = epd_hl_get_framebuffer(&hl);
    epd_clear();
    epd_poweron();
    epd_draw_rotated_image({130, 340, SailtrackLogo_width, SailtrackLogo_height}, SailtrackLogo_data, fb);
    epd_draw_rotated_image({203, 880, MetisLogo_width, MetisLogo_height}, MetisLogo_data, fb);
    epd_hl_update_screen(&hl, MODE_GL16, temperature);
}

void setup() {
    beginEPD();
    STModule.begin("monitor", "sailtrack-monitor", IPAddress(192,168,42,103));
    STModule.setCallbacks(new ModuleCallbacks());
    for (auto metric : monitorMetrics)
        STModule.subscribe(metric.topic);
    xTaskCreate(monitorTask, "monitor_task", TASK_MEDIUM_STACK_SIZE, NULL, TASK_MEDIUM_PRIORITY, NULL);
}

void loop() { }
