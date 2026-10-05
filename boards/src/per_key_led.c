/*
 * Per-key LED feedback: when a key is pressed, its LED lights up in a random
 * rainbow colour; after release it fades out over CONFIG_CAKE_PER_KEY_LED_FADE_MS.
 *
 * The LED for each key comes from the cake,per-key-led devicetree node:
 * key-positions[i] is the keymap position of the i-th LED in the chain.
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

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
#error "CONFIG_CAKE_PER_KEY_LED and CONFIG_ZMK_RGB_UNDERGLOW both drive the LED strip; disable one"
#endif

#define STRIP_NODE DT_INST_PHANDLE(0, led_strip)
#define CHAIN_LENGTH DT_PROP(STRIP_NODE, chain_length)

static const uint32_t key_positions[] = DT_INST_PROP(0, key_positions);
#define NUM_LEDS ARRAY_SIZE(key_positions)

BUILD_ASSERT(NUM_LEDS <= CHAIN_LENGTH, "key-positions has more entries than the strip's chain-length");

#define TICK_MS 16
#define LEVEL_MAX 1000
#define FADE_STEP MAX(1, LEVEL_MAX * TICK_MS / CONFIG_CAKE_PER_KEY_LED_FADE_MS)
#define VALUE_MAX (255 * CONFIG_CAKE_PER_KEY_LED_BRIGHTNESS / 100)

struct key_led {
    uint16_t hue;   /* 0-359 */
    uint16_t level; /* 0-LEVEL_MAX, scales brightness during fade-out */
    bool held;
};

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct key_led leds[NUM_LEDS];
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

static void tick_handler(struct k_work *work) {
    bool fading = false;

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
    }

    int err = led_strip_update_rgb(strip, pixels, CHAIN_LENGTH);
    if (err < 0) {
        LOG_ERR("Failed to update LED strip (%d)", err);
    }

    /* Only keep ticking while something is fading; presses re-trigger us. */
    if (fading) {
        k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &tick_work, K_MSEC(TICK_MS));
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

ZMK_LISTENER(per_key_led, per_key_led_listener);
ZMK_SUBSCRIPTION(per_key_led, zmk_position_state_changed);

static int per_key_led_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device %s is not ready", strip->name);
        return -ENODEV;
    }

    k_work_init_delayable(&tick_work, tick_handler);

    /* Start dark: the LEDs may power up showing random data. */
    return led_strip_update_rgb(strip, pixels, CHAIN_LENGTH);
}

SYS_INIT(per_key_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
