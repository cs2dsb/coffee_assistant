#include "mqtt.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "utils.h"

esp_mqtt_client_handle_t client;
volatile int outstanding_mqtt_publish_count = 0;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void configure_mqtt(const char* url) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    tcpip_adapter_init();

    esp_mqtt_client_config_t mqtt_cfg = {};
    char *uri = (char*) malloc(strlen(url) + 1);
    strcpy(uri, url);
    mqtt_cfg.uri = uri;

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

void connect_mqtt(void) {
    serial_printf("MQTT2: connecting...\n");
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
}

bool wait_for_outstanding_publishes(void) {
    int timeout = PUBLISH_TIMEOUT_SECONDS * 1000;

    while(timeout > 0 && outstanding_mqtt_publish_count > 0) {
        serial_printf("Waiting for outstanding mqtt publishes: %d (timeout: %d)\n", outstanding_mqtt_publish_count, timeout);
        delay(500);
        timeout -= 500;
    }

    if (outstanding_mqtt_publish_count > 0) {
        serial_printf("Timeout waiting for mqtt publishes: %d\n", outstanding_mqtt_publish_count);
        serial_printf("Restarting...\n");
        return false;
    }
    return true;
}

void mqtt_publish(const char* topic, const char* payload) {
    outstanding_mqtt_publish_count += 1;
    esp_mqtt_client_publish(
        client, topic, payload,
        0, // Calculate len from payload
        1, // QOS
        0); // Retain
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_t*) event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_PUBLISHED:
        serial_printf("MQTT2: MESSAGE PUBLISHED: %d\n", event->msg_id);
        //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        outstanding_mqtt_publish_count -= 1;
        break;
    case MQTT_EVENT_CONNECTED:
        serial_printf("MQTT2: MQTT_EVENT_CONNECTED\n");
        // ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        serial_printf("MQTT2: MQTT_EVENT_DISCONNECTED\n");
    //     ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    // case MQTT_EVENT_SUBSCRIBED:
    //     ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    //     msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
    //     ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    //     break;
    // case MQTT_EVENT_UNSUBSCRIBED:
    //     ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    //     break;
    // case MQTT_EVENT_DATA:
    //     ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    //     printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    //     printf("DATA=%.*s\r\n", event->data_len, event->data);
    //     break;
    case MQTT_EVENT_ERROR:
        serial_printf("MQTT2: MQTT_EVENT_ERROR\n");
    //     ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    //     if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
    //         log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
    //         log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
    //         log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
    //         ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

    //     }
        break;
    default:
        break;
    }
}
