#include <Arduino.h>
#include <Wire.h>
#include <SailtrackModule.h>
#include <epd_driver.h>
#include <epd_highlevel.h>
#include "fonts/DSEG14Classic_Regular_100.h"
#include "fonts/Roboto_Bold_40.h"
#include "images/SailtrackLogo.h"
#include "images/SignsMinus.h"
#include "images/SignsPlus.h"

// -------------------------- Configuration -------------------------- //

#define MONITOR_UPDATE_FREQ_HZ          2

#define BATTERY_ADC_PIN                 36
#define BATTERY_ADC_RESOLUTION          4095
#define BATTERY_ADC_REF_VOLTAGE         1.1
#define BATTERY_ESP32_REF_VOLTAGE       3.3
#define BATTERY_NUM_READINGS            32
#define BATTERY_READING_DELAY_MS	    20

#define METRIC_MULTIPLIER_IDENTITY      1

#define MONITOR_SLOT_0                  { 470, 227 }
#define MONITOR_SLOT_1                  { 470, 454 }
#define MONITOR_SLOT_2                  { 470, 681 }
#define MONITOR_SLOT_3                  { 470, 908 }
#define MONITOR_WAVEFORM                EPD_BUILTIN_WAVEFORM
#define MONITOR_CLEAR_INTERVAL_UPDATES  600
#define MONITOR_TEMPERATURE_CELSIUS     40

#define LOOP_TASK_INTERVAL_MS           1000 / MONITOR_UPDATE_FREQ_HZ

enum MetricType { SPEED, ANGLE, ANGLE_ZERO_CENTERED };

struct MonitorSlot {
    int cursorX;
    int cursorY;
};

struct MonitorMetric {
    float value;
    char topic[32];
    char name[32];
    char displayName[8];
    double multiplier;
    MetricType type;
    MonitorSlot slot;
} monitorMetrics[] = {
    { 0, "boat", "sog", "SOG", METRIC_MULTIPLIER_IDENTITY, SPEED, MONITOR_SLOT_0 },
    { 0, "boat", "drift", "DFT", METRIC_MULTIPLIER_IDENTITY, ANGLE_ZERO_CENTERED, MONITOR_SLOT_1 },
    { 0, "boat", "pitch", "PTC", METRIC_MULTIPLIER_IDENTITY, ANGLE_ZERO_CENTERED, MONITOR_SLOT_2 },
    { 0, "boat", "roll", "RLL", METRIC_MULTIPLIER_IDENTITY, ANGLE_ZERO_CENTERED, MONITOR_SLOT_3 }
};

// ------------------------------------------------------------------- //

SailtrackModule stm;

EpdFontProperties fontProps = epd_font_properties_default();
EpdRotation orientation = EPD_ROT_PORTRAIT;
EpdiyHighlevelState hl;
int updateCycles = 0;
uint8_t *fb;

class ModuleCallbacks: public SailtrackModuleCallbacks {
    void onStatusPublish(JsonObject status) {
		JsonObject battery = status.createNestedObject("battery");
		float avg = 0;
		for (int i = 0; i < BATTERY_NUM_READINGS; i++) {
			avg += analogRead(BATTERY_ADC_PIN) / BATTERY_NUM_READINGS;
			delay(BATTERY_READING_DELAY_MS);
		}
		battery["voltage"] = 2 * avg / BATTERY_ADC_RESOLUTION * BATTERY_ESP32_REF_VOLTAGE * BATTERY_ADC_REF_VOLTAGE;
	}

    void onMqttMessage(const char * topic, JsonObjectConst message) {
        for (int i = 0; i < sizeof(monitorMetrics)/sizeof(*monitorMetrics); i++) {
            MonitorMetric & metric = monitorMetrics[i];
            if (!strcmp(topic, metric.topic)) {
                char metricName[strlen(metric.name)+1];
                strcpy(metricName, metric.name);
                char * token = strtok(metricName, ".");
                JsonVariantConst tmpVal = message;
                while (token) {
                    if (!tmpVal.containsKey(token)) break;
                    tmpVal = tmpVal[token];
                    token = strtok(NULL, ".");
                }
                if (!token)
                    metric.value = tmpVal.as<float>() * metric.multiplier;   
            }
        }
    }
};

void beginEPD() {
    epd_init(EPD_OPTIONS_DEFAULT);
    hl = epd_hl_init(MONITOR_WAVEFORM);
    epd_set_rotation(orientation);
    fb = epd_hl_get_framebuffer(&hl);
    epd_poweron();
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        epd_clear();
        epd_draw_rotated_image({130, 350, SailtrackLogo_width, SailtrackLogo_height}, SailtrackLogo_data, fb);
        epd_hl_update_screen(&hl, MODE_GL16, MONITOR_TEMPERATURE_CELSIUS);
    }
}

void setup() {
    beginEPD();
    stm.begin("monitor", IPAddress(192, 168, 42, 103), new ModuleCallbacks());
    for (auto metric : monitorMetrics)
        stm.subscribe(metric.topic);
}

void loop() { 
    TickType_t lastWakeTime = xTaskGetTickCount();

    epd_hl_set_all_white(&hl);
    for (int i = 0; i < sizeof(monitorMetrics)/sizeof(*monitorMetrics); i++) {
        MonitorMetric & metric = monitorMetrics[i];
        char digits[8];
        char displayName[8];
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
        metric.value = 0;
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
    epd_hl_update_screen(&hl, updateCycles ? MODE_GL16 : MODE_EPDIY_WHITE_TO_GL16, MONITOR_TEMPERATURE_CELSIUS);
    updateCycles = (updateCycles + 1) % MONITOR_CLEAR_INTERVAL_UPDATES;

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(LOOP_TASK_INTERVAL_MS));
}
