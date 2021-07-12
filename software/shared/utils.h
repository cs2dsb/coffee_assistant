#ifndef UTILS
#define UTILS

#include <EEPROM.h>
#include "config.h"

typedef int channel_t;
typedef bool channel_locked_t;
typedef uint8_t mac_addr_t[6];
struct static_ip_data {
    uint32_t ip;
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns;
};


void configure_general(void);

int format(char* buf, int len, const char *fmt, ...);

void serial_printf(const char *fmt, ...);

bool delay_elapsed(unsigned long *last, unsigned long interval);

void fatal();

void fatal_on_fail(bool ok, const char *msg);

// Panic is a fatal logic error in the program, restarting or retrying won't help
void panic_on_fail(bool ok, const char *msg);

void format_mac(const uint8_t *mac, char *buf, int len);

void setup_eeprom(void);

bool is_eeprom_valid(void);

void wipe_eeprom(void);

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
        unsigned valid = EEPROM_VAL_VALID_VALUE;
        EEPROM.put(EEPROM_VAL_VALID_ADDR, valid);
        EEPROM.commit();
    }
}

class Filter {
    private:
        float m_value;
        bool m_prime;
        int m_count;
    public:
        float m_decay;
        Filter(float decay = 0.1, bool auto_prime = true);
        void push(float v);
        void reset(float v);
        float value();
        int count();
};


unsigned long log_time(unsigned long last, char* msg);

#endif
