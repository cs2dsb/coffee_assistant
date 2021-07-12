#include "buttons.h"
#include "config.h"
#include "board.h"
#include "esp32-hal.h"
#include "utils.h"

int button_pin[BUTTON_COUNT] = { PIN_TOUCH_POWER, PIN_TOUCH_TARE, PIN_TOUCH_TIMER };
bool button_state[BUTTON_COUNT][3] = { false };
volatile bool button_isr_state[BUTTON_COUNT] = { false };
unsigned long button_last_released[BUTTON_COUNT] = { 0 };
unsigned long last_button_update_millis = 0;

bool tare_requested = false;
bool calibrate_requested = false;
bool power_requested = false;

void handle_button_action(int button, int action) {
    switch (action) {
        case PRESSED:
            serial_printf("Button %d pressed\n", button);
            break;
        case RELEASED:
            serial_printf("Button %d released\n", button);
            break;
        case SHORT_PRESS:
            serial_printf("Button %d triggered SHORT\n", button);
            break;
        case LONG_PRESS:
            serial_printf("Button %d triggered HOLD\n", button);
            break;
        default:
            serial_printf("UNEXPECTED BUTTON %d ACTION: %d\n", button, action);
            break;
    }

    if (button == BUTTON_INDEX_TARE) {
        if (action == SHORT_PRESS) {
            tare_requested = true;
        } else if (action == LONG_PRESS) {
            calibrate_requested = true;
        }
    }

    if (button == BUTTON_INDEX_POWER) {
        if (action == SHORT_PRESS) {
            power_requested = true;
        }
    }
}

void update_button_state(void) {
    unsigned long now = millis();
    if (now - last_button_update_millis < BUTTON_UPDATE_INTERVAL) {
        return;
    }

    for (int i = 0; i < BUTTON_COUNT; i++) {
        // Shift the button state along
        button_state[i][1] = button_state[i][0];

        // Grab the isr value
        button_state[i][0] = button_isr_state[i];

        // Reset the isr down state only if the button has been released
        // This is because the isr likely won't fire again between
        // iterations to reset it if the button is held.
        // This could all be done without the isrs but by using them
        // you get nice hardware press debouncing for free
        if (touchRead(button_pin[i]) > TOUCH_THRESHOLD) {
            button_isr_state[i] = false;
        }

        bool pressed = button_state[i][0] && !button_state[i][1];
        bool released = !button_state[i][0] && button_state[i][1];
        bool held = button_state[i][0] && button_state[i][1];

        if (pressed) {
            button_last_released[i] = now - BUTTON_UPDATE_INTERVAL;
            handle_button_action(i, PRESSED);
        }

        unsigned long delta = now - button_last_released[i];

        if (released) {
            handle_button_action(i, RELEASED);

            if (delta > BUTTON_PRESS_INTERVAL && !button_state[i][2]) {
                handle_button_action(i, SHORT_PRESS);
            }
            button_state[i][2] = false;
        }

        if (held && delta > BUTTON_LONG_PRESS_INTERVAL && !button_state[i][2]) {
            handle_button_action(i, LONG_PRESS);
            button_state[i][2] = true;
        }
    }
}

void touch_power_isr(void) {
    button_isr_state[BUTTON_INDEX_POWER] = true;
}

void touch_tare_isr(void) {
    button_isr_state[BUTTON_INDEX_TARE] = true;
}

void touch_timer_isr(void) {
    button_isr_state[BUTTON_INDEX_TIMER] = true;
}

void configure_buttons(void) {
    touchAttachInterrupt(PIN_TOUCH_POWER, touch_power_isr, TOUCH_THRESHOLD);
    touchAttachInterrupt(PIN_TOUCH_TARE, touch_tare_isr, TOUCH_THRESHOLD);
    touchAttachInterrupt(PIN_TOUCH_TIMER, touch_timer_isr, TOUCH_THRESHOLD);
}

bool is_tare_requested(void) {
    bool v = tare_requested;
    tare_requested = false;
    return v;
}

bool is_calibrate_requested(void) {
    bool v = calibrate_requested;
    calibrate_requested = false;
    return v;
}

bool is_power_requested(void) {
    bool v = power_requested;
    power_requested = false;
    return v;
}
