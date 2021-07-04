#ifndef UTILS
#define UTILS

#include <EEPROM.h>

#define SERIAL_BUF_LEN 255

// EEPROM is actually emulated using a flash 'partition' that's bigger than 512
#define EEPROM_SIZE                     512

// This address + magic value is used to determine if the contents of the EEPROM
// has been set by the app or if it's random junk
#define EEPROM_VALID_ADDR               0
#define EEPROM_VALID_VALUE              0xCAFEBEEF

#define EEPROM_FIRST_ADDR               (EEPROM_VALID_ADDR + sizeof(unsigned))


int format(char* buf, int len, const char *fmt, ...);

void serial_printf(const char *fmt, ...);

bool delay_elapsed(unsigned long *last, unsigned long interval);

void fatal();

void fatal_on_fail(bool ok, const char *msg);

// Panic is a fatal logic error in the program, restarting or retrying won't help
void panic_on_fail(bool ok, const char *msg);

void format_mac(const uint8_t *mac, char *buf, int len);


#ifdef __ESP_NOW_H__
void print_esp_now_result(esp_err_t result, char *prefix);
#endif

void setup_eeprom(void);

bool is_eeprom_valid(void);

void wipe_eeprom(void);

template<typename T>
bool read_from_eeprom(int addr, T &out);

template<typename T>
void write_to_eeprom(int addr, const T &value, bool commit = true);

class Filter {
    private:
        float m_value;
        bool m_prime;
    public:
        float m_decay;
        Filter(float decay = 0.1, bool auto_prime = true);
        void push(float v);
        void reset(float v);
        float value();
};


unsigned long log_time(unsigned long last, char* msg);

#endif
