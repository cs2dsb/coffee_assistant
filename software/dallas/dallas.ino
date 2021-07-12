#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <PubSubClient.h>

#include "credentials.h"
#include "utils.h"

#define WAIT_FOR_SERIAL                 true
#define BAUD                            115200
#define SERIAL_BUF_LEN                  255
#define WIFI_MODE                       WIFI_STA
#define WIFI_POWERSAVING_MODE           WIFI_PS_MIN_MODEM//WIFI_PS_NONE
#define WAIT_FOR_WIFI                   true
#define WIFI_TIMEOUT_SECONDS            30
#define MQTT_TIMEOUT_SECONDS            30
#define WAIT_FOR_MQTT                   true

#define PROMISCUOUS_MODE                true

#define SERIAL_LOG_INTERVAL             (5*60*1000)
#define WIFI_STATUS_UPDATE_INTERVAL     1000

#define DALLAS_PIN                      16
#define DALLAS_RESOLUTION               9
#define DALLAS_DELAY                    (750 / (1 << (12 - DALLAS_RESOLUTION)))

#define SLEEP                           true

// From credentials.h
const char* host                = MDNS_HOST;
const char* ssid                = WIFI_SSID;
const char* password            = WIFI_PASSWORD;
IPAddress mqtt_broker(MQTT_IP);

const char* mqtt_id             = PROJECT;
const char* mqtt_topic          = PROJECT;


// last time various periodic functions were run
unsigned long last_serial_log_millis = 0;
unsigned long last_wifi_status_update_millis = 0;

unsigned long last_dallas_millis = 0;

wl_status_t last_wifi_state = WL_DISCONNECTED;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
OneWire one_wire(DALLAS_PIN);
DallasTemperature dallas(&one_wire);

void setup_serial(void);
void setup_wifi(void);
void setup_mqtt(void);
void setup_dallas(void);

void poll_wifi_status(void);
void update_serial(void);
bool mqtt_client_connect(void);
void poll_mqtt(void);
void poll_dallas(void);


void setup() {
    setup_dallas();
    poll_dallas();

    setup_serial();
    setup_wifi();
    setup_mqtt();
}

void loop() {
    if (!SLEEP) {
        poll_dallas();
    }

    update_serial();
    poll_wifi_status();
    poll_mqtt();
    send_sample();

    if (SLEEP) {
        wifi_client.flush();
        mqtt_client.disconnect();
        delay(200);

        serial_printf("sleeping now\n");
        esp_wifi_stop();
        esp_sleep_enable_timer_wakeup(SERIAL_LOG_INTERVAL * 1000);
        esp_deep_sleep_start();
    } else {
        delay(1);
    }
}

void setup_dallas(void) {
    dallas.begin();
    dallas.setWaitForConversion(false);

    DeviceAddress addr;
    dallas.getAddress(addr, 0);
    dallas.setResolution(addr, DALLAS_RESOLUTION);
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

    serial_printf("%s v%s (%s). mac: %s\n",
        PROJECT,
        VERSION,
        BUILD_TIMESTAMP,
        WiFi.macAddress().c_str()
    );
}

void setup_wifi(void) {
    WiFi.mode(WIFI_MODE);
    esp_wifi_set_promiscuous(PROMISCUOUS_MODE);
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
            serial_printf("WiFi connected\n");
        }
    }
}

void poll_dallas(void) {
    dallas.requestTemperatures();
    last_dallas_millis = millis();
}

void wait_for_dallas(void) {
    while (millis() - last_dallas_millis < DALLAS_DELAY) {}
}

void send_sample(void) {
    wait_for_dallas();

    float temp_c = dallas.getTempCByIndex(0);
    bool error = temp_c == DEVICE_DISCONNECTED_C;
    if (error) {
        serial_printf("Failed to read temp\n");
    }

    char mqtt_buf[SERIAL_BUF_LEN];
    format(mqtt_buf, SERIAL_BUF_LEN,
        "{ \"project\": \"%s\", \"mac\": \"%s\", \"temperature\": %.2f, \"error\": %s }",
        PROJECT,
        WiFi.macAddress().c_str(),
        temp_c,
        error ? "true" : "false"
    );

    if (mqtt_client_connect() || !WAIT_FOR_MQTT) {
        while (!mqtt_client.publish(mqtt_topic, mqtt_buf) && WAIT_FOR_MQTT) {
            serial_printf("mqtt publish failed. retrying\n");
            delay(500);
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
                    serial_printf("WiFi connected. Channel: %d\n",
                        WiFi.channel());
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
        serial_printf("boop\n");

        float temp_c = dallas.getTempCByIndex(0);
        if (temp_c != DEVICE_DISCONNECTED_C) {
            serial_printf("Temperature: %.2f\n", temp_c);
        } else {
            serial_printf("Failed to read temp\n");
        }
        // char buf[SERIAL_BUF_LEN];

        // format(buf, SERIAL_BUF_LEN,
        //     "{ \"sleep\": %s, \"project\": \"%s\", \"action\": \"boop\", \"mac\": \"%s\" }",
        //     SLEEP ? "true" : "false",
        //     PROJECT,
        //     WiFi.macAddress().c_str()
        // );

        // if (DO_MQTT) {
        //     while (!mqtt_client_connect()) { delay(100); };
        //     while (!mqtt_client.publish(mqtt_topic, buf)) {
        //         serial_printf("publish failed. retrying\n");
        //         delay(500);
        //     }
        // }

        // broadcast(buf);
    }
}

bool mqtt_client_connect(void) {
    if (!mqtt_client.connected()) {
        serial_printf("Attempting MQTT connection...\n");
        if (!mqtt_client.connect(mqtt_id)) {
            serial_printf("mqtt connection failed\n");
            return false;
        } else {
            serial_printf("disabling nagle's\n");
            wifi_client.setNoDelay(true);
        }
    }
    return true;
}

void setup_mqtt(void) {
    mqtt_client.setServer(mqtt_broker, MQTT_PORT);

    if (WAIT_FOR_MQTT) {
        int timeout = MQTT_TIMEOUT_SECONDS * 1000;

        while(timeout > 0 && !mqtt_client_connect()) {
            serial_printf("Waiting for mqtt (timeout: %d)\n", timeout);
            delay(500);
            timeout -= 500;
        }

        if (!mqtt_client_connect()) {
            serial_printf("Timeout connecting to mqtt\n");
            fatal();
        } else {
            serial_printf("Mqtt connected\n");
        }
    }
}

void poll_mqtt(void) {
    if (mqtt_client_connect()) {
        mqtt_client.loop();
    }
}
