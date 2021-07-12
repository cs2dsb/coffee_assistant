#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "credentials.h"
#include "utils.h"
#include "config.h"
#include "wifi.h"
#include "now.h"

// Go to sleep after sending a packet?
#define SLEEP                           true

// For debugging, set this to clear out the eeprom stored values and
// skip setting them. Then change it back and reprogram to test the init
// to rtc to eeprom dance
#define WIPE_EEPROM                     false

// Are we waiting for a send callback (if so, we can't sleep yet)
bool pending_esp_now_send = false;

// Skip the first delay
bool skip_delay = true;

// Only set once we've successfully sent a packet, to prevent sleep if the
// first packet fails
bool ping_sent = false;

void setup_serial(void);

void poll_wifi_status(void);
//void ping_bridge(void);

void setup() {
    setup_serial();

    configure_wifi(NULL, NULL, WIFI_STA, WIFI_PS_MIN_MODEM, false, false);
    configure_now(true, false, true);
}

void loop() {
    now_update();
    //ping_bridge();

    // We can only sleep once we find the channel because we need to stay awake
    // listening for the response from the bridge
    // if (SLEEP && channel_locked && !pending_esp_now_send && ping_sent) {
    //     serial_printf("sleeping now\n");
    //     esp_wifi_stop();
    //     esp_sleep_enable_timer_wakeup(SERIAL_LOG_INTERVAL * 1000);
    //     esp_deep_sleep_start();
    // } else {
    //     delay(1);
    // }
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

// void ping_bridge(void) {
//     // If the delay has passed we always want to send again, if the send cb failed to clear
//     // pending or we didn't get a response, we want to send again or we'll just get stuck
//     if (delay_elapsed(&last_ping_bridge_millis, SERIAL_LOG_INTERVAL)
//         // We skip the first delay so when we're using deep sleep we don't have to wait and
//         // so we start scanning immediatly even if we aren't using deep sleep
//         || skip_delay)
//     {
//         skip_delay = false;

//         if (!channel_locked) {
//             channel += 1;
//             if (channel > 14) {
//                 channel = 1;
//             }
//             change_channel(channel);
//         }

//         char buf[SERIAL_BUF_LEN];

//         format(buf, SERIAL_BUF_LEN,
//             "{ \"sleep\": %s, \"project\": \"%s\", \"action\": \"boop\", \"mac\": \"%s\" }",
//             SLEEP ? "true" : "false",
//             PROJECT,
//             WiFi.macAddress().c_str()
//         );

//         if (!channel_locked) {
//             broadcast("hello");
//         } else {
//             broadcast(buf);
//         }
//     }
// }
