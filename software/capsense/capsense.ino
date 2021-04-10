#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include "utils.h"
#include "credentials.h"

#define BAUD                            115200
#define SERIAL_BUF_LEN                  255
#define SERIAL_LOG_INTERVAL             500
#define TOUCH_THRESHOLD                 40
#define TOUCH_PIN                       15

// From credentials.h
const char* host            = MDNS_HOST;
const char* ssid            = WIFI_SSID;
const char* password        = WIFI_PASSWORD;

unsigned long last_serial_log_millis = 0;

void setup_serial(void);
void setup_gpio(void);
void update_serial(void);
void touch_isr(void);

void setup() {
    setup_serial();
    setup_gpio();
}

void loop() {
    update_serial();
    delay(1);
}

void setup_serial(void) {
    Serial.begin(BAUD);

    serial_printf("%s v%s (%s)\n", PROJECT, VERSION, BUILD_TIMESTAMP);
}

void setup_gpio(void) {
  touchAttachInterrupt(TOUCH_PIN, touch_isr, TOUCH_THRESHOLD);



    // pinMode(PIN_LED, OUTPUT);

    // pinMode(PIN_TARE, INPUT);
    // pinMode(PIN_CAL, INPUT);

    // attachInterrupt(digitalPinToInterrupt(PIN_TARE), tare_isr, FALLING);
    // attachInterrupt(digitalPinToInterrupt(PIN_CAL), cal_isr, FALLING);
}


void update_serial(void) {
    if (delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {
        //serial_printf("Time: %.1f   Weight: %.1f\n", elapsed_seconds, weight_ema_1);
        Serial.println(touchRead(TOUCH_PIN));
    }

}

void touch_isr(void) {
    serial_printf("touched\n");
}
