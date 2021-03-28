#ifndef __UTILS__
#define __UTILS__

#define SERIAL_BUF_LEN 255

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
    serial_printf("FATAL: Restarting...\n");
    ESP.restart();
}

#endif
