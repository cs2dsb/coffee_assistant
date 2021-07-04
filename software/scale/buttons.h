#define BUTTON_INDEX_POWER  0
#define BUTTON_INDEX_TARE   1
#define BUTTON_INDEX_TIMER  2
#define BUTTON_COUNT        3

#define PRESSED             0
#define RELEASED            1
#define SHORT_PRESS         2
#define LONG_PRESS          3

void update_button_state(void);
void configure_buttons(void);
bool is_tare_requested(void);
bool is_calibrate_requested(void);
bool is_power_requested(void);
