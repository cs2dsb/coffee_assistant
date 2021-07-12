#include <string.h>
#include <stdlib.h>
#include "messages.h"

bool extract_field(const char* data, int field_number, int* result) {
    int i = 0;
    for (int f = 0; f < field_number; f += 1) {
        bool found = false;
        for (; i < strlen(data); i += 1) {
            if (data[i] == ':') {
                found = true;
                i += 1;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    *result = atoi(&data[i]);
    return true;
}

bool has_prefix(const char* data, const char* prefix) {
    size_t len = strlen(prefix);
    return strncmp(prefix, data, len) == 0;
}

bool is_weight_message(const char* data) {
    return has_prefix(data, WEIGHT_PREFIX);
}

bool extract_weight_message(const char* data, weight_message* result) {
    if (!is_weight_message(data)) {
        return false;
    }

    if (!extract_field(data, 1, &result->weight_mg)) {
        return false;
    }

    if (!extract_field(data, 2, &result->time_ms)) {
        return false;
    }

    return true;
}
