/*
 * Trackpad on/off toggle that survives sleep.
 *
 * &tp_toggle is a global behavior, so a combo on the central also runs it on
 * the right half, where the trackpad lives. There it flips the enabled state,
 * saves it to flash, and suspends or wakes the TPS43. The state is reloaded on
 * boot, which is also how the keyboard wakes from deep sleep.
 *
 * The TPS43 driver wakes the chip whenever ZMK becomes active, so while the
 * trackpad is off it is put back to sleep after every activity change. The
 * zmk,input-processor-trackpad-gate processor drops anything that gets through
 * in between.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <drivers/behavior.h>
#include <drivers/input_processor.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define HAS_TRACKPAD DT_HAS_COMPAT_STATUS_OKAY(azoteq_tps43)

/* Re-suspend after the driver's own activity handler has woken the chip. */
#define TP_RESUSPEND_DELAY K_MSEC(50)

#ifdef CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE
#define TP_SAVE_DELAY K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE)
#else
#define TP_SAVE_DELAY K_SECONDS(1)
#endif

/* Only meaningful on the half with the trackpad; the left half just ignores the behavior. */
static bool __maybe_unused trackpad_enabled = true;

#if HAS_TRACKPAD

/* Defined in the essenceotd TPS43 driver (tps43.c). */
int tps43_set_sleep(const struct device *dev, bool sleep);

static const struct device *const trackpad = DEVICE_DT_GET_ONE(azoteq_tps43);

static void tp_apply_work_cb(struct k_work *work) {
    if (!device_is_ready(trackpad)) {
        return;
    }
    int ret = tps43_set_sleep(trackpad, !trackpad_enabled);
    if (ret != 0) {
        LOG_WRN("Trackpad %s failed: %d", trackpad_enabled ? "wake" : "suspend", ret);
    }
}

static K_WORK_DELAYABLE_DEFINE(tp_apply_work, tp_apply_work_cb);

#if IS_ENABLED(CONFIG_SETTINGS)
static void tp_save_work_cb(struct k_work *work) {
    uint8_t value = trackpad_enabled;
    int ret = settings_save_one("trackpad/enabled", &value, sizeof(value));
    if (ret != 0) {
        LOG_ERR("Failed to save trackpad state: %d", ret);
    }
}

static K_WORK_DELAYABLE_DEFINE(tp_save_work, tp_save_work_cb);

static int tp_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    uint8_t value;

    if (!settings_name_steq(name, "enabled", &next) || next) {
        return -ENOENT;
    }
    if (len != sizeof(value)) {
        return -EINVAL;
    }
    int ret = read_cb(cb_arg, &value, sizeof(value));
    if (ret < 0) {
        return ret;
    }
    trackpad_enabled = value != 0;
    return 0;
}

static int tp_settings_commit(void) {
    if (!trackpad_enabled) {
        LOG_INF("Trackpad disabled by saved setting");
        k_work_reschedule(&tp_apply_work, K_NO_WAIT);
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(trackpad_toggle, "trackpad", NULL, tp_settings_set,
                               tp_settings_commit, NULL);
#endif /* CONFIG_SETTINGS */

static int tp_activity_listener(const zmk_event_t *eh) {
    if (!trackpad_enabled) {
        k_work_reschedule(&tp_apply_work, TP_RESUSPEND_DELAY);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(trackpad_toggle, tp_activity_listener);
ZMK_SUBSCRIPTION(trackpad_toggle, zmk_activity_state_changed);

#endif /* HAS_TRACKPAD */

/* --- &tp_toggle behavior ---------------------------------------------------- */

#define DT_DRV_COMPAT zmk_behavior_trackpad_toggle

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int tp_toggle_pressed(struct zmk_behavior_binding *binding,
                             struct zmk_behavior_binding_event event) {
#if HAS_TRACKPAD
    trackpad_enabled = !trackpad_enabled;
    LOG_INF("Trackpad %s", trackpad_enabled ? "enabled" : "disabled");
    k_work_reschedule(&tp_apply_work, K_NO_WAIT);
#if IS_ENABLED(CONFIG_SETTINGS)
    k_work_reschedule(&tp_save_work, TP_SAVE_DELAY);
#endif
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_toggle_released(struct zmk_behavior_binding *binding,
                              struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api tp_toggle_driver_api = {
    .binding_pressed = tp_toggle_pressed,
    .binding_released = tp_toggle_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tp_toggle_driver_api);

#endif

/* --- trackpad gate input processor ------------------------------------------ */

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT zmk_input_processor_trackpad_gate

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int tp_gate_handle_event(const struct device *dev, struct input_event *event,
                                uint32_t param1, uint32_t param2,
                                struct zmk_input_processor_state *state) {
    if (trackpad_enabled) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    /* Let releases through so a button held when the trackpad was turned off isn't stuck. */
    if (event->type == INPUT_EV_KEY && event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    return ZMK_INPUT_PROC_STOP;
}

static struct zmk_input_processor_driver_api tp_gate_driver_api = {
    .handle_event = tp_gate_handle_event,
};

#define TP_GATE_INST(n)                                                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tp_gate_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TP_GATE_INST)

#endif
