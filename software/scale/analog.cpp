#include "analog.h"
#include "board.h"
#include "utils.h"

esp_adc_cal_characteristics_t adc_characteristics;
Filter batt_filter = Filter();
Filter vbus_filter = Filter();

void configure_and_calibrate_adc1(void) {
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_characteristics);

    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        serial_printf("ADC1 CAL: eFuse Vref\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        serial_printf("ADC1 CAL: Two Point\n");
    } else {
        serial_printf("ADC1 CAL: no cal in fuses\n");
    }

    adc1_config_width(ADC_WIDTH);
}

bool configure_pin(int pin) {
    pinMode(pin, ANALOG);

    int chan = -1;
    for (int i = 0; i < sizeof(ADC1_CHAN_PIN_MAP)/sizeof(ADC1_CHAN_PIN_MAP[0]); i++) {
        if (ADC1_CHAN_PIN_MAP[i] == pin) {
            chan = i;
            break;
        }
    }

    if (chan == -1) {
        serial_printf("ADC1 CAL: failed to find channel for pin %d\n", pin);
        return false;
    }

    adc1_config_channel_atten((adc1_channel_t)chan, ADC_ATTEN_DB_11);
    return true;
}

bool configure_batt_pin(void) {
    return configure_pin(PIN_ANALOG_BATT);
}

bool configure_vbus_pin(void) {
    return configure_pin(PIN_ANALOG_VBUS);
}

bool configure_analog_pins(void) {
    return configure_batt_pin() && configure_vbus_pin();
}

uint32_t sample_to_mv(uint32_t sample) {
    return esp_adc_cal_raw_to_voltage(sample, &adc_characteristics);
}

uint32_t sample_pin(int pin) {
    return analogRead(pin);
}

uint32_t sample_batt_pin(void) {
    return sample_pin(PIN_ANALOG_BATT);
}

uint32_t sample_vbus_pin(void) {
    return sample_pin(PIN_ANALOG_VBUS);
}

void sample_to_filters(void) {
    batt_filter.push((float)sample_to_mv(sample_batt_pin()));
    vbus_filter.push((float)sample_to_mv(sample_vbus_pin()));
}

float get_filtered_batt_voltage(void) {
    float pin_v = batt_filter.value() / 1000.0;
    return (pin_v  * (BATT_DIVIDER_R1 + BATT_DIVIDER_R2)) / BATT_DIVIDER_R2;
}

float get_filtered_vbus_voltage(void) {
    float pin_v = vbus_filter.value() / 1000.0;
    return (pin_v  * (BATT_DIVIDER_R1 + BATT_DIVIDER_R2)) / BATT_DIVIDER_R2;
}


