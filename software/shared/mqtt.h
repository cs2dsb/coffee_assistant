#define PUBLISH_TIMEOUT_SECONDS    30

void configure_mqtt(const char* url);
void connect_mqtt(void);
bool wait_for_outstanding_publishes(void);
void mqtt_publish(const char* topic, const char* payload);
