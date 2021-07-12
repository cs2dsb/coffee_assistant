#include "esp_adc_cal.h"
#include "driver/adc.h"

#define DEFAULT_VREF    1100
#define ATTEN ADC_0db // This is for the arduino function
static const adc_atten_t atten = ADC_ATTEN_DB_0; // This is for the idf function
static const adc_unit_t unit = ADC_UNIT_1;
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;

#define ADC_WIDTH ADC_WIDTH_12Bit

static const int ADC1_CHAN_PIN_MAP[8] = {
    36, //ADC1_CH0 (SENSOR_VP)
    37, //ADC1_CH1 (SENSOR_CAPP)
    38, //ADC1_CH2 (SENSOR_CAPN)
    39, //ADC1_CH3 (SENSOR_VN)
    32, //ADC1_CH4 (32K_XP)
    33, //ADC1_CH5 (32K_XP)
    34, //ADC1_CH6 (VDET_1)
    35 //ADC1_CH7 (VDET_2)
};

void configure_and_calibrate_adc1(void);
bool configure_pin(int pin);
bool configure_batt_pin(void);
bool configure_vbus_pin(void);
bool configure_analog_pins(void);
uint32_t sample_to_mv(uint32_t sample);
uint32_t sample_pin(int pin);
uint32_t sample_batt_pin(void);
uint32_t sample_vbus_pin(void);
void sample_to_filters(void);
float get_filtered_batt_voltage(void);
float get_filtered_vbus_voltage(void);
