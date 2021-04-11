#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <esp_now.h>

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

#define SERIAL_LOG_INTERVAL             10000
#define WIFI_STATUS_UPDATE_INTERVAL     1000

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

wl_status_t last_wifi_state = WL_DISCONNECTED;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

void setup_serial(void);
void setup_wifi(void);
void setup_mqtt(void);
void setup_now(void);

void poll_wifi_status(void);
void update_serial(void);
bool mqtt_client_connect(void);
void poll_mqtt(void);


void setup() {
    setup_serial();
    setup_wifi();
    setup_now();
    setup_mqtt();
}

void loop() {
    update_serial();
    poll_wifi_status();
    poll_mqtt();
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

void setup_now(void) {
    auto ret = esp_now_init();
    if (ret == ESP_OK)  {
        serial_printf("esp_now_init OK\n");
        esp_now_register_recv_cb(receive_callback);
        esp_now_register_send_cb(send_callback);
    } else {
        serial_printf("esp_now_init failed: %d\n", esp_err_to_name(ret));
        fatal();
    }
}

void send_callback(const uint8_t *mac, esp_now_send_status_t status) {
    char buf[18];
    format_mac(mac, buf, sizeof(buf));

    serial_printf("Packet sent to %s, status: %s\n",
        buf,
        status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL"
    );
}

void receive_callback(const uint8_t *mac, const uint8_t *data, int data_len) {
    int len = min(ESP_NOW_MAX_DATA_LEN, data_len);
    char now_buf[ESP_NOW_MAX_DATA_LEN + 1];
    strncpy(now_buf, (const char *)data, len);

    now_buf[len] = '\0';

    char buf[18];
    format_mac(mac, buf, sizeof(buf));
    serial_printf("Packet received from %s, contents: %s\n",
        buf,
        now_buf
    );

    const char* hello = "hello";
    if (strncmp(hello, now_buf, strlen(hello)) == 0) {
        printf("Got hello from %s. Responding\n", buf);
        send("hello", (uint8_t*)mac);
    }

    char mqtt_buf[SERIAL_BUF_LEN];

    format(mqtt_buf, SERIAL_BUF_LEN,
        "{ \"project\": \"%s\", \"action\": \"relay\", \"mac\": \"%s\", \"inner\": %s }",
        PROJECT,
        WiFi.macAddress().c_str(),
        now_buf
    );

    if (mqtt_client_connect() || !WAIT_FOR_MQTT) {
        while (!mqtt_client.publish(mqtt_topic, mqtt_buf) && WAIT_FOR_MQTT) {
            serial_printf("mqtt publish failed. retrying\n");
            delay(500);
        }
    }
}

void broadcast(const String &message) {
    uint8_t broadcast_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t peer_info = {};

    memcpy(&peer_info.peer_addr, broadcast_address, 6);
    if (!esp_now_is_peer_exist(broadcast_address)) {
        esp_now_add_peer(&peer_info);
    }
    esp_err_t result = esp_now_send(
        broadcast_address,
        (const uint8_t *)message.c_str(),
        message.length()
    );

    print_esp_now_result(result, "esp-now-broadcast");
}

void send(const String &message, uint8_t mac[6]) {
    esp_now_peer_info_t peer_info = {};

    memcpy(&peer_info.peer_addr, mac, 6);
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_add_peer(&peer_info);
    }
    esp_err_t result = esp_now_send(
        mac,
        (const uint8_t *)message.c_str(),
        message.length()
    );

    print_esp_now_result(result, "esp-now-send");
}
