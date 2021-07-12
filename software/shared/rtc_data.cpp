#include "rtc_data.h"

RTC_DATA_ATTR rtc_data data;

rtc_data* get_rtc_data(void) {
    return &data;
}
