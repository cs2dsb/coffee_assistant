#include <stdint.h>
#include <stddef.h>
#include "esp_now.h"
#include "WString.h"

using receive_callback_fn = void (*)(const uint8_t *mac, const uint8_t *data, int len);

bool configure_now(bool with_scan = false, bool as_relay = false, bool with_heartbeat = false);
bool now_send(const uint8_t* message, size_t len, uint8_t mac[6]);
bool now_send_to_bridge(const uint8_t* message, size_t len);
bool now_send_to_bridge(const String message);
bool now_broadcast(const uint8_t* message, size_t len);
bool now_broadcast(const String message);
void now_update(void);
void now_set_on_receive(receive_callback_fn callback);
int now_latency(void);
