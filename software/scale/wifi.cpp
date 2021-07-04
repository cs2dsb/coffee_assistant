#include "wifi.h"
#include "esp_attr.h"
#include <stdint.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "utils.h"

// If channel is 0 it will scan through them all
// after a connection is established this is updated to the
// discovered channel. It is then retained during deep sleep
RTC_DATA_ATTR int channel = 0;
RTC_DATA_ATTR uint8_t bssid[6] = {};
RTC_DATA_ATTR int wake_count = 0;

struct static_ip_data {
    uint32_t ip;
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns;
};

RTC_DATA_ATTR struct static_ip_data static_ip = {};

wl_status_t last_wifi_state = WL_NO_SHIELD;

void configure_wifi(const char *ssid, const char *password) {
    wake_count += 1;

    WiFi.mode(WIFI_MODE);
    esp_wifi_set_ps(WIFI_POWERSAVING_MODE);

    serial_printf("SSID: '%s', Channel: %d\n", ssid, channel);
    uint8_t* bssid_ = NULL;
    if (channel > 0) {
        bssid_ = &bssid[0];

        if (wake_count < DHCP_RENEW_INTERVAL) {
            IPAddress ip = IPAddress(static_ip.ip);
            IPAddress gateway = IPAddress(static_ip.gateway);
            IPAddress subnet = IPAddress(static_ip.subnet);

            serial_printf("using static:\n   static_address: %s\n   gateway: %s\n   subnet: %s\n",
                ip.toString().c_str(),
                gateway.toString().c_str(),
                subnet.toString().c_str());

            if (!WiFi.config(ip, gateway, subnet, gateway)) {
                serial_printf("Failed to configure static IP\n");
            }
        }
    }
    WiFi.begin(ssid, password, channel, bssid_);
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
        // Update the cached channel so we don't need to scan next time
        channel = WiFi.channel();
        uint8_t *bssid_ = WiFi.BSSID();
        memcpy(bssid, bssid_, 6);

        static_ip.ip = (uint32_t)WiFi.localIP();
        static_ip.gateway = (uint32_t)WiFi.gatewayIP();
        static_ip.subnet = (uint32_t)WiFi.subnetMask();
        static_ip.dns = (uint32_t)WiFi.gatewayIP();

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
