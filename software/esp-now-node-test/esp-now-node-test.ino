#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#include "credentials.h"
#include "utils.h"

#define WAIT_FOR_SERIAL                 true
#define BAUD                            115200
#define SERIAL_BUF_LEN                  255
#define WIFI_MODE                       WIFI_STA
#define WIFI_POWERSAVING_MODE           WIFI_PS_MIN_MODEM//WIFI_PS_NONE
#define SERIAL_LOG_INTERVAL             2000

// Go to sleep after sending a packet?
#define SLEEP                           false
#define CHANNEL                         3

// last time various periodic functions were run
unsigned long last_serial_log_millis = 0;

int channel = 0;
bool channel_locked = false;
// Start of broadcasting, changed to the real mac once we find the bridge
uint8_t bridge_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup_serial(void);
void setup_wifi(void);
void setup_now(void);

void poll_wifi_status(void);
void update_serial(void);
void poll_mqtt(void);

void setup() {
    setup_serial();
    setup_wifi();
    setup_now();
}

void loop() {
    update_serial();

    if (SLEEP) {
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

    serial_printf("%s v%s (%s). mac: %s\n",
        PROJECT,
        VERSION,
        BUILD_TIMESTAMP,
        WiFi.macAddress().c_str()
    );
}

void setup_wifi(void) {
    WiFi.mode(WIFI_MODE);
    esp_wifi_set_ps(WIFI_POWERSAVING_MODE);
}

void change_channel(int channel) {
    serial_printf("Switching to channel %d\n", channel);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

void update_serial(void) {
    if (SLEEP || delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {

        if (!channel_locked) {
            channel += 1;
            if (channel > 14) {
                channel = 1;
            }
            change_channel(channel);
        }

        char buf[SERIAL_BUF_LEN];

        format(buf, SERIAL_BUF_LEN,
            "{ \"sleep\": %s, \"project\": \"%s\", \"action\": \"boop\", \"mac\": \"%s\" }",
            SLEEP ? "true" : "false",
            PROJECT,
            WiFi.macAddress().c_str()
        );

        if (!channel_locked) {
            broadcast("hello");
        } else {
            broadcast(buf);
        }
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

    if (!channel_locked) {
        serial_printf("Discovered bridge on channel %d\n", channel);
        // We're on the right channel
        channel_locked = true;
        // Update the mac we're using for the bridge to the real mac we just found
        memcpy(&bridge_mac, mac, 6);
    }
}

void broadcast(const String &message) {
    esp_now_peer_info_t peer_info = {};
    memcpy(&peer_info.peer_addr, bridge_mac, 6);

    if (!esp_now_is_peer_exist(bridge_mac)) {
        esp_now_add_peer(&peer_info);
    }

    esp_err_t result = esp_now_send(
        bridge_mac,
        (const uint8_t *)message.c_str(),
        message.length()
    );

    print_esp_now_result(result, "esp-now-broadcast");
}
