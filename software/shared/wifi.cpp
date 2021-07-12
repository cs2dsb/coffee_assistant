#include "wifi.h"
#include "esp_attr.h"
#include <stdint.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "utils.h"
#include "rtc_data.h"

wl_status_t last_wifi_state = WL_NO_SHIELD;

void configure_wifi(const char *ssid, const char *password, wifi_mode_t mode, wifi_ps_type_t ps, bool promiscuous, bool begin) {
    WiFi.mode(mode);
    if (mode == WIFI_MODE_NULL) {
        esp_wifi_stop();
        return;
    }

    esp_wifi_set_ps(ps);
    esp_wifi_set_promiscuous(promiscuous);

    auto rtc_data = get_rtc_data();

    uint8_t* bssid_ = NULL;
    if (rtc_data->channel > 0) {
        bssid_ = &rtc_data->bssid[0];

        if (rtc_data->wake_count % DHCP_RENEW_INTERVAL != 0) {
            IPAddress ip = IPAddress(rtc_data->static_ip.ip);
            IPAddress gateway = IPAddress(rtc_data->static_ip.gateway);
            IPAddress subnet = IPAddress(rtc_data->static_ip.subnet);

            serial_printf("using static:\n   static_address: %s\n   gateway: %s\n   subnet: %s\n",
                ip.toString().c_str(),
                gateway.toString().c_str(),
                subnet.toString().c_str());

            if (!WiFi.config(ip, gateway, subnet, gateway)) {
                serial_printf("Failed to configure static IP\n");
            }
        }
    }

    if (begin) {
        serial_printf("WiFi: Connecting with SSID: '%s', Channel: %d\n", ssid, rtc_data->channel);
        WiFi.begin(ssid, password, rtc_data->channel, bssid_);
    }
}

bool wait_for_connection(void) {
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
        serial_printf("Restarting...\n");
        return false;
    } else {
        serial_printf("Connected\n");
    }
    return true;
}

void disable_wifi(void) {
    if (WiFi.status() == WL_CONNECTED) {
        auto rtc_data = get_rtc_data();

        // Update the cached channel so we don't need to scan next time
        rtc_data->channel = WiFi.channel();
        uint8_t *bssid_ = WiFi.BSSID();
        memcpy(rtc_data->bssid, bssid_, 6);

        rtc_data->static_ip.ip = (uint32_t)WiFi.localIP();
        rtc_data->static_ip.gateway = (uint32_t)WiFi.gatewayIP();
        rtc_data->static_ip.subnet = (uint32_t)WiFi.subnetMask();
        rtc_data->static_ip.dns = (uint32_t)WiFi.gatewayIP();

        serial_printf("storing:\n   ip: %s\n   gateway: %s\n   subnet: %s\n",
            WiFi.localIP().toString().c_str(),
            WiFi.gatewayIP().toString().c_str(),
            WiFi.subnetMask().toString().c_str());
    }
    esp_wifi_stop();
}

void print_wifi_status(void) {
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

const char* mac_address(void) {
    return WiFi.macAddress().c_str();
}

void set_wifi_channel(int channel) {
    serial_printf("Switching to channel %d\n", channel);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

int wifi_channel() {
    uint8_t channel;
    wifi_second_chan_t second;
    fatal_on_fail(ESP_OK == esp_wifi_get_channel(&channel, &second), "failed calling esp_wifi_get_channel");
    return (int)channel;
}
