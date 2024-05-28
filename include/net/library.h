/* TODO: Handle other events */
#define WPA_SUPP_EVENTS (NET_EVENT_WPA_SUPP_READY)

static struct net_mgmt_event_callback net_wpa_supp_cb;

typedef void (*start_app_callback_t)();
typedef void (*stop_app_callback_t)();

typedef struct {
    start_app_callback_t start_app;
    stop_app_callback_t stop_app;
} app_callbacks_t;

