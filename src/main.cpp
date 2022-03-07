#include <Arduino.h>
#include <ArduinoJson.h>
#include <epd_driver.h>
#include <epd_highlevel.h>
#include "DSEG14Classic_Regular_100.h"
#include "SignsPlus.h"
#include "SignsMinus.h"
#include "Roboto_Bold_40.h"
#include "SailtrackLogo.h"
#include <SailtrackModule.h>

#define BATTERY_ADC_PIN 36
#define BATTERY_ADC_MULTIPLIER 1.7

#define IDENTITY_MULTIPLIER 1
#define MMS_TO_KNOTS_MULTIPLIER 0.00194384
#define UDEGREE_TO_DEGREE_MULTIPLIER 1e-5

#define MONITOR_UPDATE_RATE 2

#define MONITOR_SLOT_0 { 470, 227 }
#define MONITOR_SLOT_1 { 470, 454 }
#define MONITOR_SLOT_2 { 470, 681 }
#define MONITOR_SLOT_3 { 470, 908 }

#define WAVEFORM EPD_BUILTIN_WAVEFORM
#define CLEAR_MONITOR_CYCLES_INTERVAL 600

enum MetricType { SPEED, ANGLE, ANGLE_ZERO_CENTERED };

struct MonitorSlot {
    int cursorX;
    int cursorY;
};

struct MonitorMetric {
    float value;
    char topic[20];
    char name[10];
    char displayName[10];
    double multiplier;
    MetricType type;
    MonitorSlot slot;
} monitorMetrics[] = {
    {0, "sensor/gps0", "speed", "SOG", MMS_TO_KNOTS_MULTIPLIER, SPEED, MONITOR_SLOT_0},
    {0, "sensor/gps0", "heading", "COG", UDEGREE_TO_DEGREE_MULTIPLIER, ANGLE, MONITOR_SLOT_1},
    {0, "sensor/imu0", "euler.x", "HDG", IDENTITY_MULTIPLIER, ANGLE, MONITOR_SLOT_2},
    {0, "sensor/imu0", "euler.y", "RLL", IDENTITY_MULTIPLIER, ANGLE_ZERO_CENTERED, MONITOR_SLOT_3}
};

SailtrackModule STM;

EpdFontProperties fontProps = epd_font_properties_default();
EpdRotation orientation = EPD_ROT_PORTRAIT;
EpdiyHighlevelState hl;
int temperature = 40;
int updateCycles = 0;
uint8_t *fb;

class ModuleCallbacks: public SailtrackModuleCallbacks {
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
            if (!strcmp(topic, metric.topic)) {
                char metricName[sizeof(metric.name)];
                strcpy(metricName, metric.name);
                char * token = strtok(metricName, ".");
                JsonVariant tmpVal = payload.as<JsonVariant>();
                while (token != NULL) {
                    if (!tmpVal.containsKey(token)) return;
                    tmpVal = tmpVal[token];
                    token = strtok(NULL, ".");
                }
                metric.value = tmpVal.as<float>() * metric.multiplier;   
            }
        }
    }
};

void beginEPD() {
    epd_init(EPD_OPTIONS_DEFAULT);
    hl = epd_hl_init(WAVEFORM);
    epd_set_rotation(orientation);
    fb = epd_hl_get_framebuffer(&hl);
    epd_poweron();
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        epd_clear();
        epd_draw_rotated_image({130, 350, SailtrackLogo_width, SailtrackLogo_height}, SailtrackLogo_data, fb);
        epd_hl_update_screen(&hl, MODE_GL16, temperature);
    }
}

void setup() {
    beginEPD();
    STM.begin("monitor", IPAddress(192, 168, 42, 103), new ModuleCallbacks());
    for (auto metric : monitorMetrics)
        STM.subscribe(metric.topic);
}

void loop() { 
    epd_hl_set_all_white(&hl);
    for (auto metric : monitorMetrics) {
        char digits[4];
        char displayName[6];
        int cursorX;
        int cursorY;

        if (metric.type == ANGLE_ZERO_CENTERED) {
            cursorX = 15;
            cursorY = metric.slot.cursorY - 153;
            if (metric.value >= 0) epd_draw_rotated_image({cursorX, cursorY, SignsPlus_width, SignsPlus_height}, SignsPlus_data, fb);
            else epd_draw_rotated_image({cursorX, cursorY, SignsMinus_width, SignsMinus_height}, SignsMinus_data, fb);
            metric.value = abs(metric.value);
        }

        sprintf(digits, metric.type == SPEED ? "%.1f" : "%.0f", metric.value);
        fontProps.flags = EPD_DRAW_ALIGN_RIGHT;
        cursorX = metric.slot.cursorX;
        cursorY = metric.slot.cursorY;
        epd_write_string(&DSEG14Classic_Regular_100, digits, &cursorX, &cursorY, fb, &fontProps);

        sprintf(displayName, "%c\n%c\n%c", metric.displayName[0], metric.displayName[1], metric.displayName[2]);
        fontProps.flags = EPD_DRAW_ALIGN_CENTER;
        cursorX = metric.slot.cursorX + 33;
        cursorY = metric.slot.cursorY - 140;
        epd_write_string(&Roboto_Bold_40, displayName, &cursorX, &cursorY, fb, &fontProps);
    }
    if (!updateCycles) {
        epd_clear();
        epd_hl_set_all_white(&hl);
    }
    epd_hl_update_screen(&hl, updateCycles ? MODE_GL16 : MODE_EPDIY_WHITE_TO_GL16, temperature);
    updateCycles = (updateCycles + 1) % CLEAR_MONITOR_CYCLES_INTERVAL;
    delay(1000 / MONITOR_UPDATE_RATE);
}
