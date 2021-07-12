#include <stdlib.h>
#include <stdarg.h>
#include <ESPmDNS.h>
#include <FS.h>
#define SPIFFS LITTLEFS
#include <LITTLEFS.h>
#include <EEPROM.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include "credentials.h"
#include "static/routes.h"
#include "handlers.h"
#include "utils.h"
#include "board.h"
#include "analog.h"
#include "wifi.h"
#include "mqtt.h"
#include "config.h"
#include "buttons.h";
#include "now.h";
#include "messages.h";

char now_buf[ESP_NOW_MAX_DATA_LEN + 1];
char mac_buf[18];
char json_buf[JSON_BUF_LEN];
char format_buf[SERIAL_BUF_LEN];

// last time various periodic functions were run
unsigned long last_serial_log_millis = 0;
unsigned long last_ws_message_millis = 0;
unsigned long last_wifi_status_update_millis = 0;
unsigned long last_elapsed_millis = 0;
unsigned long last_timer_update_millis = 0;
unsigned long last_shot_button_update_millis = 0;
unsigned long last_extraction_update_millis = 0;
unsigned long shot_button_enable_millis = 0;


// elapsed_millis is updated every time loop is called
unsigned long elapsed_millis = 0;
// elapsed_seconds is updated from elapsed_millis every time TIMER_INTERVAL expires
float elapsed_seconds = 0;
// timer is assumed not to be running unless weight is being added to the scale
bool timer_running = false;

bool last_shot_button_state = false;
Filter weight_filter = Filter(0.999);
Filter weight_change_filter = Filter(0.99, false);

#define STATE_IDLE                              0
#define STATE_START                             1
#define STATE_WAITING_FOR_SCALE_READY           2
#define STATE_PREINFUSION                       3
#define STATE_EXTRACTING                        4
#define STATE_DRIPPING                          5

// This is time will override any weight logic and stop extraction if it takes too long
#define EXTRACTION_MAX_TIME                     55
#define EXTRACTION_EARLY_STOP_MG                2000
volatile int state                = STATE_IDLE;
volatile int preinfusion_ms       = 0;
volatile int extraction_target_mg = 0;
volatile int last_weight_mg       = 0;

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws_server("/ws");

void setup() {
    setup_serial();
    //read_wakeup_reason();

    configure_general();
    configure_and_calibrate_adc1();

    setup_gpio();

    setup_eeprom();
    setup_spiffs();


    unsigned long start = millis();
    unsigned long last = start;

    configure_wifi(WIFI_SSID, WIFI_PASSWORD);
    last = log_time(last, "configure_wifi");

    fatal_on_fail(configure_now(false, true), "failed configuring now");
    now_set_on_receive(&now_on_receive);
    last = log_time(last, "configure_now");

    setup_server();
    last = log_time(last, "setup_server");

    configure_mqtt(MQTT_URL);
    last = log_time(last, "configure_mqtt");

    fatal_on_fail(wait_for_connection(), "failed to connect to wifi after timeout");
    last = log_time(last, "wait_for_connection");

    connect_mqtt();
    last = log_time(last, "connect_mqtt2");

    send_mqtt_data();
    last = log_time(last, "send_mqtt_data");

    wait_for_outstanding_publishes();
    last = log_time(last, "outstanding_mqtt_publish_count > 0");

    unsigned long delta = millis() - start;

    serial_printf("connect+send time = %.2f\n", ((float)delta)/1000.);
    //sleep();
}

void loop() {
    AsyncElegantOTA.loop();
    ws_server.cleanupClients();

    update_elapsed();
    update_serial();
    sample_to_filters();
    print_wifi_status();
    update_extraction_state();

    delay(1);
}

void sleep(void) {
    disable_wifi();

    serial_printf("sleeping now\n");
    esp_sleep_enable_touchpad_wakeup();
    uint64_t sleep_us = DEEP_SLEEP_SECONDS * 1000 * 1000ULL;
    esp_sleep_enable_timer_wakeup(sleep_us);
    esp_deep_sleep_start();
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
    setCpuFrequencyMhz(CPU_MHZ);
    serial_printf("CPU running at %lu\n", getCpuFrequencyMhz());
}

void setup_server(void) {
    serial_printf("Starting mDNS responder on %s.local\n", MDNS_HOST);
    MDNS.begin(MDNS_HOST);

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
    pinMode(PIN_SHOT_BUTTON, INPUT);
    ledcSetup(0, 2000, 8);
    ledcAttachPin(14, 0);
    ledcWrite(0, 0);
}

void set_shot_button(bool on) {
    if (on) {
        ledcWrite(0, 50);
        if (!last_shot_button_state) {
            last_shot_button_state = true;
            serial_printf("Pressing shot button\n");
            now_broadcast("PRESSING SHOT BUTTON");
        }
        digitalWrite(PIN_SHOT_BUTTON, HIGH);
        pinMode(PIN_SHOT_BUTTON, OUTPUT);
        digitalWrite(PIN_SHOT_BUTTON, HIGH);
    } else {
        ledcWrite(0, 0);
        if (last_shot_button_state) {
            last_shot_button_state = false;
            serial_printf("Releasing shot button\n");
            now_broadcast("RELEASEING SHOT BUTTON");
        }
        pinMode(PIN_SHOT_BUTTON, INPUT);
    }
}

void setup_spiffs(void) {
    if(!SPIFFS.begin()){
        serial_printf("Failed to mount SPIFFS");
        fatal();
    }
}

void update_serial(void) {
    if (delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {
        //serial_printf("Boop\n");

        // int state = digitalRead(PIN_SHOT_BUTTON);

        // char buf[SERIAL_BUF_LEN];
        // format(buf, SERIAL_BUF_LEN,
        //     "{ \"shot_button_state\": %d }",
        //     state
        // );
        // ws_server.textAll(buf);
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

void read_wakeup_reason(){
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
        default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
    }
}

// Sends a message to the scale to tare itself IFF the weight filter doesn't indicate it's already 0
bool tare_scale() {
    float w = weight_filter.value();
    if (weight_filter.count() > 10 && w > -0.3 && w < 0.3) {
        serial_printf("Scale already tared\n");
        return false;
    } else {
        now_broadcast(TARE_COMMAND_PREFIX);
        return true;
    }
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
    if (type == WS_EVT_CONNECT && TRACE_WS) {
        serial_printf("WS (id: %u) CONNECTED\n", id);
    } else if (type == WS_EVT_DISCONNECT && TRACE_WS) {
        serial_printf("WS (id: %u) DISCONNECTED\n", id);
    } else if (type == WS_EVT_ERROR && TRACE_WS) {
        serial_printf("WS (id: %u) ERROR (%u). DATA: %s\n", id, *((uint16_t*)arg), (char*)data);
    } else if (type == WS_EVT_PONG && TRACE_WS) {
        serial_printf("WS (id: %u) PONG\n", id);
    } else if (type == WS_EVT_DATA) {
        if (TRACE_WS) {
            serial_printf("WS (id: %u) DATA: %s\n", id, len > 0 ? (char*) data: "");
        }

        if (has_prefix((char*)data, TARE_COMMAND_PREFIX)) {
            tare_scale();
        } else if (state == STATE_IDLE && has_prefix((char*)data, EXTRACT_COMMAND_PREFIX)) {
            int i = strlen(EXTRACT_COMMAND_PREFIX);

            auto preinfusion_part = &data[i];
            int preinfusion = atoi((const char*)preinfusion_part);

            bool found = false;
            for (; i < strlen((const char*)data); i += 1) {
                if (data[i] == ':') {
                    found = true;
                    break;
                }
            }

            if (!found) {
                serial_printf("ERROR: failed to find second ':' in extract command: %s\n", data);
            } else {
                i += 1;
                auto weight_part = &data[i];
                int weight = atoi((const char*)weight_part);
                serial_printf("i: %dnpp: %s\nwp: %s\n%d\n%d\n", i, preinfusion_part, weight_part, preinfusion, weight);

                if (preinfusion < 0) {
                    ws_server.textAll("{ \"error\": \"Negative preinfusion is invalid\" }");
                } else if (weight < 10) {
                    ws_server.textAll("{ \"error\": \"Target weight < 10 is invalid\" }");
                } else {
                    preinfusion_ms = preinfusion;
                    extraction_target_mg = weight;
                    state = STATE_START;
                    ws_server.textAll("{ \"state\": \"Starting\" }");
                }
            }
        } else if (has_prefix((const char*)data, HEARTBEAT_COMMAND_PREFIX)) {
            // TODO: just respond to sender
            if (TRACE_WS) {
                serial_printf("WS: replying to heartbeat\n");
            }
            ws_server.textAll(HEARTBEAT_COMMAND_PREFIX);
        }
    }
}

void update_extraction_state(void) {
    auto now = millis();
    auto delta = now - last_extraction_update_millis;
    last_extraction_update_millis = now;

    switch (state) {
        case STATE_START:
            if (tare_scale()) {
                state = STATE_WAITING_FOR_SCALE_READY;
                ws_server.textAll("{ \"state\": \"Synching scale\" }");
            } else {
                state = STATE_PREINFUSION;
                ws_server.textAll("{ \"state\": \"Preinfusing\" }");
            }
            break;
        case STATE_PREINFUSION:
            {
                auto prev = preinfusion_ms;
                if (preinfusion_ms <= 0) {
                    preinfusion_ms = 0;
                } else {
                    preinfusion_ms -= delta;
                }

                if (preinfusion_ms <= now_latency() && prev > now_latency()) {
                    serial_printf("Starting scale timer\n");
                    now_broadcast(START_TIMER_PREFIX);
                }
                set_shot_button(preinfusion_ms > 0);

                if (preinfusion_ms == 0) {
                    state = STATE_EXTRACTING;
                    ws_server.textAll("{ \"state\": \"Extracting\" }");
                    shot_button_enable_millis = 0;
                    reset_timer();
                    timer_running = true;
                    serial_printf("Extracting\n");
                }
            }
            break;
        case STATE_EXTRACTING:
            if (elapsed_seconds > EXTRACTION_MAX_TIME || last_weight_mg + EXTRACTION_EARLY_STOP_MG >= extraction_target_mg) {
                if (shot_button_enable_millis == 0) {
                    shot_button_enable_millis = millis() + 200;
                } else if (shot_button_enable_millis < millis()) {
                    serial_printf("Dripping\n");
                    state = STATE_DRIPPING;
                    ws_server.textAll("{ \"state\": \"Dripping\" }");
                    timer_running = false;
                }
                set_shot_button(shot_button_enable_millis > 0 && shot_button_enable_millis > millis());
            }
            break;
        case STATE_DRIPPING:
            {
                auto c = weight_change_filter.value();
                if (c > -0.1 && c < 0.1) {
                    state = STATE_IDLE;
                    ws_server.textAll("{ \"state\": \"Idle\" }");
                    now_broadcast(STOP_TIMER_PREFIX);
                }
            }
            break;
        default:
            reset_timer();
    }
}

void reset_timer(void) {
    elapsed_millis = 0;
    elapsed_seconds = 0;
}

void send_mqtt_data(void) {
    float batt_v = get_filtered_batt_voltage();
    float vbus_v = get_filtered_vbus_voltage();

    format(format_buf, SERIAL_BUF_LEN,
        "{ \"project\": \"%s\", \"build:\": \"%s\", \"mac\": \"%s\", \"battery\": %.2f, \"vbus\": %.2f }",
        PROJECT,
        BUILD_TIMESTAMP,
        mac_address(),
        batt_v,
        vbus_v
    );

    serial_printf("sending batt_v: %.2f\n", batt_v);
    serial_printf("%s\n", format_buf);

    mqtt_publish(MQTT_TOPIC, format_buf);
}

void now_on_receive(const uint8_t *mac, const uint8_t *data, int len) {
    strncpy(now_buf, (const char *)data, len);

    now_buf[len] = '\0';

    format_mac(mac, mac_buf, sizeof(mac_buf));

    //ws_server.textAll(now_buf);

    if (is_weight_message((const char*) now_buf)) {
        weight_message m;
        if (extract_weight_message((const char*) now_buf, &m)) {
            printf("WEIGHT: %dmg, TIME: %dms\n", m.weight_mg, m.time_ms);
            float weight_g = ((float)m.weight_mg)/1000;
            format(json_buf, JSON_BUF_LEN,
                "{ \"weight\": %.2f, \"time\": %.2f }",
                weight_g,
                ((float)m.time_ms)/1000);
            last_weight_mg = m.weight_mg;

            auto prev = weight_filter.value();
            weight_filter.push(weight_g);
            weight_change_filter.push(weight_g - prev);
            printf("c: %.2f, p: %.2f\n", weight_change_filter.value(), prev);
        } else {
            printf("FAILED to extract weight and time from message: %s\n", now_buf);
        }
    } else if (has_prefix((const char*) now_buf, TARE_COMMAND_PREFIX)) {
        printf("scale tared\n");
        weight_filter.reset(0.0);
        if (state == STATE_WAITING_FOR_SCALE_READY) {
            state = STATE_PREINFUSION;
            ws_server.textAll("{ \"state\": \"Preinfusing\" }");
        }
    }

    if (strlen(json_buf) > 0) {
        ws_server.textAll(json_buf);
    }
}
