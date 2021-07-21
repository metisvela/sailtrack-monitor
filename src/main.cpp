#include <Arduino.h>

// esp32 sdk imports
#include "esp_heap_caps.h"
#include "esp_log.h"

// epd
#include "epd_driver.h"
#include "epd_highlevel.h"

// battery
#include <driver/adc.h>
#include "esp_adc_cal.h"

// deepsleep
#include "esp_sleep.h"

// font
#include "Firasans.h"
#include "MonospaceMedium32.h"
#include "DSEG14_Classic_Regular32.h"
#include "DSEG14_Classic_Regular64.h"
#include "DSEG14_Classic_Regular128.h"

// sailtrack
#include "SailtrackModule.h"

#define BATT_PIN 36

#define WAVEFORM EPD_BUILTIN_WAVEFORM

SailtrackModule monitorModule;

/**
 * Upper most button on side of device. Used to setup as wakeup source to start from deepsleep.
 * Pinout here https://ae01.alicdn.com/kf/H133488d889bd4dd4942fbc1415e0deb1j.jpg
 */
gpio_num_t FIRST_BTN_PIN = GPIO_NUM_39;

EpdiyHighlevelState hl;
// ambient temperature around device
int temperature = 20;
uint8_t *fb;
enum EpdDrawError err;

// CHOOSE HERE YOU IF YOU WANT PORTRAIT OR LANDSCAPE
// both orientations possible
EpdRotation orientation = EPD_ROT_PORTRAIT;
// EpdRotation orientation = EPD_ROT_LANDSCAPE;

int vref = 1100;

/**
 * RTC Memory var to get number of wakeups via wakeup source button
 * For demo purposes of rtc data attr
**/
RTC_DATA_ATTR int pressed_wakeup_btn_index;

/**
 * That's maximum 30 seconds of runtime in microseconds
 */
int64_t maxTimeRunning = 30000000;

double_t get_battery_percentage()
{
    // When reading the battery voltage, POWER_EN must be turned on
    epd_poweron();
    delay(50);

    Serial.println(epd_ambient_temperature());

    uint16_t v = analogRead(BATT_PIN);
    Serial.print("Battery analogRead value is");
    Serial.println(v);
    double_t battery_voltage = ((double_t)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);

    // Better formula needed I suppose
    // experimental super simple percent estimate no lookup anything just divide by 100
    double_t percent_experiment = ((battery_voltage - 3.7) / 0.5) * 100;

    // cap out battery at 100%
    // on charging it spikes higher
    if (percent_experiment > 100)
    {
        percent_experiment = 100;
    }

    String voltage = "Battery Voltage :" + String(battery_voltage) + "V which is around " + String(percent_experiment) + "%";
    Serial.println(voltage);

    epd_poweroff();
    delay(50);

    return percent_experiment;
}

void display_center_message(const char *text)
{
    // first set full screen to white
    epd_hl_set_all_white(&hl);

    int cursor_x = EPD_WIDTH / 2;
    int cursor_y = EPD_HEIGHT / 2;
    if (orientation == EPD_ROT_PORTRAIT)
    {
        // height and width switched here because portrait mode
        cursor_x = EPD_HEIGHT / 2;
        cursor_y = EPD_WIDTH / 2;
    }
    EpdFontProperties font_props = epd_font_properties_default();
    font_props.flags = EPD_DRAW_ALIGN_CENTER;
    epd_write_string(&DSEG14_Classic_Regular64, text, &cursor_x, &cursor_y, fb, &font_props);

    epd_poweron();
    err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
    delay(500);
    epd_poweroff();
    delay(1000);
}

void display_full_screen_left_aligned_text(const char *text)
{
    EpdFontProperties font_props = epd_font_properties_default();
    font_props.flags = EPD_DRAW_ALIGN_CENTER;

    // first set full screen to white
    epd_hl_set_all_white(&hl);

    /************* Display the text itself ******************/
    // hardcoded to start at upper left corner
    // with bit of padding
    int cursor_x = EPD_HEIGHT/2;
    int cursor_y = 200;
    epd_write_string(&DSEG14_Classic_Regular64, text, &cursor_x, &cursor_y, fb, &font_props);
    /********************************************************/

    /************ Battery percentage display ****************/
    // height and width switched here because portrait mode
    int battery_cursor_x = EPD_WIDTH - 30;
    int battery_cursor_y = EPD_HEIGHT - 10;
    if (orientation == EPD_ROT_PORTRAIT)
    {
        // switched x and y constants in portrait mode
        battery_cursor_x = EPD_HEIGHT - 10;
        battery_cursor_y = EPD_WIDTH - 30;
    }
    EpdFontProperties battery_font_props = epd_font_properties_default();
    battery_font_props.flags = EPD_DRAW_ALIGN_RIGHT;
    String battery_text = String(get_battery_percentage());
    battery_text.concat("% Battery");
    epd_write_string(&FiraSans_12, battery_text.c_str(), &battery_cursor_x, &battery_cursor_y, fb, &battery_font_props);
    /********************************************************/

    epd_poweron();
    err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
    delay(500);
    epd_poweroff();
    delay(1000);
}

/**
 * Powers off everything into deepsleep so device and display.
 */
void start_deep_sleep_with_wakeup_sources()
{
    epd_poweroff();
    delay(400);
    esp_sleep_enable_ext0_wakeup(FIRST_BTN_PIN, 0);

    Serial.println("Sending device to deepsleep");
    esp_deep_sleep_start();
}

/**
 * Correct the ADC reference voltage. Was in example of lilygo epd47 repository to calc battery percentage
*/
void correct_adc_reference()
{
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
        vref = adc_chars.vref;
    }
}

void setup()
{
    Serial.begin(115200);

    correct_adc_reference();

    monitorModule.init("sailtrack-monitor");

    // First setup epd to use later
    epd_init(EPD_OPTIONS_DEFAULT);
    hl = epd_hl_init(WAVEFORM);
    epd_set_rotation(orientation);
    fb = epd_hl_get_framebuffer(&hl);
    epd_clear();

    int cursor_x = EPD_HEIGHT/2;
    int cursor_y = DSEG14_Classic_Regular128.ascender + 5;

    EpdRect rect = {.x = DSEG14_Classic_Regular128.ascender + 10, .y = 0, .width = 540, .height = DSEG14_Classic_Regular128.ascender + 15};
    uint8_t color = 0;
    epd_draw_rect(rect, color,fb);

    EpdFontProperties font_props = epd_font_properties_default();
    font_props.flags = EPD_DRAW_ALIGN_CENTER;

    const char *text = "023"; 
    
    epd_write_string(&DSEG14_Classic_Regular64, text, &cursor_x, &cursor_y, fb, &font_props);

    epd_poweron();
    err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
    delay(500);
    epd_poweroff();
}

void loop()
{
    /* 
    * Shutdown device after 30s always to conserve battery.
    * Basically almost no battery usage then until next wakeup.
    */

    // Serial.printf("T0: %d\t", touchRead(T0)); // get value of Touch 0 pin = GPIO 4
    // Serial.printf("T1: %d\t", touchRead(T1));
    // Serial.printf("T2: %d\t", touchRead(T2));
    // Serial.printf("T3: %d\t", touchRead(T3));
    // Serial.printf("T4: %d\t", touchRead(T4));
    // Serial.printf("T5: %d\t", touchRead(T5));
    // Serial.printf("T6: %d\t", touchRead(T6));
    // Serial.printf("T7: %d\t", touchRead(T7));
    // Serial.printf("T8: %d\t", touchRead(T8));
    // Serial.printf("T9: %d\n", touchRead(T9));
    // delay(100);

    // if (esp_timer_get_time() > maxTimeRunning) {
    // Serial.println("Max runtime of 30s reached. Forcing deepsleep now to save energy");
    // display_center_message("Sleeping now.\nWake me up from deepsleep again\nwith the first button on my side");
    // delay(1500);

    // start_deep_sleep_with_wakeup_sources();
    // }
}