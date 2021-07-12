#include "config.h"
#include "esp_wifi_types.h"

void configure_wifi(const char *ssid, const char *password, wifi_mode_t mode = DEFAULT_WIFI_MODE, wifi_ps_type_t ps = DEFAULT_WIFI_POWERSAVING_MODE, bool promiscuous = DEFAULT_WIFI_PROMISCUOUS_MODE, bool begin = true);
bool wait_for_connection(void);
void disable_wifi(void);
void print_wifi_status(void);
const char* mac_address(void);
void set_wifi_channel(int channel);
int wifi_channel();
