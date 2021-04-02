#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <FS.h>
#define SPIFFS LITTLEFS
#include <LITTLEFS.h>
#include <EEPROM.h>
#include <freertos/queue.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <HX711_ADC.h>

#include "credentials.h"
#include "static/routes.h"
#include "handlers.h"
#include "utils.h"


// The built in static file handler is probably better but
// it doesn't log anything which can be useful to make sure
// clients are really requesting files rather than using cached
#define USE_BUILTIN_STATIC              true
#define WAIT_FOR_SERIAL                 true
#define BAUD                            115200
#define SERIAL_BUF_LEN                  255
//#define WIFI_MODE                     WIFI_AP_STA
#define WIFI_MODE                       WIFI_STA
#define WIFI_POWERSAVING_MODE           WIFI_PS_NONE
#define WAIT_FOR_WIFI                   false
#define WIFI_TIMEOUT_SECONDS            30
#define BLINK_INTERVAL                  1000
#define HX711_POLL_INTERVAL             500
#define WS_MESSAGE_INTERVAL             200
#define SERIAL_LOG_INTERVAL             10000
#define WIFI_STATUS_UPDATE_INTERVAL     1000
#define TIMER_INTERVAL                  100
#define HTTP_PORT                       80
#define DEFAULT_CALIBRATION             3753.989990

#define PIN_TARE                        26
#define PIN_CAL                         27

#define PIN_HX711_DT                    14
#define PIN_HX711_SCK                   12

#define PIN_LED                         21

// EEPROM is actually emulated using a flash 'partition' that's bigger than 512
#define EEPROM_SIZE                     512
#define EEPROM_VAL_VALID_ADDR           0
#define EEPROM_CAL_ADDR                 4
#define EEPROM_VAL_VALID_VALUE          0xCAFEBEEF

// The amount (in calibrated units, grams typically) to wait for to detect the
// start of a weighing event
#define WEIGHT_CHANGE_THRESHOLD         0.3
#define KNOWN_CALIBRATION_MASS_DEFAULT  100.0
#define WEIGHT_HYSTERESIS               0.1
#define WEIGHT_START_DELTA              0.3
#define WEIGHT_EMA_FACTOR_1             0.95
#define WEIGHT_EMA_FACTOR_2             0.9
#define WEIGHT_EMA_FACTOR_3             0.6
#define WEIGHT_EMA_FACTOR_4             0.05
#define WEIGHT_IDLE_TIMEOUT             2.0

// From credentials.h
const char* host            = MDNS_HOST;
const char* ssid            = WIFI_SSID;
const char* password        = WIFI_PASSWORD;

const char* cal_command     = "do-calibrate";
const char* tare_command    = "do-tare";

// How much the calibration weight is
float known_mass            = KNOWN_CALIBRATION_MASS_DEFAULT;

struct measurement {
    float weight;
    float time;
    bool timer_running;
    float ema2;
    float ema3;
};

// Global variables
unsigned long last_blink_millis = 0;
int last_blink_state = LOW;

unsigned long last_weight_poll_millis = 0;
float weight_last_change_time         = 0.0;

float weight_new    = 0.0;
float weight_start  = 0.0;
float weight_peak   = 0.0;
float weight_ema_1  = 0.0;
float weight_ema_2  = 0.0;
float weight_ema_3  = 0.0;
float weight_ema_4  = 0.0;

// last time various periodic functions were run
unsigned long last_serial_log_millis = 0;
unsigned long last_ws_message_millis = 0;
unsigned long last_wifi_status_update_millis = 0;
unsigned long last_elapsed_millis = 0;
unsigned long last_timer_update_millis = 0;
unsigned long last_tare_millis = 0;
unsigned long last_cal_millis = 0;

// elapsed_millis is updated every time loop is called
unsigned long elapsed_millis = 0;
// elapsed_seconds is updated from elapsed_millis every time TIMER_INTERVAL expires
float elapsed_seconds = 0;
// timer is assumed not to be running unless weight is being added to the scale
bool timer_running = false;

wl_status_t last_wifi_state = WL_DISCONNECTED;

bool tare = false;
bool calibrate = false;

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws_server("/ws");

HX711_ADC hx711(PIN_HX711_DT, PIN_HX711_SCK);

void setup_spiffs(void);
void setup_serial(void);
void setup_server(void);
void setup_gpio(void);
void update_elapsed(void);
void setup_hx711(void);
void setup_eeprom(void);
void poll_hx711(void);
void tare_hx711(bool wait);
void poll_wifi_status(void);
void blink(void);
void process_buttons(byte buttons);
bool delay_elapsed(unsigned long *last, unsigned long interval);
void fatal(void);
float get_cal_from_eeprom(void);
void save_cal_to_eeprom(float cal);
void on_websocket_event(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
void reset_timer(void);
void send_websocket_measurement(measurement m);
void update_serial(void);
void tare_isr(void);
void cal_isr(void);

void setup() {
    setup_serial();
    setup_eeprom();
    setup_gpio();
    setup_spiffs();

    setup_hx711();

    setup_server();

    // Wait while server starts up to let the hx711 stabilize before tareing
    tare_hx711(true);
}

void loop() {
    AsyncElegantOTA.loop();
    ws_server.cleanupClients();

    blink();

    poll_hx711();

    update_elapsed();
    update_serial();
    poll_wifi_status();

    delay(1);
}

void setup_serial(void) {
    Serial.begin(BAUD);

    // Wait for serial to connect
    while (WAIT_FOR_SERIAL && !Serial) {
        delay(10);
    }
    if (WAIT_FOR_SERIAL) {
        delay(500);
    }

    serial_printf("%s v%s (%s)\n", PROJECT, VERSION, BUILD_TIMESTAMP);
}

void setup_server(void) {
    WiFi.mode(WIFI_MODE);
    esp_wifi_set_ps(WIFI_POWERSAVING_MODE);

    serial_printf("SSID: '%s'\n", ssid);
    WiFi.begin(ssid, password);

    if (WAIT_FOR_WIFI) {
        int timeout = WIFI_TIMEOUT_SECONDS * 1000;
        wl_status_t status = WiFi.status();
        while(timeout > 0 && (!status || status >= WL_DISCONNECTED)) {
            serial_printf("Waiting for wifi (timeout: %d)\n", timeout);
            delay(500);
            timeout -= 500;
            status = WiFi.status();
        }

        if (status >= WL_DISCONNECTED) {
            serial_printf("Timeout connecting to wifi. Status: %d\n", status);
            fatal();
        } else {
            serial_printf("Connected\n");
        }
    }

    serial_printf("Starting mDNS responder on %s.local\n", host);
    MDNS.begin(host);

    // Configure websockets
    ws_server.onEvent(on_websocket_event);
    server.addHandler(&ws_server);

    // Configure static files
    if (USE_BUILTIN_STATIC) {
        server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    } else {
        register_routes(&server);
    }

    // Configure OTA
    AsyncElegantOTA.begin(&server);

    // 404 handler
    server.onNotFound([](AsyncWebServerRequest *request) {
        serial_printf("GET %s 404\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
    });

    server.begin();
    MDNS.addService("http", "tcp", HTTP_PORT);
    serial_printf("HTTP listening on port %d\n", HTTP_PORT);
}

void setup_gpio(void) {
    pinMode(PIN_LED, OUTPUT);

    pinMode(PIN_TARE, INPUT);
    pinMode(PIN_CAL, INPUT);

    attachInterrupt(digitalPinToInterrupt(PIN_TARE), tare_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_CAL), cal_isr, FALLING);
}

void setup_eeprom(void) {
    EEPROM.begin(EEPROM_SIZE);
}

void setup_hx711(void) {
    float cal = get_cal_from_eeprom();
    hx711.setCalFactor(cal);
    hx711.begin();
    hx711.setSamplesInUse(1);
    hx711.start(0);
    hx711.update();
}

void setup_spiffs(void) {
    if(!SPIFFS.begin()){
        serial_printf("Failed to mount SPIFFS");
        fatal();
    }
}

void tare_hx711(bool wait) {
    hx711.tareNoDelay();
    tare = false;

    bool timeout;
    do {
        hx711.update();
        timeout = hx711.getTareTimeoutFlag()
               || hx711.getSignalTimeoutFlag();
    } while (wait && !hx711.getTareStatus() && !timeout);

    if (timeout) {
        serial_printf("hx711 tare timed out\n");
        fatal();
    }
}

void blink(void) {
    if (delay_elapsed(&last_blink_millis, BLINK_INTERVAL)) {
        if (last_blink_state == LOW) {
          last_blink_state = HIGH;
        } else {
          last_blink_state = LOW;
        }

        digitalWrite(PIN_LED, last_blink_state);
    }
}

void poll_hx711(void) {
    if (hx711.update()) {
        if (delay_elapsed(&last_weight_poll_millis, HX711_POLL_INTERVAL)) {

            if (tare || calibrate) {
                // Reset the timer
                reset_timer();
            }

            if (tare) {
                tare_hx711(false);
            }

            if (hx711.getTareStatus()) {
                serial_printf("tare complete\n");

                // Clear out the averages
                weight_ema_1 = 0.0;
                weight_ema_2 = 0.0;
                weight_ema_3 = 0.0;
                weight_ema_4 = 0.0;

                // Make sure the timer isn't running
                reset_timer();

                ws_server.textAll("{ \"tare\": true }");
            }

            if (calibrate) {
                while (!hx711.update()) {}

                // Clear out existing averaging values
                hx711.setSamplesInUse(16);
                hx711.refreshDataSet();

                float calibration = hx711.getNewCalibration(known_mass);
                hx711.setSamplesInUse(1);
                serial_printf("new calibration: %f\n", calibration);
                save_cal_to_eeprom(calibration);

                char buf[SERIAL_BUF_LEN];
                format(buf, SERIAL_BUF_LEN, "{ \"calibration_value\": %.3f }", calibration);
                ws_server.textAll(buf);

                calibrate = false;
            }

            weight_new = hx711.getData();

            // ema 1 is just a gentle smoothing to remove noise/drips/etc
            weight_ema_1 += (weight_new - weight_ema_1) * WEIGHT_EMA_FACTOR_1;

            // ema 2 and 3 are used to detect if there is a changing weight
            weight_ema_2 += (weight_ema_1 - weight_ema_2) * WEIGHT_EMA_FACTOR_2;
            weight_ema_3 += (weight_ema_1 - weight_ema_3) * WEIGHT_EMA_FACTOR_3;

            // ema 4 is the source for the weight_start value
            weight_ema_4 += (weight_ema_1 - weight_ema_4) * WEIGHT_EMA_FACTOR_4;

            // Only detect increasing weight for starting
            bool start = weight_ema_2 > (weight_ema_3 + WEIGHT_START_DELTA);
            // But any change thereafter should keep the run going (the weight
            // tends to bounce a bit as drips land and cause the scale to wobble)
            bool change = abs(weight_ema_2 - weight_ema_3) > WEIGHT_HYSTERESIS;

            if (!timer_running && start) {
                reset_timer();
                timer_running = true;
                weight_peak = 0.0;

                // More fudging required here perhaps?
                weight_start = weight_ema_4;
            }

            if (timer_running) {
                if (change) {
                    weight_last_change_time = elapsed_seconds;
                    if (weight_ema_1 > weight_peak) {
                        weight_peak = weight_ema_1;
                    }
                } else if (elapsed_seconds - weight_last_change_time > WEIGHT_IDLE_TIMEOUT) {
                    timer_running = false;

                    char buf[SERIAL_BUF_LEN];
                    format(buf, SERIAL_BUF_LEN,
                        "{ \"total_time\": %.1f, \"weight\": %.3f, \"weight_peak\": %.3f, \"weight_start\": %.3f }",
                        weight_last_change_time,
                        weight_ema_1,
                        weight_peak,
                        weight_start
                    );

                    // Skip the slow updating in case a new weight starts imminently
                    weight_ema_4 = weight_ema_1;

                    ws_server.textAll(buf);
                }
            }

            send_websocket_measurement(measurement {
                weight: weight_ema_1,
                time: elapsed_seconds,
                timer_running: timer_running,
                ema2: weight_ema_2,
                ema3: weight_ema_3,
            });
        }
    }
}

void poll_wifi_status(void) {
    if (delay_elapsed(&last_wifi_status_update_millis, WIFI_STATUS_UPDATE_INTERVAL)) {
        wl_status_t status = WiFi.status();
        if (last_wifi_state != status) {
            switch (status) {
                case WL_IDLE_STATUS:
                    serial_printf("WiFi idle\n");
                    break;
                case WL_NO_SSID_AVAIL:
                    serial_printf("WiFi no SSID available\n");
                    break;
                case WL_SCAN_COMPLETED:
                    serial_printf("WiFi scan complete\n");
                    break;
                case WL_CONNECTED:
                    serial_printf("WiFi connected\n");
                    break;
                case WL_CONNECT_FAILED:
                    serial_printf("WiFi connection FAILED\n");
                    break;
                case WL_CONNECTION_LOST:
                    serial_printf("WiFi connection LOST\n");
                    break;
                case WL_DISCONNECTED:
                    serial_printf("WiFi disconnected\n");
                    break;
            }
            last_wifi_state = status;
        }
    }
}

void update_serial(void) {
    if (delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {
        serial_printf("Time: %.1f   Weight: %.1f\n", elapsed_seconds, weight_ema_1);
    }
}

void update_elapsed(void) {
    unsigned long now = millis();
    unsigned long delta = now - last_elapsed_millis;
    last_elapsed_millis = now;

    if (timer_running) {
        elapsed_millis += delta;

        if (delay_elapsed(&last_timer_update_millis, TIMER_INTERVAL)) {
            elapsed_seconds = ((float)elapsed_millis) / 1000.0;
        }
    }
}

void process_buttons(byte buttons) {
    if ((buttons >> 7) & 1 == 1) {
        tare = true;
    }
    if ((buttons >> 6) & 1 == 1) {
        calibrate = true;
    }
    if ((buttons >> 5) & 1 == 1) {
        elapsed_millis = 0;
    }
}


float get_cal_from_eeprom(void) {
    unsigned is_valid;
    EEPROM.get(EEPROM_VAL_VALID_ADDR, is_valid);

    float cal = DEFAULT_CALIBRATION;

    if (is_valid == EEPROM_VAL_VALID_VALUE) {
        EEPROM.get(EEPROM_CAL_ADDR, cal);
    }

    return cal;
}

void save_cal_to_eeprom(float cal) {
    unsigned is_valid = EEPROM_VAL_VALID_VALUE;

    EEPROM.put(EEPROM_CAL_ADDR, cal);
    EEPROM.put(EEPROM_VAL_VALID_ADDR, is_valid);
    EEPROM.commit();
}

bool is_command(const char* command, char* data) {
    size_t len = strlen(command);
    bool same = strncmp(command, data, len) == 0;
    // serial_printf("Comparing first %d chars of '%s' and '%s' = %s\n",
    //     len,
    //     command,
    //     data,
    //     same? "true" : "false"
    // );
    return same;
}

void on_websocket_event(
    AsyncWebSocket * server,
    AsyncWebSocketClient * client,
    AwsEventType type,
    void* arg,
    uint8_t *data,
    size_t len
) {
    unsigned id = client->id();
    if (type == WS_EVT_CONNECT) {
        serial_printf("WS (id: %u) CONNECTED\n", id);
    } else if (type == WS_EVT_DISCONNECT) {
        serial_printf("WS (id: %u) DISCONNECTED\n", id);
    } else if (type == WS_EVT_ERROR) {
        serial_printf("WS (id: %u) ERROR (%u). DATA: %s\n", id, *((uint16_t*)arg), (char*)data);
    } else if (type == WS_EVT_PONG) {
        serial_printf("WS (id: %u) PONG\n", id);
    } else if (type == WS_EVT_DATA) {
        serial_printf("WS (id: %u) DATA: %s\n", id, len > 0 ? (char*) data: "");

        if (is_command(cal_command, (char*)data)) {
            serial_printf("WS (id: %u) requested CALIBRATION\n", id);
            calibrate = true;
        } else if (is_command(tare_command, (char*)data)) {
            serial_printf("WS (id: %u) requested TARE\n", id);
            tare = true;
        }
    }
}

void reset_timer(void) {
    elapsed_millis = 0;
    elapsed_seconds = 0;
    timer_running = false;
}

void send_websocket_measurement(measurement m) {
    if (delay_elapsed(&last_ws_message_millis, WS_MESSAGE_INTERVAL)) {
        char buf[SERIAL_BUF_LEN];
        format(buf, SERIAL_BUF_LEN,
            "{ \"time\": %.1f, \"weight\": %.3f, \"timer_running\": %s, \"ema2\": %.3f, \"ema3\": %.3f }",

            m.time,
            m.weight,
            timer_running ? "true" : "false",
            m.ema2,
            m.ema3
        );
        ws_server.textAll(buf);
    }
}

void tare_isr(void) {
    unsigned long now = millis();
    if (now - last_tare_millis > 1000) {
        last_tare_millis = now;
        tare = true;
    }
}

void cal_isr(void) {
    unsigned long now = millis();
    if (now - last_cal_millis > 1000) {
        last_cal_millis = now;
        calibrate = true;
    }
}
