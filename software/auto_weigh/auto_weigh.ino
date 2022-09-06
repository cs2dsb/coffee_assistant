#include <stdlib.h>
#include <stdarg.h>
#include <ESPmDNS.h>
#include <FS.h>
#define SPIFFS LITTLEFS
#include <LITTLEFS.h>
#include <EEPROM.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <TM1638plus.h>
#include <HX711_ADC.h>
#include "credentials.h"
#include "static/routes.h"
#include "handlers.h"
#include "utils.h"
#include "board.h"
#include "analog.h"
#include "wifi.h"
#include "mqtt.h"
#include "config.h"
#include "buttons.h";
#include "now.h";
#include "messages.h";

#define PIEZO_PIN   19
#define PIEZO_FREQ  5000
#define PIEZO_CHAN  0
#define PIEZO_RES   8
#define BEEP_MILLIS 500

#define STEP_LOW_FREQ  1
#define STEP_HIGH_FREQ 1500
#define STEP_CHAN      1
#define STEP_RES       8

#define TARGET_WEIGHT  15.9
#define LERP_WEIGHT    TARGET_WEIGHT*0.9

#define STEP_PIN    32
#define DIR_PIN     33
#define EN_PIN      25

bool run_stepper                    = false;
unsigned long step_delay_micros     = 2000;
unsigned long step_remaining_micros = 0;
unsigned long last_step_micros      = 0;
bool step_polarity                  = false;
bool dir_polarity                   = true;
float target_weight                 = 0;
int last_step_freq                  = 0;

// How much the calibration weight is
float known_mass            = KNOWN_CALIBRATION_MASS_DEFAULT;

struct measurement {
    float weight;
    float time;
    bool timer_running;
    float ema2;
    float ema3;
};

// Global variables
unsigned long last_blink_millis = 0;

unsigned long last_weight_poll_millis = 0;
float weight_last_change_time         = 0.0;

float weight_new    = 0.0;
float weight_start  = 0.0;
float weight_peak   = 0.0;
float weight_ema_1  = 0.0;
float weight_ema_2  = 0.0;
float weight_ema_3  = 0.0;
float weight_ema_4  = 0.0;
float last_weight   = 0.0;


// last time various periodic functions were run
unsigned long last_serial_log_millis = 0;
unsigned long last_ws_message_millis = 0;
unsigned long last_wifi_status_update_millis = 0;
unsigned long last_elapsed_millis = 0;
unsigned long last_timer_update_millis = 0;
unsigned long last_tare_millis = 0;
unsigned long last_cal_millis = 0;
unsigned long last_tm1638_update_millis = 0;
unsigned long last_batt_broadcast_millis = 0;
unsigned long end_beep_millis = 0;
unsigned long last_weighed_beans = 0;

// elapsed_millis is updated every time loop is called
unsigned long elapsed_millis = 0;
// elapsed_seconds is updated from elapsed_millis every time TIMER_INTERVAL expires
float elapsed_seconds = 0;
// timer is assumed not to be running unless weight is being added to the scale
bool timer_running = false;

bool tare = false;
bool calibrate = false;
bool user_wake = false;

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws_server("/ws");

HX711_ADC hx711(PIN_HX711_DT, PIN_HX711_SCK);
TM1638plus tm1638(PIN_TM1638_STB, PIN_TM1638_CLK, PIN_TM1638_DIO, TM1638_HIGH_FREQ);

void setup() {
    setup_serial();
    read_wakeup_reason();
    // Skip sleep during dev
    user_wake = true;

    configure_and_calibrate_adc1();
    panic_on_fail(configure_analog_pins(), "Failed to configure analog pins");

    setup_step_pwm();

    setup_gpio();
    configure_buttons();

    //setup_piezo();

    // Read it before starting any heavy current consuming things
    read_batt_until_reliable();

    setup_eeprom();
    setup_spiffs();

    if (user_wake) {
        setup_tm1638();
        setup_hx711();
    }

    unsigned long start = millis();
    unsigned long last = start;


    configure_wifi(NULL, NULL, WIFI_STA, WIFI_PS_MIN_MODEM, false, false);
    configure_now(true, false, false);
    now_set_on_receive(&now_on_receive);

    configure_wifi(WIFI_SSID, WIFI_PASSWORD);
    // last = log_time(last, "configure_wifi");

    // // scan is off currently because there's a direct wifi connection, which would conflict
    // // with the channel switching of the scan
    // configure_now();
    // last = log_time(last, "configure_now");

    setup_server();
    // last = log_time(last, "setup_server");

    // configure_mqtt(MQTT_URL);
    // last = log_time(last, "configure_mqtt");

    if (user_wake) {
        // Wait while server starts up to let the hx711 stabilize before tareing
        tare_hx711(true);
    }

    if (!user_wake) {
        // fatal_on_fail(wait_for_connection(), "failed to connect to wifi after timeout");
        // last = log_time(last, "wait_for_connection");

        // connect_mqtt();
        // last = log_time(last, "connect_mqtt2");

        // send_mqtt_data();
        // last = log_time(last, "send_mqtt_data");

        // wait_for_outstanding_publishes();
        // last = log_time(last, "outstanding_mqtt_publish_count > 0");

        // unsigned long delta = millis() - start;

        // serial_printf("connect+send time = %.2f\n", ((float)delta)/1000.);
        sleep();
    }
}

void loop() {
    AsyncElegantOTA.loop();
    ws_server.cleanupClients();

    if (user_wake) {
        update_tm1638();
        poll_hx711();
        update_button_state();
        handle_buttons();
    }

    update_elapsed();
    update_serial();
    sample_to_filters();
    print_wifi_status();
    // now_update();

    update_tone();

    update_stepper();
    update_weigh_out_beans();
}

void update_weigh_out_beans(void) {
    if (target_weight > 0 && delay_elapsed(&last_weighed_beans, 500)) {
        auto lerp_over = LERP_WEIGHT;
        auto delta = target_weight - weight_new;

        auto alpha = delta / lerp_over;
        if (alpha > 1.0) { alpha = 1.0; }
        else if (alpha < 0.0) { alpha = 0.0; }

        auto lerp = STEP_LOW_FREQ + alpha * (STEP_HIGH_FREQ - STEP_LOW_FREQ);
        set_step_freq(lerp);

        if (delta > 0) {
            serial_printf("Weight remaining: %f\n", delta);
            run_stepper = true;
        } else {
            run_stepper = false;
            target_weight = 0;
        }
    }
}

void update_stepper(void) {
    enable_stepping(run_stepper);
    // unsigned long now = micros();
    // unsigned long elapsed = now - last_step_micros;
    // last_step_micros = now;

    // if (run_stepper) {
    //     // serial_printf("step_remaining_micros: %lu, elapsed: %lu\n", step_remaining_micros, elapsed);
    //     if (step_remaining_micros > elapsed) {
    //         step_remaining_micros -= elapsed;
    //     } else {
    //         step_polarity = !step_polarity;
    //         digitalWrite(STEP_PIN, step_polarity);
    //         step_remaining_micros = step_delay_micros;
    //     }
    // } else {
    //     step_remaining_micros = 0;
    // }
}

void handle_buttons() {
    if (is_tare_requested()) {
        tare = true;
    }

    if (is_calibrate_requested()) {
        calibrate = true;
    }

    if (is_power_requested() &&
        // Prevent sleep immidately after powering on
        last_elapsed_millis > 2000)
    {
        serial_printf("power button pressed. going to sleep\n");
        sleep();
    }

    if (is_timer_requested()) {
        //run_stepper = true;
        target_weight = TARGET_WEIGHT;
    }
}

void sleep(void) {
    disable_wifi();

    // "Turn off" display (mosfet that would actually kill it is incorrect on prototype)
    tm1638.displayText("        ");

    serial_printf("sleeping now\n");
    esp_sleep_enable_touchpad_wakeup();
    uint64_t sleep_us = DEEP_SLEEP_SECONDS * 1000 * 1000ULL;
    esp_sleep_enable_timer_wakeup(sleep_us);
    esp_deep_sleep_start();
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

    serial_printf("%s v%s (%s). MAC: %s\n", PROJECT, VERSION, BUILD_TIMESTAMP, mac_address());
    setCpuFrequencyMhz(CPU_MHZ);
    serial_printf("CPU running at %lu\n", getCpuFrequencyMhz());
}

void setup_piezo(void) {
    // setup pwm for channel
    ledcSetup(PIEZO_CHAN, PIEZO_FREQ, PIEZO_RES);

    // attach the pin to the pwm channel
    ledcAttachPin(PIEZO_PIN, PIEZO_CHAN);

    set_piezo_duty(0);
}

void set_step_freq(int freq) {
    if (freq != last_step_freq) {
        if (freq >= last_step_freq) {
            last_step_freq = last_step_freq + (freq - last_step_freq) * 0.5;
            if (last_step_freq * 1.05 >= freq) {
                last_step_freq = freq;
            }
        } else {
            last_step_freq = freq;
        }

        // double actual_freq = ledcChangeFrequency(STEP_CHAN, freq, STEP_RES);
        double actual_freq = ledcSetup(STEP_CHAN, last_step_freq, STEP_RES);
        serial_printf("Actual step frequency: %f\n", actual_freq);
    }
}

void setup_step_pwm(void) {
    // double actual_freq = ledcSetup(STEP_CHAN, STEP_LOW_FREQ, STEP_RES);
    // serial_printf("Actual step frequency: %f\n", actual_freq);
    set_step_freq(STEP_LOW_FREQ);

    // attach the pin to the pwm channel
    ledcAttachPin(STEP_PIN, STEP_CHAN);

    enable_stepping(false);
}

void set_piezo_duty(int d) {
    ledcWrite(PIEZO_CHAN, d);
}

void enable_stepping(bool en) {
    // serial_printf("enable_stepping: %s\n", en ? "true" : "false" );
    if (en) {
        ledcWrite(STEP_CHAN, 1<<(STEP_RES-1));
    } else {
        ledcWrite(STEP_CHAN, 0);
    }
}

void beep(void) {
    end_beep_millis = millis() + BEEP_MILLIS;
    set_piezo_duty(128);
}

void update_tone(void) {
    if (millis() > end_beep_millis) {
        set_piezo_duty(0);
    }
}

void setup_server(void) {
    serial_printf("Starting mDNS responder on %s.local\n", MDNS_HOST);
    MDNS.begin(MDNS_HOST);

    // Configure websockets
    ws_server.onEvent(on_websocket_event);
    server.addHandler(&ws_server);

    // Configure static files
    if (USE_BUILTIN_STATIC) {
        server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    } else {
        register_routes(&server);
    }

    // Configure OTA
    AsyncElegantOTA.begin(&server);

    // 404 handler
    server.onNotFound([](AsyncWebServerRequest *request) {
        serial_printf("GET %s 404\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
    });

    server.begin();
    MDNS.addService("http", "tcp", HTTP_PORT);
    serial_printf("HTTP listening on port %d\n", HTTP_PORT);
}

void setup_gpio(void) {
    pinMode(PIN_EN_BATT_DIVIDER, OUTPUT);
    digitalWrite(PIN_EN_BATT_DIVIDER, HIGH);


    pinMode(EN_PIN, OUTPUT);
    //pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);

    digitalWrite(DIR_PIN, dir_polarity);
    //digitalWrite(STEP_PIN, step_polarity);
    digitalWrite(EN_PIN, LOW);
}

void setup_hx711(void) {
    float cal = get_cal_from_eeprom();
    hx711.setCalFactor(cal);
    hx711.begin();
    hx711.setSamplesInUse(1);
    hx711.start(0);
    hx711.update();
}

void setup_tm1638(void) {
    tm1638.displayBegin();
    tm1638.displayText("booted");
    tm1638.brightness(1);

    // All LEDs OFF
    tm1638.setLEDs(0x0000);
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

void poll_hx711(void) {
    if (hx711.update()) {
        if (true || delay_elapsed(&last_weight_poll_millis, 10)) {
        //if (delay_elapsed(&last_weight_poll_millis, HX711_POLL_INTERVAL)) {

            if (tare || calibrate) {
                // Reset the timer
                reset_timer();
            }

            if (tare) {
                tare_hx711(false);
            }

            if (hx711.getTareStatus()) {
                serial_printf("tare complete\n");

                // Clear out the averages
                weight_ema_1 = 0.0;
                weight_ema_2 = 0.0;
                weight_ema_3 = 0.0;
                weight_ema_4 = 0.0;

                // Make sure the timer isn't running
                reset_timer();

                ws_server.textAll("{ \"tare\": true }");
                now_send_to_bridge(TARE_COMMAND_PREFIX);
            }

            if (calibrate) {
                while (!hx711.update()) {}

                // Clear out existing averaging values
                hx711.setSamplesInUse(16);
                hx711.refreshDataSet();

                // Reset cal to prevent NaN/Inf from propagating
                hx711.setCalFactor(1.0);

                float calibration = hx711.getNewCalibration(known_mass);
                hx711.setSamplesInUse(1);
                serial_printf("new calibration: %f\n", calibration);
                save_cal_to_eeprom(calibration);

                char buf[SERIAL_BUF_LEN];
                format(buf, SERIAL_BUF_LEN, "{ \"calibration_value\": %.3f }", calibration);
                ws_server.textAll(buf);

                calibrate = false;
            }

            weight_new = hx711.getData();

            if (weight_new > (last_weight + 0.1)) {
                beep();
                last_weight = weight_new;
                // run_stepper = false;
            } else if (weight_new < last_weight) {
                last_weight = weight_new;
            }

            //serial_printf("weight_new: %f\n", weight_new);


            // ema 1 is just a gentle smoothing to remove noise/drips/etc
            weight_ema_1 += (weight_new - weight_ema_1) * 1;

            // // ema 2 and 3 are used to detect if there is a changing weight
            // weight_ema_2 += (weight_ema_1 - weight_ema_2) * WEIGHT_EMA_FACTOR_2;
            // weight_ema_3 += (weight_ema_1 - weight_ema_3) * WEIGHT_EMA_FACTOR_3;

            // // ema 4 is the source for the weight_start value
            // weight_ema_4 += (weight_ema_1 - weight_ema_4) * WEIGHT_EMA_FACTOR_4;

            // // Only detect increasing weight for starting
            // bool start = weight_ema_2 > (weight_ema_3 + WEIGHT_START_DELTA);
            // // But any change thereafter should keep the run going (the weight
            // // tends to bounce a bit as drips land and cause the scale to wobble)
            // bool change = abs(weight_ema_2 - weight_ema_3) > WEIGHT_HYSTERESIS;

            // if (!timer_running && start) {
            //     reset_timer();
            //     timer_running = true;
            //     weight_peak = 0.0;

            //     // More fudging required here perhaps?
            //     weight_start = weight_ema_4;
            // }

            // if (timer_running) {
            //     if (change) {
            //         weight_last_change_time = elapsed_seconds;
            //         if (weight_ema_1 > weight_peak) {
            //             weight_peak = weight_ema_1;
            //         }
            //     } else if (elapsed_seconds - weight_last_change_time > WEIGHT_IDLE_TIMEOUT) {
            //         timer_running = false;

            //         char buf[SERIAL_BUF_LEN];
            //         format(buf, SERIAL_BUF_LEN,
            //             "{ \"total_time\": %.1f, \"weight\": %.3f, \"weight_peak\": %.3f, \"weight_start\": %.3f }",
            //             weight_last_change_time,
            //             weight_ema_1,
            //             weight_peak,
            //             weight_start
            //         );

            //         // Skip the slow updating in case a new weight starts imminently
            //         weight_ema_4 = weight_ema_1;

            //         ws_server.textAll(buf);
            //     }
            // }

            // send_websocket_measurement(measurement {
            //     weight: weight_ema_1,
            //     time: elapsed_seconds,
            //     timer_running: timer_running,
            //     ema2: weight_ema_2,
            //     ema3: weight_ema_3,
            // });
        }
    }
}

void update_serial(void) {
    if (delay_elapsed(&last_serial_log_millis, SERIAL_LOG_INTERVAL)) {
        //serial_printf("Time: %.1f   Weight: %.1f\n", elapsed_seconds, weight_ema_1);
    }
}

void update_elapsed(void) {
    unsigned long now = millis();
    unsigned long delta = now - last_elapsed_millis;
    last_elapsed_millis = now;

    if (timer_running) {
        elapsed_millis += delta;

        if (delay_elapsed(&last_timer_update_millis, TIMER_INTERVAL)) {
            elapsed_seconds = ((float)elapsed_millis) / 1000.0;
        }
    }
}

void read_batt_until_reliable(void) {
    delay(200);
    for (int i = 0; i < 10; i++) {
        sample_to_filters();
    }
}

void update_tm1638(void) {
    if (delay_elapsed(&last_tm1638_update_millis, 100)) { //TM1638_UPDATE_INTERVAL)) {
        long seconds = elapsed_millis / 1000;
        while (seconds > 9999) {
            seconds -= 9999;
        }

        char buf[SERIAL_BUF_LEN];

        uint16_t v =  touchRead(PIN_TOUCH_POWER);
        format(buf, SERIAL_BUF_LEN, "%5.1f%4d", weight_ema_1, seconds);

        if (calibrate) {
            format(buf, SERIAL_BUF_LEN, "CAL     ");
        } else if (tare) {
            format(buf, SERIAL_BUF_LEN, "88888888");
        }

        tm1638.displayText(buf);
    }
}

void read_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD :
        Serial.println("Wakeup caused by touchpad");
        user_wake = true;
        break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

float get_cal_from_eeprom(void) {
    unsigned is_valid;
    EEPROM.get(EEPROM_VAL_VALID_ADDR, is_valid);

    float cal = DEFAULT_CALIBRATION;

    if (is_valid == EEPROM_VAL_VALID_VALUE) {
        EEPROM.get(EEPROM_CAL_ADDR, cal);
    }

    return cal;
}

void save_cal_to_eeprom(float cal) {
    unsigned is_valid = EEPROM_VAL_VALID_VALUE;

    EEPROM.put(EEPROM_CAL_ADDR, cal);
    EEPROM.put(EEPROM_VAL_VALID_ADDR, is_valid);
    EEPROM.commit();
}

bool is_command(const char* command, char* data) {
    size_t len = strlen(command);
    bool same = strncmp(command, data, len) == 0;
    // serial_printf("Comparing first %d chars of '%s' and '%s' = %s\n",
    //     len,
    //     command,
    //     data,
    //     same? "true" : "false"
    // );
    return same;
}

void on_websocket_event(
    AsyncWebSocket * server,
    AsyncWebSocketClient * client,
    AwsEventType type,
    void* arg,
    uint8_t *data,
    size_t len
) {
    unsigned id = client->id();
    if (type == WS_EVT_CONNECT) {
        serial_printf("WS (id: %u) CONNECTED\n", id);
    } else if (type == WS_EVT_DISCONNECT) {
        serial_printf("WS (id: %u) DISCONNECTED\n", id);
    } else if (type == WS_EVT_ERROR) {
        serial_printf("WS (id: %u) ERROR (%u). DATA: %s\n", id, *((uint16_t*)arg), (char*)data);
    } else if (type == WS_EVT_PONG) {
        serial_printf("WS (id: %u) PONG\n", id);
    } else if (type == WS_EVT_DATA) {
        serial_printf("WS (id: %u) DATA: %s\n", id, len > 0 ? (char*) data: "");

        if (is_command(CAL_COMMAND_PREFIX, (char*)data)) {
            serial_printf("WS (id: %u) requested CALIBRATION\n", id);
            calibrate = true;
        } else if (is_command(TARE_COMMAND_PREFIX, (char*)data)) {
            serial_printf("WS (id: %u) requested TARE\n", id);
            tare = true;
        }
    }
}

void reset_timer(void) {
    elapsed_millis = 0;
    elapsed_seconds = 0;
    timer_running = false;
}

void send_websocket_measurement(measurement m) {
    if (delay_elapsed(&last_ws_message_millis, WS_MESSAGE_INTERVAL)) {
        char buf[SERIAL_BUF_LEN];
        format(buf, SERIAL_BUF_LEN,
            "{ \"time\": %.1f, \"weight\": %.3f, \"timer_running\": %s, \"ema2\": %.3f, \"ema3\": %.3f }",

            m.time,
            m.weight,
            timer_running ? "true" : "false",
            m.ema2,
            m.ema3
        );
        ws_server.textAll(buf);
        //now_broadcast(buf);

        format(buf, SERIAL_BUF_LEN,
            "%s%d:%d",
            WEIGHT_PREFIX,
            (int)(m.weight * 1000),
            (int)(m.time * 1000));

        now_send_to_bridge(buf);
    }
}

void send_mqtt_data(void) {
    float batt_v = get_filtered_batt_voltage();
    float vbus_v = get_filtered_vbus_voltage();

    char buf[SERIAL_BUF_LEN];

    format(buf, SERIAL_BUF_LEN,
        "{ \"project\": \"%s\", \"build:\": \"%s\", \"mac\": \"%s\", \"battery\": %.2f, \"vbus\": %.2f }",
        PROJECT,
        BUILD_TIMESTAMP,
        mac_address(),
        batt_v,
        vbus_v
    );

    serial_printf("sending batt_v: %.2f\n", batt_v);
    serial_printf("%s\n", buf);

    mqtt_publish(MQTT_TOPIC, buf);
}

void now_on_receive(const uint8_t *mac, const uint8_t *data, int len) {
    char now_buf[ESP_NOW_MAX_DATA_LEN + 1];
    strncpy(now_buf, (const char *)data, len);

    now_buf[len] = '\0';

    char mac_buf[18];
    format_mac(mac, mac_buf, sizeof(mac_buf));

    char json_buf[JSON_BUF_LEN];

    if (has_prefix((const char*) now_buf, TARE_COMMAND_PREFIX)) {
        tare = true;
    } else if (has_prefix((const char*) now_buf, START_TIMER_PREFIX)) {
        reset_timer();
        timer_running = true;
        serial_printf("starting timer\n");
    } else if (has_prefix((const char*) now_buf, STOP_TIMER_PREFIX)) {
        timer_running = false;
        serial_printf("stopping timer\n");
    } else {
        serial_printf("%s\n", now_buf);
    }
}
