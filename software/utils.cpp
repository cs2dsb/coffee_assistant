#include "utils.h"

bool __utils_eeprom_init = false;

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

    if (now - *last >= interval) {
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


#ifdef __ESP_NOW_H__
void print_esp_now_result(esp_err_t result, char *prefix) {
    if (result == ESP_OK) {
        serial_printf("%s: success\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
        serial_printf("%s: espnow not init\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_ARG) {
        serial_printf("%s: invalid argument\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
        serial_printf("%s: internal error\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
        serial_printf("%s: no mem\n", prefix);
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
        serial_printf("%s: peer not found\n", prefix);
    } else {
        serial_printf("%s: unknown error %d\n", prefix, result);
    }
}
#endif

void setup_eeprom(void) {
    if (!__utils_eeprom_init) {
        __utils_eeprom_init = true;
        EEPROM.begin(EEPROM_SIZE);
    }
}

bool is_eeprom_valid(void) {
    setup_eeprom();

    unsigned is_valid = 0;
    EEPROM.get(EEPROM_VALID_ADDR, is_valid);

    return is_valid == EEPROM_VALID_VALUE;
}

void wipe_eeprom(void) {
    setup_eeprom();

    unsigned valid = 0;
    EEPROM.put(EEPROM_VALID_ADDR, valid);
    EEPROM.commit();
}

template<typename T>
bool read_from_eeprom(int addr, T &out) {
    setup_eeprom();

    if (is_eeprom_valid()) {
        EEPROM.get(addr, out);
        return true;
    } else {
        return false;
    }
}

template<typename T>
void write_to_eeprom(int addr, const T &value, bool commit) {
    setup_eeprom();

    EEPROM.put(addr, value);

    if (commit) {
        unsigned valid = EEPROM_VALID_VALUE;
        EEPROM.put(EEPROM_VALID_ADDR, valid);
        EEPROM.commit();
    }
}


Filter::Filter(float decay, bool auto_prime) {
    m_decay = decay;
    m_prime = auto_prime;
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
