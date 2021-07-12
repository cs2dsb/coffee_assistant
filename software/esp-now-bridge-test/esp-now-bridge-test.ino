#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>

#include "credentials.h"
#include "utils.h"
#include "config.h"
#include "wifi.h"
#include "now.h"
#include "mqtt.h"

// last time various periodic functions were run
unsigned long last_serial_log_millis = 0;

void setup_serial(void);
void update_serial(void);

void setup() {
    setup_serial();
    configure_general();
    configure_wifi(WIFI_SSID, WIFI_PASSWORD);
    configure_now(false, true);
    configure_mqtt(MQTT_URL);
}

void loop() {
    update_serial();
    now_update();
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

void update_serial(void) {
    if (delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {
        //serial_printf("boop\n");
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
