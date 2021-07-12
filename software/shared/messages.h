#define WEIGHT_PREFIX               "weight:"
#define EXTRACT_COMMAND_PREFIX      "do-extract:"
#define CAL_COMMAND_PREFIX          "do-calibrate"
#define TARE_COMMAND_PREFIX         "do-tare"
#define START_TIMER_PREFIX          "start-timer"
#define STOP_TIMER_PREFIX           "stop-timer"
#define HEARTBEAT_COMMAND_PREFIX    "heartbeat"

struct weight_message {
    int weight_mg;
    int time_ms;
};

bool has_prefix(const char* data, const char* prefix);
bool extract_field(const char* data, int field_number, int* result);
bool is_weight_message(const char* data);
bool extract_weight_message(const char* data, weight_message* result);
