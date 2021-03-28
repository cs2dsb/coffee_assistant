# Software

## scale

### Todo

*   Captive portal
*   mDNS service
*   use SPIFFS for storing gzipped assets
    *   test is working, update auto generated routes to serve these
    *   esp32 panics if spiffs file doesn't exist, probably check in the handler?
    *   brotli will only work over https

## controller

## Notes

*   [Using this dev board](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html)
    ![Pinout](../datasheets/esp32-devkit-c-v4.jpg)
*   `make app-flash` to flash just the app
*   `make monitor` to watch serial
*   `Ctrl-]` to exit `monitor`
