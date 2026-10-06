/*
 * &kb_status: show the battery level, active OS and BT profile on the per-key
 * LEDs while held.
 *
 * Global locality: the central runs it locally and on every peripheral, so
 * each half shows its own battery level. Only the central knows the active
 * layers and BT profile, so it puts the OS in param1 and the profile in
 * param2 before the binding is forwarded.
 * Builds without per-key LEDs (the dongle) still need the behavior so they
 * can forward it to the halves.
 */

#define DT_DRV_COMPAT cake_behavior_status_leds

#include <zephyr/device.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>

/* Only the central has a keymap; peripherals get the OS from the central. */
#define HAS_KEYMAP (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

#if HAS_KEYMAP
#include <zmk/keymap.h>
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#endif
#endif

#include <cake/per_key_led.h>

struct behavior_status_leds_config {
    int win_layer; /* -1: don't show the OS */
};

/* param2: 0 if the BT profile is unknown, else (index + 1) | BT_CONNECTED | BT_OPEN. */
#define BT_PROFILE_MASK 0xff
#define BT_CONNECTED BIT(8)
#define BT_OPEN BIT(9)

static struct cake_status decode_status(const struct zmk_behavior_binding *binding) {
    uint32_t bt = binding->param2;

    return (struct cake_status){
        .os = binding->param1,
        .bt_profile = (int8_t)(bt & BT_PROFILE_MASK) - 1,
        .bt_connected = bt & BT_CONNECTED,
        .bt_open = bt & BT_OPEN,
    };
}

#if HAS_KEYMAP
static int on_keymap_binding_convert_central_state_dependent_params(
    struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_status_leds_config *config = dev->config;

    if (config->win_layer < 0) {
        binding->param1 = CAKE_OS_UNKNOWN;
    } else {
        binding->param1 =
            zmk_keymap_layer_active(config->win_layer) ? CAKE_OS_WIN : CAKE_OS_MAC;
    }

    binding->param2 = 0;
#if IS_ENABLED(CONFIG_ZMK_BLE)
    binding->param2 = (zmk_ble_active_profile_index() + 1) |
                      (zmk_ble_active_profile_is_connected() ? BT_CONNECTED : 0) |
                      (zmk_ble_active_profile_is_open() ? BT_OPEN : 0);
#endif

    return 0;
}
#endif

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    struct cake_status status = decode_status(binding);
    cake_per_key_led_show_status(true, &status);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    struct cake_status status = decode_status(binding);
    cake_per_key_led_show_status(false, &status);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_status_leds_driver_api = {
#if HAS_KEYMAP
    .binding_convert_central_state_dependent_params =
        on_keymap_binding_convert_central_state_dependent_params,
#endif
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define BSL_INST(n)                                                                                \
    static const struct behavior_status_leds_config bsl_config_##n = {                             \
        .win_layer = DT_INST_PROP_OR(n, win_layer, -1),                                            \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &bsl_config_##n, POST_KERNEL,                     \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_status_leds_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BSL_INST)
