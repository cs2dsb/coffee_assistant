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

typedef int channel_t;
typedef bool channel_locked_t;
typedef uint8_t mac_addr_t[6];

#define EEPROM_CHANNEL_ADDR             EEPROM_FIRST_ADDR
#define EEPROM_CHANNEL_LOCKED_ADDR      (EEPROM_CHANNEL_ADDR + sizeof(channel_t))
#define EEPROM_BRIDGE_MAC_ADDR          (EEPROM_CHANNEL_LOCKED_ADDR + sizeof(channel_locked_t))

// Go to sleep after sending a packet?
#define SLEEP                           true

// For debugging, set this to clear out the eeprom stored values and
// skip setting them. Then change it back and reprogram to test the init
// to rtc to eeprom dance
#define WIPE_EEPROM                     false

// last time various periodic functions were run
unsigned long last_ping_bridge_millis = 0;

RTC_DATA_ATTR channel_t channel = 0;
RTC_DATA_ATTR channel_locked_t channel_locked = false;
// Start off broadcasting, changed to the real mac once we find the bridge
RTC_DATA_ATTR mac_addr_t bridge_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Are we waiting for a send callback (if so, we can't sleep yet)
bool pending_esp_now_send = false;

// Skip the first delay
bool skip_delay = true;

// Only set once we've successfully sent a packet, to prevent sleep if the
// first packet fails
bool ping_sent = false;

void setup_serial(void);
void setup_wifi(void);
void setup_now(void);

void read_persistent_state(void);
void write_persistent_state(void);

void poll_wifi_status(void);
void ping_bridge(void);
void poll_mqtt(void);

void setup() {
    setup_serial();
    setup_wifi();
    setup_now();
    read_persistent_state();
}

void loop() {
    ping_bridge();

    // We can only sleep once we find the channel because we need to stay awake
    // listening for the response from the bridge
    if (SLEEP && channel_locked && !pending_esp_now_send && ping_sent) {
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

void ping_bridge(void) {
    // If the delay has passed we always want to send again, if the send cb failed to clear
    // pending or we didn't get a response, we want to send again or we'll just get stuck
    if (delay_elapsed(&last_ping_bridge_millis, SERIAL_LOG_INTERVAL)
        // We skip the first delay so when we're using deep sleep we don't have to wait and
        // so we start scanning immediatly even if we aren't using deep sleep
        || skip_delay)
    {
        skip_delay = false;

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

    pending_esp_now_send = false;
    ping_sent |= status == ESP_NOW_SEND_SUCCESS;
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

        // Stash the values in eeprom for next time we hard reboot
        write_persistent_state();
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

    pending_esp_now_send = result == ESP_OK;

    print_esp_now_result(result, "broadcast (esp_now_send)");
}

void read_persistent_state(void) {
    // Check if we already updated rtc memory
    // channel will only be 0 after the chip is programmed or powered on (not from sleep)
    if (channel == 0) {
        serial_printf("No stored channel/mac data in rtc memory\n");

        if (WIPE_EEPROM) {
            serial_printf("WIPE_EEPROM set, clearing out any stored values\n");
            wipe_eeprom();
        } else {
            channel_t channel_tmp;

            if (read_from_eeprom(EEPROM_CHANNEL_ADDR, channel_tmp)) {
                // EEPROM contains valid data we can use

                channel = channel_tmp;

                read_from_eeprom(EEPROM_CHANNEL_LOCKED_ADDR, channel_locked);
                read_from_eeprom(EEPROM_BRIDGE_MAC_ADDR, bridge_mac);

                serial_printf("Loaded stored channel/mac data from eeprom\n");
            } else {
                serial_printf("No stored channel/mac data in eeprom\n");
            }
        }
    } else {
        serial_printf("Loaded stored channel/mac data from rtc memory\n");
    }

    char mac_str[18];
    format_mac(bridge_mac, mac_str, sizeof(mac_str));

    serial_printf("read_persistent_state => channel: %d, channel_locked: %s, bridge_mac: %s\n",
        channel,
        channel_locked ? "true": "false",
        mac_str
    );

    if (channel != 0) {
        change_channel(channel);
    }
}

void write_persistent_state(void) {
    if (WIPE_EEPROM) { return; }

    write_to_eeprom(EEPROM_CHANNEL_ADDR, channel, false);
    write_to_eeprom(EEPROM_CHANNEL_LOCKED_ADDR, channel_locked, false);
    write_to_eeprom(EEPROM_BRIDGE_MAC_ADDR, bridge_mac);

    char mac_str[18];
    format_mac(bridge_mac, mac_str, sizeof(mac_str));

    serial_printf("write_persistent_state => channel: %d, channel_locked: %s, bridge_mac: %s\n",
        channel,
        channel_locked ? "true": "false",
        mac_str
    );
}
