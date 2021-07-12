#include "utils.h"
#include "config.h"
#include "rtc_data.h"

bool __utils_eeprom_init = false;

void configure_general(void) {
    get_rtc_data()->wake_count += 1;
}

int format(char* buf, int len, const char *fmt, ...) {
    va_list pargs;
    va_start(pargs, fmt);
    int chars = vsnprintf(buf, len, fmt, pargs);
    va_end(pargs);
    return chars;
}

void serial_printf(const char *fmt, ...) {
    char buf[SERIAL_BUF_LEN];
    va_list pargs;
    va_start(pargs, fmt);
    vsnprintf(buf, SERIAL_BUF_LEN, fmt, pargs);
    va_end(pargs);
    Serial.print(buf);
}

bool delay_elapsed(unsigned long *last, unsigned long interval) {
    unsigned long now = millis();
    if (*last == 0 || now - *last >= interval) {
        *last = now;
        return true;
    } else {
        return false;
    }
}

void fatal() {
    delay(10000);
    serial_printf("FATAL: Restarting...\n");
    ESP.restart();
}

void fatal_on_fail(bool ok, const char *msg) {
    if (!ok) {
        serial_printf("FATAL: %s\n", msg);
        fatal();
    }
}

// Panic is a fatal logic error in the program, restarting or retrying won't help
void panic_on_fail(bool ok, const char *msg) {
    if (!ok) {
        serial_printf("PANIC: %s\n", msg);
        while (true) {}
    }
}

void format_mac(const uint8_t *mac, char *buf, int len) {
    format(buf, len,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );
}

void setup_eeprom(void) {
    if (!__utils_eeprom_init) {
        __utils_eeprom_init = true;
        EEPROM.begin(EEPROM_SIZE);
    }
}

//TODO: checksums instead of hardcoded valid value
bool is_eeprom_valid(void) {
    setup_eeprom();

    unsigned is_valid = 0;
    EEPROM.get(EEPROM_VAL_VALID_ADDR, is_valid);

    return is_valid == EEPROM_VAL_VALID_VALUE;
}

void wipe_eeprom(void) {
    setup_eeprom();

    unsigned valid = 0;
    EEPROM.put(EEPROM_VAL_VALID_ADDR, valid);
    EEPROM.commit();
}

Filter::Filter(float decay, bool auto_prime) {
    m_decay = decay;
    m_prime = auto_prime;
}

int Filter::count() {
    return m_count;
}

void Filter::push(float v) {
    if (m_prime && m_value == 0.0) {
        reset(v);
    } else {
        m_value += (v - m_value) * m_decay;
    }
}

void Filter::reset(float v) {
    m_value = v;
    m_count = 0;
}

float Filter::value() {
    return m_value;
}

unsigned long log_time(unsigned long last, char* msg) {
    unsigned long now = millis();
    float delta = ((float)(now - last))/1000.0;
    serial_printf("TIME: %.2f\t%s\n", delta, msg);
    return now;
}
