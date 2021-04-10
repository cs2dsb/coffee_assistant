#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <FS.h>
#define SPIFFS LITTLEFS
#include <LITTLEFS.h>


#include <PubSubClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

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
#define WIFI_MODE                       WIFI_STA
#define WIFI_POWERSAVING_MODE           WIFI_PS_MIN_MODEM//WIFI_PS_NONE
#define WAIT_FOR_WIFI                   true
#define WIFI_TIMEOUT_SECONDS            30
#define WS_MESSAGE_INTERVAL             200
#define SERIAL_LOG_INTERVAL             10000
#define WIFI_STATUS_UPDATE_INTERVAL     1000
#define HTTP_PORT                       80

#define PIN_VBUS                        34
#define VBUS_VD_R1                      4700 + 4700 + 1000 + 1000
#define VBUS_VD_R2                      22000
#define VBUS_VD_RT                      (VBUS_VD_R1 + VBUS_VD_R2)
// The resistors and everything else involved are low precision, this factor
// is from using a DMM and comparing it to the reading the ESP is giving
#define VBUS_FUDGE_FACTOR               0.9573
#define VBUS_DELAY                      1000
#define VBUS_SAMPLES                    10
#define VBUS_DELAY_BETWEEN_SAMPLES      100

// Go to sleep after sending a packet?
#define SLEEP                           true
#define USE_WEBSOCKET                   false
#define USE_HTTP_SERVER                 false

// From credentials.h
const char* host                = MDNS_HOST;
const char* ssid                = WIFI_SSID;
const char* password            = WIFI_PASSWORD;

const char* heartbeat_command   = "heartbeat";

const char* mqtt_id             = "esp32-bat-test";
const char* mqtt_topic          = "battery";
int mqtt_port                   = 1883;


uint16_t samples[VBUS_SAMPLES];

IPAddress mqtt_broker(192, 168, 0, 34);


struct measurement {
    float voltage;
};

// last time various periodic functions were run
unsigned long last_serial_log_millis = 0;
unsigned long last_ws_message_millis = 0;
unsigned long last_wifi_status_update_millis = 0;

wl_status_t last_wifi_state = WL_DISCONNECTED;

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws_server("/ws");

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

void setup_spiffs(void);
void setup_serial(void);
void setup_wifi(void);
void setup_server(void);
void setup_gpio(void);
void setup_mqtt(void);

void poll_wifi_status(void);
void on_websocket_event(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
void send_websocket_measurement(measurement m);
void update_serial(void);
void client_connect(void);
void poll_mqtt(void);
void collect_samples(void);

void setup() {
    setup_gpio();

    if (SLEEP) {
        delay(VBUS_DELAY);
    }
    collect_samples();

    setup_serial();
    setup_spiffs();
    setup_wifi();
    if (USE_HTTP_SERVER) {
        setup_server();
    }
    setup_mqtt();
}

void loop() {
    if (USE_HTTP_SERVER) {
        AsyncElegantOTA.loop();
    }
    if (USE_HTTP_SERVER && USE_WEBSOCKET) {
        ws_server.cleanupClients();
    }

    update_serial();
    //poll_wifi_status();
    poll_mqtt();

    if (SLEEP) {
        serial_printf("flushing before sleep\n");
        mqtt_client.disconnect();
        while (mqtt_client.state() != MQTT_DISCONNECTED){
            delay(10);
        }
        wifi_client.flush();
        wifi_client.stop();
        while (wifi_client.connected()) {
            delay(10);
        }
        // You'd think flush & stop would be sufficient but apparently not
        delay(200);

        serial_printf("sleeping now\n");
        esp_wifi_stop();
        esp_sleep_enable_timer_wakeup(SERIAL_LOG_INTERVAL * 1000);
        esp_deep_sleep_start();
    } else {
        delay(1);
    }
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

void setup_wifi(void) {
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
}

void setup_server(void) {
    char buf[SERIAL_BUF_LEN];
    format(buf, SERIAL_BUF_LEN, "%s-%s", host, PROJECT);
    serial_printf("Starting mDNS responder on %s.local\n", buf);
    MDNS.begin(buf);

    // Configure websockets
    if (USE_WEBSOCKET) {
        ws_server.onEvent(on_websocket_event);
        server.addHandler(&ws_server);
    }

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
    pinMode(PIN_VBUS, INPUT);
}

void setup_spiffs(void) {
    if(!SPIFFS.begin()){
        serial_printf("Failed to mount SPIFFS");
        fatal();
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

float sample_to_voltage(float sample, bool fudge) {
    int max_count = (1 << 12) - 1;
    float max_count_f = (float)max_count;
    float vcc = 3.3;

    float v_adc = vcc * sample / max_count_f;

    float v = v_adc * VBUS_VD_RT / VBUS_VD_R2;

    if (fudge) {
        v *= VBUS_FUDGE_FACTOR;
    }
    return v;
}

void collect_samples(void) {
    for (int i = 0; i <= VBUS_SAMPLES; i++) {
        delay(VBUS_DELAY_BETWEEN_SAMPLES);
        samples[i] = analogRead(PIN_VBUS);
    }
}

void update_serial(void) {
    if (SLEEP || delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {

        while (!mqtt_client_connect()) { delay(100); };
        char buf[SERIAL_BUF_LEN];

        if (SLEEP) {
            delay(VBUS_DELAY);
        }

        float count = 0.0;
        for (int i = 0; i < VBUS_SAMPLES; i++) {
            float sample = samples[i];

            serial_printf("sample %d: %.2f\n", i, sample);
            count += sample;

            float unfudged = sample_to_voltage(sample, false);
            float fudged = sample_to_voltage(sample, true);

            format(buf, SERIAL_BUF_LEN,
                "{ \"sleep\": %s, \"sample\": %.0f, \"unfudged\": %.3f, \"fudged\": %.3f }",
                SLEEP ? "true" : "false",
                sample,
                unfudged,
                fudged
            );

            while (!mqtt_client.publish(mqtt_topic, buf)) {
                serial_printf("publish failed. retrying\n");
                delay(500);
            }
        }
        count /= (float)VBUS_SAMPLES;

        int max_count = (1 << 12) - 1;
        float max_count_f = (float)max_count;
        float vcc = 3.3;

        float v_adc = vcc * count / max_count_f;

        float v = v_adc * VBUS_VD_RT / VBUS_VD_R2;
        v *= VBUS_FUDGE_FACTOR;

        serial_printf("Count: %.2f, max_count: %.2f, V: %.2f\n", count, max_count_f, v);

        measurement m = measurement { voltage: v };

        if (USE_WEBSOCKET) {
            send_websocket_measurement(m);
        }

        format(buf, SERIAL_BUF_LEN,
            "{ \"voltage\": %.3f, \"sleep\": %s }",
            m.voltage,
            SLEEP ? "true" : "false"
        );

        while (!mqtt_client.publish(mqtt_topic, buf)) {
            serial_printf("publish failed. retrying\n");
            delay(500);
        }
    }
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

        if (is_command(heartbeat_command, (char*)data)) {
            ws_server.textAll(heartbeat_command);
        }
    }
}

void send_websocket_measurement(measurement m) {
    if (delay_elapsed(&last_ws_message_millis, WS_MESSAGE_INTERVAL)) {
        char buf[SERIAL_BUF_LEN];
        format(buf, SERIAL_BUF_LEN,
            "{ \"voltage\": %.3f }",
            m.voltage
        );
        ws_server.textAll(buf);
    }
}

bool mqtt_client_connect(void) {
    if (!mqtt_client.connected()) {
        serial_printf("Attempting MQTT connection...\n");
        if (mqtt_client.connect(mqtt_id)) {
            serial_printf("connected\n");
        } else {
            serial_printf("connection failed\n");
            return false;
        }
    }
    return true;
}

void setup_mqtt(void) {
    mqtt_client.setServer(mqtt_broker, mqtt_port);
}

void poll_mqtt(void) {
    if (mqtt_client_connect()) {
        mqtt_client.loop();
    }
}
