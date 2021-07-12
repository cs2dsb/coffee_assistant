#include "utils.h"

#define TRACE_NOW                       true
#define TRACE_WS                        false

// The built in static file handler is probably better but
// it doesn't log anything which can be useful to make sure
// clients are really requesting files rather than using cached
#define USE_BUILTIN_STATIC              true
#define WAIT_FOR_SERIAL                 true
#define BAUD                            115200
#define CPU_MHZ                         240
#define SERIAL_BUF_LEN                  512
#define MQTT_TIMEOUT_SECONDS            30
#define WAIT_FOR_MQTT                   false
#define BLINK_INTERVAL                  1000
#define HX711_POLL_INTERVAL             500
#define TM1638_POLL_INTERVAL            100
#define TM1638_UPDATE_INTERVAL          500
#define BUTTON_UPDATE_INTERVAL          15
#define BUTTON_PRESS_INTERVAL           80
#define BUTTON_LONG_PRESS_INTERVAL      1500
#define TM1638_HIGH_FREQ                true
#define WS_MESSAGE_INTERVAL             200
#define SERIAL_LOG_INTERVAL             200
#define WIFI_STATUS_UPDATE_INTERVAL     1000
#define TIMER_INTERVAL                  100
#define HTTP_PORT                       80
#define DEFAULT_CALIBRATION             3300
#define TOUCH_THRESHOLD                 50
#define BATT_BROADCAST_INTERVAL         10000
#define DEEP_SLEEP_SECONDS              60*60
// This is how often now will look for the relay
#define NOW_UPDATE_INTERVAL             2000

//#define DEFAULT_WIFI_MODE                     WIFI_MODE_AP_STA
#define DEFAULT_WIFI_MODE               WIFI_MODE_STA
#define DEFAULT_WIFI_POWERSAVING_MODE   WIFI_PS_NONE
//#define DEFAULT_WIFI_POWERSAVING_MODE           WIFI_PS_MAX_MODEM
#define WIFI_TIMEOUT_SECONDS            30
#define DHCP_RENEW_INTERVAL             7*24*60*60 / DEEP_SLEEP_SECONDS

#define DEFAULT_WIFI_PROMISCUOUS_MODE   false

#define SERIAL_BUF_LEN                  255
#define JSON_BUF_LEN                    512


// For debugging, if this is set to true the eeprom will be cleared during read operations
#define WIPE_EEPROM                     true

// EEPROM is actually emulated using a flash 'partition' that's bigger than 512
#define EEPROM_SIZE                     512
#define EEPROM_VAL_VALID_ADDR           0
#define EEPROM_VAL_VALID_VALUE          0xCAFEBEEF
#define EEPROM_CAL_ADDR                 (EEPROM_VAL_VALID_ADDR + sizeof(unsigned))
#define EEPROM_CHANNEL_ADDR             (EEPROM_CAL_ADDR + sizeof(float))
#define EEPROM_CHANNEL_LOCKED_ADDR      (EEPROM_CHANNEL_ADDR + sizeof(channel_t))
#define EEPROM_BRIDGE_MAC_ADDR          (EEPROM_CHANNEL_LOCKED_ADDR + sizeof(channel_locked_t))

// The amount (in calibrated units, grams typically) to wait for to detect the
// start of a weighing event
#define WEIGHT_CHANGE_THRESHOLD         0.3
#define KNOWN_CALIBRATION_MASS_DEFAULT  100.0
#define WEIGHT_HYSTERESIS               0.1
#define WEIGHT_START_DELTA              0.2
#define WEIGHT_EMA_FACTOR_1             0.99
#define WEIGHT_EMA_FACTOR_2             0.9
#define WEIGHT_EMA_FACTOR_3             0.6
#define WEIGHT_EMA_FACTOR_4             0.05
#define WEIGHT_IDLE_TIMEOUT             2.0

#define MQTT_TOPIC                      PROJECT
