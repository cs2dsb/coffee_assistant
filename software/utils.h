#include <EEPROM.h>

#ifndef __UTILS__
#define __UTILS__

#define SERIAL_BUF_LEN 255

// EEPROM is actually emulated using a flash 'partition' that's bigger than 512
#define EEPROM_SIZE                     512

// This address + magic value is used to determine if the contents of the EEPROM
// has been set by the app or if it's random junk
#define EEPROM_VALID_ADDR               0
#define EEPROM_VALID_VALUE              0xCAFEBEEF

#define EEPROM_FIRST_ADDR               (EEPROM_VALID_ADDR + sizeof(unsigned))

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

void format_mac(const uint8_t *mac, char *buf, int len) {
    format(buf, len,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );
}

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
void write_to_eeprom(int addr, const T &value, bool commit = true) {
    setup_eeprom();

    EEPROM.put(addr, value);

    if (commit) {
        unsigned valid = EEPROM_VALID_VALUE;
        EEPROM.put(EEPROM_VALID_ADDR, valid);
        EEPROM.commit();
    }
}

#endif
