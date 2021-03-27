#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include "credentials.h"
#include "static/index.html.h"

#define WAIT_FOR_SERIAL         true
#define BAUD                    115200
#define SERIAL_BUF_LEN          256
#define WIFI_MODE               WIFI_AP_STA
#define WAIT_FOR_WIFI           true
#define WIFI_TIMEOUT_SECONDS    30
#define PIN_LED                 21
#define BLINK_INTERVAL          1000
#define HTTP_PORT               80

// From credentials.h
const char* host            = MDNS_HOST;
const char* ssid            = WIFI_SSID;
const char* password        = WIFI_PASSWORD;

// From static/*.html.h
const char* server_index    = INDEX_HTML;

// Global variables
AsyncWebServer server(HTTP_PORT);
unsigned long last_blink_millis = 0;
int last_blink_state = LOW;

void setup_serial(void);
void setup_server(void);
void setup_gpio(void);
void serial_printf(const char *fmt, ...);
void blink(void);

void setup() {
    setup_serial();
    setup_server();
    setup_gpio();
}

void loop() {
    blink();
    AsyncElegantOTA.loop();

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
            serial_printf("Restarting...\n");
            ESP.restart();
        } else {
            serial_printf("Connected\n");
        }
    }

    serial_printf("Starting mDNS responder on %s.local\n", host);
    MDNS.begin(host);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        serial_printf("GET /\n");
        request->send(200, "text/html", server_index);
    });

    // Install OTA
    AsyncElegantOTA.begin(&server);

    server.begin();
    MDNS.addService("http", "tcp", HTTP_PORT);
    serial_printf("HTTP listening on port %d\n", HTTP_PORT);
}

void setup_gpio(void) {
  pinMode(PIN_LED, OUTPUT);
}

void blink(void) {
    unsigned long now = millis();

    if (now - last_blink_millis >= BLINK_INTERVAL) {
        last_blink_millis = now;

        if (last_blink_state == LOW) {
          last_blink_state = HIGH;
        } else {
          last_blink_state = LOW;
        }

        digitalWrite(PIN_LED, last_blink_state);
        serial_printf("BLINK: %lu\n", last_blink_millis);
    }
}

void serial_printf(const char *fmt, ...) {
  char buf[SERIAL_BUF_LEN];
  va_list pargs;
  va_start(pargs, fmt);
  vsnprintf(buf, SERIAL_BUF_LEN, fmt, pargs);
  va_end(pargs);
  Serial.print(buf);
}
