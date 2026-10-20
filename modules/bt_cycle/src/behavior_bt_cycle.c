/*
 * Next / previous Bluetooth profile, skipping profiles with nothing paired.
 *
 * ZMK only connects to the selected profile, so it can't tell whether the other
 * computers are switched on. It does know which profiles have a host saved, so
 * this cycles through those and leaves the empty slots out. If no other profile
 * is paired it stays where it is.
 */

#define DT_DRV_COMPAT zmk_behavior_bt_cycle

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/ble.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct bt_cycle_config {
    bool reverse;
};

static int bt_cycle_pressed(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct bt_cycle_config *config = dev->config;
    int count = ZMK_BLE_PROFILE_COUNT;
    int current = zmk_ble_active_profile_index();

    for (int step = 1; step < count; step++) {
        int index = (current + (config->reverse ? count - step : step)) % count;
        if (!zmk_ble_profile_is_open(index)) {
            int ret = zmk_ble_prof_select(index);
            if (ret < 0) {
                LOG_WRN("Selecting BT profile %d failed: %d", index, ret);
            }
            return ZMK_BEHAVIOR_OPAQUE;
        }
    }

    LOG_DBG("No other paired BT profile to switch to");
    return ZMK_BEHAVIOR_OPAQUE;
}

static int bt_cycle_released(struct zmk_behavior_binding *binding,
                             struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api bt_cycle_driver_api = {
    .binding_pressed = bt_cycle_pressed,
    .binding_released = bt_cycle_released,
};

#define BT_CYCLE_INST(n)                                                                           \
    static const struct bt_cycle_config bt_cycle_config_##n = {                                    \
        .reverse = DT_INST_PROP(n, reverse),                                                       \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &bt_cycle_config_##n, POST_KERNEL,                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &bt_cycle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BT_CYCLE_INST)
