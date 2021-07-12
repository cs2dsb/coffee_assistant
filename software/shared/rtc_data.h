#include "utils.h"

struct rtc_data {
    /* WiFi channel
        1. if connecting to WiFi, the framework will automatically
           scan if this is 0. Upon shutting down/sleeping, the active
           channel gets stored to this rtc variable to speed up
           subsequent start ups
        2. if connecting to esp-now with scan turned on, it will sweep
           through all the channels broadcasting on esp-now until it
           gets a response from a relay (now node that's also connected
           to WiFi). Once a response is received the channel is stored
           for subsequent boot ups.
    */
    int channel = 0;

    // The bssid of the AP we're connecting to. See channel comment.1
    // for the logic around when it's used/stored.
    uint8_t bssid[6] = {};

    // A copy of the IP data we were previously given by DHCP that is
    // reused as static IP config to speed up subequent boot ups.
    // See channel comment.1 for logic around storage. Ignored if
    // wake_count == DHCP_RENEW_INTERVAL to give DHCP a chance to give
    // us a different address
    struct static_ip_data static_ip = { 0 };

    // How many times device has been woken from deep sleep
    // Incremented in utils/configure_general which should be called by setup
    int wake_count = 0;


    // Used during esp-now channel scanning to indicate when the current, non-
    // zero channel is confirmed to be correct. See channel comment.2
    channel_locked_t channel_locked = false;

    // The mac address of the esp-now bridge we're communicating with. Initially
    // set to the broadcast address tehn changed to the correct mac once a reply
    // has been received
    mac_addr_t bridge_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

rtc_data* get_rtc_data(void);
