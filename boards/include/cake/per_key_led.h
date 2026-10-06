#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util.h>

/* Active OS base layer, as shown by &kb_status. */
enum cake_os {
    CAKE_OS_UNKNOWN = 0,
    CAKE_OS_MAC,
    CAKE_OS_WIN,
};

/* What &kb_status shows besides each half's own battery level. */
struct cake_status {
    enum cake_os os;
    int8_t bt_profile; /* active BT profile index, or -1 if unknown */
    bool bt_connected; /* whether the active profile's host is connected */
    bool bt_open;      /* whether the active profile has no host paired */
};

/*
 * Show (true) or stop showing (false) the status on the per-key LEDs: the
 * battery level bar, the OS colour and the BT profile. When stopped, it
 * fades out like a released key.
 */
#if IS_ENABLED(CONFIG_CAKE_PER_KEY_LED)
void cake_per_key_led_show_status(bool show, const struct cake_status *status);
#else
static inline void cake_per_key_led_show_status(bool show, const struct cake_status *status) {}
#endif
