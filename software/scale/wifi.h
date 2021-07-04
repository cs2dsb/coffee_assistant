#include "config.h"

void configure_wifi(const char *ssid, const char *password);
bool wait_for_connection(void);
void disable_wifi(void);
void print_wifi_status(void);
const char* mac_address(void);
