#include "now.h"
#include "esp_err.h"
#include "utils.h"
#include "wifi.h"
#include "rtc_data.h"

#define CHANNEL_REQUEST_PREFIX "channel:"
#define LATENCY_REQUEST_PREFIX "latency:"

bool relay = false;
bool scan = false;
bool heartbeat = false;
unsigned long last_update_millis = 0;
receive_callback_fn on_receive = NULL;
unsigned long latency_send_time = 0;

// TODO: This is pretty hacky
// the latency on a relay will be from whichever node most recently did a test
// might be a good idea to keep a list of peers on the relay or something
int latency = 0;

void send_callback(const uint8_t *mac, esp_now_send_status_t status);
void receive_callback(const uint8_t *mac, const uint8_t *data, int data_len);
void print_esp_now_result(esp_err_t result, char *prefix);
void load_persistent_state(void);
void write_persistent_state(void);

void now_set_on_receive(receive_callback_fn callback) {
    on_receive = callback;
}

int now_latency(void) {
    return latency;
}

bool configure_now(bool with_scan, bool as_relay, bool with_heartbeat) {
    panic_on_fail(!(as_relay && with_scan), "can't configure now to both scan and act as a relay");

    relay = as_relay;
    scan = with_scan;
    heartbeat = with_heartbeat;

    auto ret = esp_now_init();
    if (ret == ESP_OK)  {
        serial_printf("esp_now_init OK\n");
        esp_now_register_recv_cb(receive_callback);
        esp_now_register_send_cb(send_callback);
    } else {
        serial_printf("esp_now_init failed: %d\n", esp_err_to_name(ret));
        return false;
    }

    if (scan) {
        load_persistent_state();
    }

    return true;
}

void now_update(void) {
    if (delay_elapsed(&last_update_millis, NOW_UPDATE_INTERVAL)) {
        if (scan) {
            auto rtc_data = get_rtc_data();

            // If we haven't cofirmed we have found the right channel
            if (!rtc_data->channel_locked) {
                // Move to the next channel (it starts at 0 which is invalid)
                rtc_data->channel += 1;
                if (rtc_data->channel > 14) {
                    rtc_data->channel = 1;
                }
                set_wifi_channel(rtc_data->channel);
            }

            char buf[SERIAL_BUF_LEN];
            if (!rtc_data->channel_locked) {
                format(buf, SERIAL_BUF_LEN, CHANNEL_REQUEST_PREFIX);
            } else if(heartbeat) {
                format(buf, SERIAL_BUF_LEN, "{ \"channel\": %d, \"mac\": \"%s\" }", rtc_data->channel, mac_address());
            } else {
                buf[0] = '\0';
            }

            auto len = strlen(buf);
            if (len > 0) {
                now_send((uint8_t*)buf, len, rtc_data->bridge_mac);
            }
        }

        auto rtc_data = get_rtc_data();
        // Only send latency requests if we've locked the channel (i.e. we're sending to a known bridge)
        if (rtc_data->channel_locked) {
            now_send((uint8_t*)LATENCY_REQUEST_PREFIX, strlen(LATENCY_REQUEST_PREFIX), rtc_data->bridge_mac);
            latency_send_time = millis();
        }
    }
}

bool now_send(const uint8_t* message, size_t len, uint8_t mac[6]) {
    esp_now_peer_info_t peer_info = {};

    memcpy(&peer_info.peer_addr, mac, 6);
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_add_peer(&peer_info);
    }

    esp_err_t result = esp_now_send(
        mac,
        message,
        len
    );

    if (TRACE_NOW) {
        print_esp_now_result(result, "now_send");
    }

    return ESP_OK == result;
}

bool now_send_to_bridge(const uint8_t* message, size_t len) {
    auto rtc_data = get_rtc_data();

    if (!rtc_data->channel_locked) {
        serial_printf("NOW: now_send_to_bridge called before bridge discovered: %s\n", message);
        return false;
    }

    return now_send(message, len, rtc_data->bridge_mac);
}

bool now_send_to_bridge(const String message) {
    return now_send_to_bridge((uint8_t*)message.c_str(), message.length());
}

bool now_broadcast(const uint8_t* message, size_t len) {
    uint8_t broadcast_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return now_send(message, len, broadcast_address);
}

bool now_broadcast(const String message) {
    return now_broadcast((uint8_t*)message.c_str(), message.length());
}

void send_callback(const uint8_t *mac, esp_now_send_status_t status) {
    char buf[18];
    format_mac(mac, buf, sizeof(buf));

    if (TRACE_NOW) {
        serial_printf("NOW: Packet sent to %s, status: %s\n",
            buf,
            status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL"
        );
    }
}

void receive_callback(const uint8_t *mac, const uint8_t *data, int data_len) {
    int len = min(ESP_NOW_MAX_DATA_LEN, data_len);
    char now_buf[ESP_NOW_MAX_DATA_LEN + 1];
    strncpy(now_buf, (const char *)data, len);

    now_buf[len] = '\0';

    char buf[18];
    format_mac(mac, buf, sizeof(buf));
    if (TRACE_NOW) {
        serial_printf("NOW: packet received from %s, contents: %s\n",
            buf,
            now_buf
        );
    }

    if (strncmp(CHANNEL_REQUEST_PREFIX, now_buf, strlen(CHANNEL_REQUEST_PREFIX)) == 0) {
        // Relay should respond to channel requests
        if (relay) {
            serial_printf("Got channel req from %s. Responding\n", buf);
            format(now_buf, ESP_NOW_MAX_DATA_LEN, "%s%d", CHANNEL_REQUEST_PREFIX, wifi_channel());
            now_send((uint8_t*)now_buf, strlen(now_buf), (uint8_t*)mac);
        }

        // Scanner can lock in when getting a channel response
        if (scan) {
            auto rtc_data = get_rtc_data();
            if (!rtc_data->channel_locked) {
                serial_printf("Discovered bridge on channel %d\n", rtc_data->channel);

                auto chan_part = &now_buf[strlen(CHANNEL_REQUEST_PREFIX)];

                int channel = atoi(chan_part);
                if (channel != rtc_data->channel) {
                    // TODO: make this more robust?
                    // This happens if the signal strength is really high and should be resolved when we test the next
                    // channel which is probably the real one we're interested in. If you lock in to the wrong channel
                    // it *might* work but more often than not there'll be a lot of packet loss
                    serial_printf("Got reply on channel %d but we're actually on %d, ignoring", channel, rtc_data->channel);
                } else {
                    // We're on the right channel
                    rtc_data->channel_locked = true;
                    // Update the mac we're using for the bridge to the real mac we just found
                    memcpy(&rtc_data->bridge_mac, mac, 6);

                    // Stash the values in eeprom for next time we hard reboot
                    write_persistent_state();
                }
            }
        }
    } else if (strncmp(LATENCY_REQUEST_PREFIX, now_buf, strlen(LATENCY_REQUEST_PREFIX)) == 0) {
        if (relay) {
            if (strlen(now_buf) == strlen(LATENCY_REQUEST_PREFIX)) {
                // It's just a request, just echo back
                now_send((uint8_t*)now_buf, strlen(now_buf), (uint8_t*)mac);
            } else {
                // It's got the latency from the sender included
                auto latency_part = &now_buf[strlen(LATENCY_REQUEST_PREFIX)];
                latency = atoi(latency_part);
                if (TRACE_NOW) {
                    serial_printf("Got latency value from peer: %d\n", latency);
                }
            }
        } else {
            latency = (int)(millis() - latency_send_time);
            format(now_buf, ESP_NOW_MAX_DATA_LEN + 1,
                "%s%d", LATENCY_REQUEST_PREFIX, latency);
            now_send((uint8_t*)now_buf, strlen(now_buf), (uint8_t*)mac);
        }
    } else if (on_receive != NULL) {
        on_receive(mac, data, len);
    }
}

void print_esp_now_result(esp_err_t result, char *prefix) {
    if (result == ESP_OK) {
        serial_printf("%s: success\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
        serial_printf("%s: espnow not init\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_ARG) {
        serial_printf("%s: invalid argument\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
        serial_printf("%s: internal error\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
        serial_printf("%s: no mem\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
        serial_printf("%s: peer not found\n", prefix);
    } else {
        serial_printf("%s: unknown error %d\n", prefix, result);
    }
}

void load_persistent_state(void) {
    auto rtc_data = get_rtc_data();
    // Check if we already updated rtc memory
    // channel will only be 0 after the chip is programmed or powered on (not from sleep)
    // TODO: these checks should be proper checksum checks instead of just checking the channel is broadly valid
    if (rtc_data->channel <= 0) {
        serial_printf("NOW: No stored channel/mac data in rtc memory\n");

        if (WIPE_EEPROM) {
            serial_printf("NOW: WIPE_EEPROM set, clearing out any stored values\n");
            wipe_eeprom();
        } else {
            channel_t channel_tmp;

            if (read_from_eeprom(EEPROM_CHANNEL_ADDR, channel_tmp) && channel_tmp > 0) {
                // EEPROM contains valid data we can use
                rtc_data->channel = channel_tmp;

                read_from_eeprom(EEPROM_CHANNEL_LOCKED_ADDR, rtc_data->channel_locked);
                read_from_eeprom(EEPROM_BRIDGE_MAC_ADDR, rtc_data->bridge_mac);

                serial_printf("NOW: Loaded stored channel/mac data from eeprom\n");
            } else {
                serial_printf("NOW: No stored channel/mac data in eeprom\n");
            }
        }
    } else {
        serial_printf("NOW: Loaded stored channel/mac data from rtc memory\n");
    }

    char mac_str[18];
    format_mac(rtc_data->bridge_mac, mac_str, sizeof(mac_str));

    serial_printf("NOW: read_persistent_state => channel: %d, channel_locked: %s, bridge_mac: %s\n",
        rtc_data->channel,
        rtc_data->channel_locked ? "true": "false",
        mac_str
    );

    if (rtc_data->channel != 0) {
        set_wifi_channel(rtc_data->channel);
    }
}

void write_persistent_state(void) {
    if (WIPE_EEPROM) { return; }

    auto rtc_data = get_rtc_data();

    write_to_eeprom(EEPROM_CHANNEL_ADDR, rtc_data->channel, false);
    write_to_eeprom(EEPROM_CHANNEL_LOCKED_ADDR, rtc_data->channel_locked, false);
    write_to_eeprom(EEPROM_BRIDGE_MAC_ADDR, rtc_data->bridge_mac);

    char mac_str[18];
    format_mac(rtc_data->bridge_mac, mac_str, sizeof(mac_str));

    serial_printf("write_persistent_state => channel: %d, channel_locked: %s, bridge_mac: %s\n",
        rtc_data->channel,
        rtc_data->channel_locked ? "true": "false",
        mac_str
    );
}
