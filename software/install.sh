#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail


function export_env() {
    if test -s "$1"; then
        while IFS="" read -r line || [ -n "$line" ]; do

            if [[ "$line" =~ ^-D ]]; then
                echo "Adding \"$line\" to build properties"

                key=`cut -d'=' -f1 <<< $line`
                value=`cut -d'=' -f2 <<< $line`

                BUILD_PROPERTIES_HASH["$key"]="$value"
            else
                echo "Exporting: $line"
                export $line
            fi
        done < <(sed '/^\s*$/d' "$1" | sed '/^#/d') # seds remove comments and empty lines
    fi
}

export_env .env_shared
export_env .env


# Drop cached creds if there are any
sudo -k

# # Try and sudo without password
# if sudo -n whoami &>/dev/null; then
#     echo "Passwordless sudo is enabled. Running random installation scripts not recommended..." 1>&2
#     exit 1
# fi

# Install arduino-cli
A_CLI=third_party/arduino-cli/arduino-cli
if ! test -s $A_CLI &>/dev/null; then
    mkdir -p third_party/arduino-cli
    cd third_party/arduino-cli

    curl -sL https://api.github.com/repos/arduino/arduino-cli/releases/latest \
        | grep "Linux_64bit.tar.gz" \
        | grep browser_download_url \
        | cut -d : -f 2,3 \
        | tr -d \" \
        | tr -d , \
        | wget -q -O arduino-cli.tar.gz -i -
    tar -xf arduino-cli.tar.gz
    rm arduino-cli.tar.gz
    cd ../..
fi

# Install esp32 core
cat > arduino-cli.yml <<- EOF
board_manager:
  additional_urls:
    - https://dl.espressif.com/dl/package_esp32_index.json

sketchbook_path: `realpath .`
arduino_data: `realpath .arduino15`
logging:
    level: info
EOF

A_CLI="`realpath $A_CLI`"
A_CFG="--config-file `realpath arduino-cli.yml`"

$A_CLI $A_CFG core update-index
$A_CLI $A_CFG core install esp32:esp32 --config-file arduino-cli.yml

if ! test -d skeleton || true; then
    $A_CLI $A_CFG sketch new skeleton
    cat > skeleton/skeleton.ino <<- EOF
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "credentials.h"
#include "static/index.html.h"

#define VERSION                 "0.0.1"
#define WAIT_FOR_SERIAL         true
#define BAUD                    115200
#define SERIAL_BUF_LEN          256
#define WIFI_MODE               WIFI_AP_STA
#define WAIT_FOR_WIFI           true
#define WIFI_TIMEOUT_SECONDS    30
#define PIN_LED                 27
#define BLINK_INTERVAL          1000
#define HTTP_PORT               80

// From credentials.h
const char* host            = MDNS_HOST;
const char* ssid            = WIFI_SSID;
const char* password        = WIFI_PASSWORD;

// From static/*.html.h
const char* server_index    = INDEX_HTML;

// Global variables
WebServer server(HTTP_PORT);
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
    server.handleClient();
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

    serial_printf("VERSION: %s\n", VERSION);
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

    server.on("/", HTTP_GET, []() {
        serial_printf("GET /\n");
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", server_index);
    });

    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.setDebugOutput(true);
            Serial.printf("Update: %s\n", upload.filename.c_str());
            if (!Update.begin()) { //start with max available size
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
                Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
            Serial.setDebugOutput(false);
        } else {
            Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
        }
    });

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
EOF

    mkdir -p skeleton/static
    cat > skeleton/static/index.html.h <<- EOF
#define INDEX_HTML "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"
EOF

    ln -s ../credentials.h skeleton/credentials.h
fi

./build.sh skeleton
