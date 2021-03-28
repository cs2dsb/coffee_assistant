#include <stdlib.h>
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>

#define USE_LittleFS
#include <FS.h>
#ifdef USE_LittleFS
  #define SPIFFS LITTLEFS
  #include <LITTLEFS.h>
#else
  #include <SPIFFS.h>
#endif

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <TM1638plus.h>
#include <HX711_ADC.h>

#include "credentials.h"
#include "static/routes.h"
#include "handlers.h"
#include "utils.h"

#define WAIT_FOR_SERIAL         true
#define BAUD                    115200
#define SERIAL_BUF_LEN          255
#define WIFI_MODE               WIFI_AP_STA
#define WAIT_FOR_WIFI           false
#define WIFI_TIMEOUT_SECONDS    30
#define BLINK_INTERVAL          1000
#define HX711_POLL_INTERVAL     500
#define TM1638_POLL_INTERVAL    100
#define TM1638_UPDATE_INTERVAL  50
#define SERIAL_LOG_INTERVAL     1000
#define HTTP_PORT               80
#define DEFAULT_CALIBRATION     1100.0

#define TM1638_HIGH_FREQ        true

#define PIN_TM1638_STB          27
#define PIN_TM1638_CLK          26
#define PIN_TM1638_DIO          25
#define PIN_HX711_DT            14
#define PIN_HX711_SCK           12
#define PIN_LED                 21

// From credentials.h
const char* host            = MDNS_HOST;
const char* ssid            = WIFI_SSID;
const char* password        = WIFI_PASSWORD;


// Global variables
unsigned long last_blink_millis = 0;
int last_blink_state = LOW;

unsigned long last_hx711_poll_millis = 0;
float last_hx711_value = 0.0;

unsigned long last_tm1638_poll_millis = 0;
byte last_tm1638_buttons = false;

unsigned long last_tm1638_update_millis = 0;
unsigned long last_serial_log_millis = 0;

unsigned long elapsed = 0;
unsigned long last_elapsed_millis = 0;

bool tare = false;
bool calibrate = false;

AsyncWebServer server(HTTP_PORT);
TM1638plus tm1638(PIN_TM1638_STB, PIN_TM1638_CLK, PIN_TM1638_DIO, TM1638_HIGH_FREQ);
HX711_ADC hx711(PIN_HX711_DT, PIN_HX711_SCK);

void setup_spiffs(void);
void setup_serial(void);
void setup_server(void);
void setup_gpio(void);
void setup_tm1638(void);
void update_tm1638(void);
void update_elapsed(void);
void setup_hx711(void);
void poll_hx711(void);
void tare_hx711(bool wait);
void poll_tm1638(void);
void blink(void);
void process_buttons(byte buttons);
bool delay_elapsed(unsigned long *last, unsigned long interval);
void fatal(void);

void setup() {
    setup_serial();
    setup_gpio();
    setup_tm1638();
    setup_hx711();

    setup_spiffs();
    setup_server();

    // Wait while server starts up to let the hx711 stabilize before tareing
    tare_hx711(true);
}

void loop() {
    AsyncElegantOTA.loop();

    blink();
    poll_hx711();
    poll_tm1638();
    update_tm1638();
    update_elapsed();

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
            fatal();
        } else {
            serial_printf("Connected\n");
        }
    }

    serial_printf("Starting mDNS responder on %s.local\n", host);
    MDNS.begin(host);

    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     serial_printf("GET /\n");
    //     request->send(200, "text/html", server_index);
    // });
    register_routes(&server);

    // this doesn't work because brotli isn't enabled on http
    // server.on("/spiffy", HTTP_GET, [](AsyncWebServerRequest *request){
    //     AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html.br", "text/html", false);
    //     response->addHeader("Content-Encoding", "br");
    //     request->send(response);
    // });

    // server.on("/gzippy", HTTP_GET, [](AsyncWebServerRequest *request){
    //     AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html.gz", "text/html", false);
    //     response->addHeader("Content-Encoding", "gzip");
    //     request->send(response);
    // });

    // Install OTA
    AsyncElegantOTA.begin(&server);

    server.begin();
    MDNS.addService("http", "tcp", HTTP_PORT);
    serial_printf("HTTP listening on port %d\n", HTTP_PORT);
}

void setup_gpio(void) {
  pinMode(PIN_LED, OUTPUT);
}

void setup_tm1638(void) {
    tm1638.displayBegin();
    tm1638.displayText("booted");

    // All LEDs OFF
    tm1638.setLEDs(0x0000);
}

void setup_hx711(void) {
    hx711.setCalFactor(DEFAULT_CALIBRATION);
    hx711.begin();
    hx711.setSamplesInUse(1);
    hx711.start(0);
    hx711.update();
}

void setup_spiffs(void) {
    if(!SPIFFS.begin()){
        serial_printf("Failed to mount SPIFFS");
        fatal();
    }
}

void tare_hx711(bool wait) {
    hx711.tareNoDelay();
    tare = false;

    bool timeout;
    do {
        hx711.update();
        timeout = hx711.getTareTimeoutFlag()
               || hx711.getSignalTimeoutFlag();
    } while (wait && !hx711.getTareStatus() && !timeout);

    if (timeout) {
        serial_printf("hx711 tare timed out\n");
        fatal();
    }
}

void blink(void) {
    if (delay_elapsed(&last_blink_millis, BLINK_INTERVAL)) {
        if (last_blink_state == LOW) {
          last_blink_state = HIGH;
        } else {
          last_blink_state = LOW;
        }

        digitalWrite(PIN_LED, last_blink_state);
        tm1638.setLED(0, last_blink_state);
    }
}

void poll_hx711(void) {
    if (hx711.update()) {
        if (delay_elapsed(&last_hx711_poll_millis, HX711_POLL_INTERVAL)) {
            last_hx711_value = hx711.getData();
            //serial_printf("scale value: %f\n", last_hx711_value);

            if (tare) {
                tare_hx711(false);
            }

            if (hx711.getTareStatus()) {
                serial_printf("tare complete\n");
            }

            if (calibrate) {
                float known_mass = 100.0;

                while (!hx711.update()) {}

                // Clear out existing averaging values
                hx711.setSamplesInUse(16);
                hx711.refreshDataSet();

                float calibration = hx711.getNewCalibration(known_mass);
                hx711.setSamplesInUse(1);
                serial_printf("new calibration: %f\n", calibration);

                calibrate = false;
            }
        }
    }
}

void poll_tm1638(void) {
    if (delay_elapsed(&last_tm1638_poll_millis, TM1638_POLL_INTERVAL)) {
        byte buttons = tm1638.readButtons();
        if (buttons != last_tm1638_buttons) {
            last_tm1638_buttons = buttons;
            serial_printf("tm1638 buttons: %lu\n", last_tm1638_buttons);
            process_buttons(last_tm1638_buttons);
        }
    }
}

void update_tm1638(void) {
    if (delay_elapsed(&last_tm1638_update_millis, TM1638_UPDATE_INTERVAL)) {
        float seconds = ((float)elapsed) / 1000.0;
        while (seconds > 999.0) {
            seconds -= 999.0;
        }

        char buf[SERIAL_BUF_LEN];
        format(buf, SERIAL_BUF_LEN, "%5.1f%5.1f", seconds, last_hx711_value);

        tm1638.displayText(buf);

        if (delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {
            serial_printf("Time: %5.1f   Weight: %5.1f\n", seconds, last_hx711_value);
        }
    }
}

void update_elapsed(void) {
    unsigned long now = millis();
    unsigned long delta = now - last_elapsed_millis;
    last_elapsed_millis = now;
    elapsed += delta;
}

void process_buttons(byte buttons) {
    if ((buttons >> 7) & 1 == 1) {
        tare = true;
    }
    if ((buttons >> 6) & 1 == 1) {
        calibrate = true;
    }
    if ((buttons >> 5) & 1 == 1) {
        elapsed = 0;
    }
}
