/*
 * Per-key LED feedback: when a key is pressed, its LED lights up in a random
 * rainbow colour; after release it fades out over CONFIG_CAKE_PER_KEY_LED_FADE_MS.
 *
 * The LED for each key comes from the cake,per-key-led devicetree node:
 * key-positions[i] is the keymap position of the i-th LED in the chain.
 *
 * &kb_status (behavior_status_leds.c) also uses this strip: while it is held,
 * and for CONFIG_CAKE_PER_KEY_LED_STATUS_HOLD_MS after it is pressed, the LEDs
 * under battery-positions show the battery level as a bar, the LEDs under
 * os-positions show the active OS as a colour, and the LED under the active
 * profile's entry in bt-positions shows whether it is connected, paired or empty,
 * then fade.
 */

#define DT_DRV_COMPAT cake_per_key_led

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/workqueue.h>

#include <cake/per_key_led.h>

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
#include <zmk/battery.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
#error "CONFIG_CAKE_PER_KEY_LED and CONFIG_ZMK_RGB_UNDERGLOW both drive the LED strip; disable one"
#endif

#define STRIP_NODE DT_INST_PHANDLE(0, led_strip)
#define CHAIN_LENGTH DT_PROP(STRIP_NODE, chain_length)

static const uint32_t key_positions[] = DT_INST_PROP(0, key_positions);
#define NUM_LEDS ARRAY_SIZE(key_positions)

BUILD_ASSERT(NUM_LEDS <= CHAIN_LENGTH, "key-positions has more entries than the strip's chain-length");

#if DT_INST_NODE_HAS_PROP(0, battery_positions)
static const uint32_t battery_positions[] = DT_INST_PROP(0, battery_positions);
#define NUM_BATTERY_LEDS ARRAY_SIZE(battery_positions)
#else
static const uint32_t battery_positions[1];
#define NUM_BATTERY_LEDS 0
#endif

#if DT_INST_NODE_HAS_PROP(0, os_positions)
static const uint32_t os_positions[] = DT_INST_PROP(0, os_positions);
#define NUM_OS_LEDS ARRAY_SIZE(os_positions)
#else
static const uint32_t os_positions[1];
#define NUM_OS_LEDS 0
#endif

#if DT_INST_NODE_HAS_PROP(0, bt_positions)
static const uint32_t bt_positions[] = DT_INST_PROP(0, bt_positions);
#define NUM_BT_LEDS ARRAY_SIZE(bt_positions)
#else
static const uint32_t bt_positions[1];
#define NUM_BT_LEDS 0
#endif

#define TICK_MS 16
#define LEVEL_MAX 1000
#define FADE_STEP MAX(1, LEVEL_MAX * TICK_MS / CONFIG_CAKE_PER_KEY_LED_FADE_MS)
#define VALUE_MAX (255 * CONFIG_CAKE_PER_KEY_LED_BRIGHTNESS / 100)

/* Battery bar colours: green above 50%, yellow above 20%, red otherwise. */
#define BATTERY_HUE_HIGH 120
#define BATTERY_HUE_MID 60
#define BATTERY_HUE_LOW 0
/* Percent of full brightness the first bar LED always gets, so 0% still shows. */
#define BATTERY_MIN_FILL 20

/* OS colours, as fractions of full brightness out of 255: white for Mac, pink for Windows. */
static const struct led_rgb os_colours[] = {
    [CAKE_OS_MAC] = {.r = 255, .g = 255, .b = 255},
    [CAKE_OS_WIN] = {.r = 255, .g = 40, .b = 140},
};

/* BT profile colours: blue if connected, orange if paired but not connected, purple if unpaired. */
#define BT_HUE_CONNECTED 240
#define BT_HUE_DISCONNECTED 30
#define BT_HUE_OPEN 280

struct key_led {
    uint16_t hue;   /* 0-359 */
    uint16_t level; /* 0-LEVEL_MAX, scales brightness during fade-out */
    bool held;
};

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct key_led leds[NUM_LEDS];

/* Status display: LED indexes for the battery bar and OS colour, and their state. */
static int battery_leds[NUM_BATTERY_LEDS + 1];
static int os_leds[NUM_OS_LEDS + 1];
static int bt_leds[NUM_BT_LEDS + 1];
static struct {
    uint8_t soc;     /* 0-100 */
    enum cake_os os;
    int8_t bt_profile; /* -1: unknown */
    bool bt_connected;
    bool bt_open;
    uint16_t level;  /* 0-LEVEL_MAX, scales brightness during fade-out */
    bool held;
    int64_t until;   /* uptime the status stays lit until, even if released */
} status;
static struct led_rgb pixels[CHAIN_LENGTH];
static struct k_spinlock lock;
static struct k_work_delayable tick_work;

/* Fully saturated HSV -> RGB, value 0-255. */
static struct led_rgb hue_to_rgb(uint16_t hue, uint8_t value) {
    uint8_t f = (hue % 60) * 255 / 60;
    uint8_t rise = value * f / 255;
    uint8_t fall = value * (255 - f) / 255;

    switch (hue / 60) {
    case 0:
        return (struct led_rgb){.r = value, .g = rise, .b = 0};
    case 1:
        return (struct led_rgb){.r = fall, .g = value, .b = 0};
    case 2:
        return (struct led_rgb){.r = 0, .g = value, .b = rise};
    case 3:
        return (struct led_rgb){.r = 0, .g = fall, .b = value};
    case 4:
        return (struct led_rgb){.r = rise, .g = 0, .b = value};
    default:
        return (struct led_rgb){.r = value, .g = 0, .b = fall};
    }
}

static struct led_rgb scale_rgb(struct led_rgb c, uint8_t value) {
    return (struct led_rgb){.r = c.r * value / 255, .g = c.g * value / 255, .b = c.b * value / 255};
}

static uint16_t battery_hue(uint8_t soc) {
    if (soc > 50) {
        return BATTERY_HUE_HIGH;
    }
    return soc > 20 ? BATTERY_HUE_MID : BATTERY_HUE_LOW;
}

/* Draw the battery bar and OS colour over their LEDs. Caller holds the lock. */
static void draw_status(void) {
    uint16_t hue = battery_hue(status.soc);
    /* How full the bar is, in percent of one LED: N LEDs = N * 100. */
    uint32_t fill = status.soc * NUM_BATTERY_LEDS;

    for (int j = 0; j < NUM_BATTERY_LEDS; j++) {
        if (battery_leds[j] < 0) {
            continue;
        }

        uint32_t start = j * 100;
        uint32_t amount = fill > start ? MIN(fill - start, 100) : 0;
        if (j == 0) {
            amount = MAX(amount, BATTERY_MIN_FILL);
        }

        uint8_t value = VALUE_MAX * amount / 100 * status.level / LEVEL_MAX;
        pixels[battery_leds[j]] = hue_to_rgb(hue, value);
    }

    uint8_t value = VALUE_MAX * status.level / LEVEL_MAX;

    if (status.os != CAKE_OS_UNKNOWN) {
        for (int j = 0; j < NUM_OS_LEDS; j++) {
            if (os_leds[j] >= 0) {
                pixels[os_leds[j]] = scale_rgb(os_colours[status.os], value);
            }
        }
    }

    if (status.bt_profile >= 0 && status.bt_profile < NUM_BT_LEDS &&
        bt_leds[status.bt_profile] >= 0) {
        uint16_t bt_hue = status.bt_connected ? BT_HUE_CONNECTED
                          : status.bt_open    ? BT_HUE_OPEN
                                              : BT_HUE_DISCONNECTED;
        pixels[bt_leds[status.bt_profile]] = hue_to_rgb(bt_hue, value);
    }
}

static void tick_handler(struct k_work *work) {
    bool fading = false;
    k_timeout_t status_wait = K_FOREVER;

    K_SPINLOCK(&lock) {
        for (int i = 0; i < NUM_LEDS; i++) {
            struct key_led *led = &leds[i];

            if (!led->held && led->level > 0) {
                led->level = led->level > FADE_STEP ? led->level - FADE_STEP : 0;
            }
            if (!led->held && led->level > 0) {
                fading = true;
            }

            pixels[i] = hue_to_rgb(led->hue, VALUE_MAX * led->level / LEVEL_MAX);
        }

        int64_t remaining = status.until - k_uptime_get();
        if (status.held) {
            status.level = LEVEL_MAX;
        } else if (remaining > 0) {
            status.level = LEVEL_MAX;
            status_wait = K_MSEC(remaining);
        } else if (status.level > 0) {
            status.level = status.level > FADE_STEP ? status.level - FADE_STEP : 0;
            fading |= status.level > 0;
        }

        if (status.level > 0) {
            draw_status();
        }
    }

    int err = led_strip_update_rgb(strip, pixels, CHAIN_LENGTH);
    if (err < 0) {
        LOG_ERR("Failed to update LED strip (%d)", err);
    }

    /* Only keep ticking while something is fading; presses re-trigger us. */
    if (fading) {
        k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &tick_work, K_MSEC(TICK_MS));
    } else if (!K_TIMEOUT_EQ(status_wait, K_FOREVER)) {
        /* Wake up when a tapped status display should start fading. */
        k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &tick_work, status_wait);
    }
}

static int led_index_for_position(uint32_t position) {
    for (int i = 0; i < NUM_LEDS; i++) {
        if (key_positions[i] == position) {
            return i;
        }
    }
    return -1;
}

static int per_key_led_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    int idx = led_index_for_position(ev->position);
    if (idx < 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    K_SPINLOCK(&lock) {
        if (ev->state) {
            leds[idx].hue = sys_rand32_get() % 360;
            leds[idx].level = LEVEL_MAX;
            leds[idx].held = true;
        } else {
            leds[idx].held = false;
        }
    }

    k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &tick_work, K_NO_WAIT);
    return ZMK_EV_EVENT_BUBBLE;
}

void cake_per_key_led_show_status(bool show, const struct cake_status *new_status) {
    K_SPINLOCK(&lock) {
        if (show) {
#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
            status.soc = zmk_battery_state_of_charge();
#endif
            status.os = new_status->os < ARRAY_SIZE(os_colours) ? new_status->os : CAKE_OS_UNKNOWN;
            status.bt_profile = new_status->bt_profile;
            status.bt_connected = new_status->bt_connected;
            status.bt_open = new_status->bt_open;
            status.until = k_uptime_get() + CONFIG_CAKE_PER_KEY_LED_STATUS_HOLD_MS;
            status.level = LEVEL_MAX;
        }
        status.held = show;
    }

    k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &tick_work, K_NO_WAIT);
}

ZMK_LISTENER(per_key_led, per_key_led_listener);
ZMK_SUBSCRIPTION(per_key_led, zmk_position_state_changed);

static int per_key_led_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device %s is not ready", strip->name);
        return -ENODEV;
    }

    k_work_init_delayable(&tick_work, tick_handler);

    for (int j = 0; j < NUM_BATTERY_LEDS; j++) {
        battery_leds[j] = led_index_for_position(battery_positions[j]);
        if (battery_leds[j] < 0) {
            LOG_WRN("battery-positions entry %d is not in key-positions", battery_positions[j]);
        }
    }

    for (int j = 0; j < NUM_OS_LEDS; j++) {
        os_leds[j] = led_index_for_position(os_positions[j]);
        if (os_leds[j] < 0) {
            LOG_WRN("os-positions entry %d is not in key-positions", os_positions[j]);
        }
    }

    for (int j = 0; j < NUM_BT_LEDS; j++) {
        bt_leds[j] = led_index_for_position(bt_positions[j]);
        if (bt_leds[j] < 0) {
            LOG_WRN("bt-positions entry %d is not in key-positions", bt_positions[j]);
        }
    }

    /* Start dark: the LEDs may power up showing random data. */
    return led_strip_update_rgb(strip, pixels, CHAIN_LENGTH);
}

SYS_INIT(per_key_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
