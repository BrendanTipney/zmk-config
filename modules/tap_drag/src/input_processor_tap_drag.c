/*
 * Laptop-style tap, tap-and-drag and double tap for the TPS43 trackpad.
 *
 * The TPS43 driver reports INPUT_BTN_TOUCH while a finger is down, and a
 * press/release of INPUT_BTN_0 once the chip recognises a tap (after the
 * finger has lifted). This processor turns those into the gestures a laptop
 * touchpad has:
 *
 *   tap                   -> left click, sent once the drag window passes
 *   tap, touch and move   -> left button held until the finger lifts
 *   tap, tap              -> double click
 *
 * INPUT_BTN_TOUCH is consumed here; ZMK would otherwise treat it as a held
 * left click. Press-and-hold events arrive while the finger is still down, so
 * they can be told apart from taps and are passed through untouched.
 */

#define DT_DRV_COMPAT zmk_input_processor_tap_drag

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>

#include <drivers/input_processor.h>

/* Held-back tap clicks are re-reported on this code so they pass straight through. */
#define TD_DEFERRED_CLICK INPUT_BTN_8

enum td_state {
    TD_IDLE,
    TD_TAP_PENDING,  /* tap seen, click held back for the drag window */
    TD_DRAGGING,     /* second touch landed inside the window, left button down */
    TD_DRAG_ENDED,   /* drag finger lifted; a tap reported now is a double click */
    TD_DOUBLE_CLICK, /* passing a double click's second tap through */
};

struct td_config {
    uint32_t window_ms;
};

struct td_data {
    const struct device *source;
    struct k_work_delayable click_work;
    struct k_spinlock lock;
    enum td_state state;
    bool touching;
    bool swallow_release;
};

static void td_click_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct td_data *data = CONTAINER_OF(dwork, struct td_data, click_work);
    bool send = false;

    k_spinlock_key_t key = k_spin_lock(&data->lock);
    if (data->state == TD_TAP_PENDING) {
        data->state = TD_IDLE;
        send = true;
    }
    k_spin_unlock(&data->lock, key);

    if (send) {
        input_report_key(data->source, TD_DEFERRED_CLICK, 1, true, K_FOREVER);
        input_report_key(data->source, TD_DEFERRED_CLICK, 0, true, K_FOREVER);
    }
}

static int td_handle_touch(struct td_data *data, struct input_event *event) {
    if (data->touching) {
        if (data->state == TD_TAP_PENDING) {
            /* Second touch inside the window: the held-back click becomes a drag. */
            k_work_cancel_delayable(&data->click_work);
            data->state = TD_DRAGGING;
            event->code = INPUT_BTN_0;
            event->value = 1;
            return ZMK_INPUT_PROC_CONTINUE;
        }
        if (data->state != TD_DRAGGING) {
            data->state = TD_IDLE;
        }
        return ZMK_INPUT_PROC_STOP;
    }

    if (data->state == TD_DRAGGING) {
        data->state = TD_DRAG_ENDED;
        event->code = INPUT_BTN_0;
        event->value = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }
    return ZMK_INPUT_PROC_STOP;
}

static int td_handle_left(const struct td_config *config, struct td_data *data,
                          struct input_event *event) {
    if (data->state == TD_DRAGGING) {
        /* The button is already held for the drag; drop the chip's own press-and-hold. */
        return ZMK_INPUT_PROC_STOP;
    }
    if (data->touching) {
        /* Taps are only reported after lift, so this is press-and-hold: leave it alone. */
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->value > 0) {
        if (data->state == TD_DRAG_ENDED) {
            data->state = TD_DOUBLE_CLICK;
            return ZMK_INPUT_PROC_CONTINUE;
        }
        data->source = event->dev;
        data->state = TD_TAP_PENDING;
        data->swallow_release = true;
        k_work_reschedule(&data->click_work, K_MSEC(config->window_ms));
        return ZMK_INPUT_PROC_STOP;
    }

    if (data->swallow_release) {
        data->swallow_release = false;
        return ZMK_INPUT_PROC_STOP;
    }
    if (data->state == TD_DOUBLE_CLICK) {
        data->state = TD_IDLE;
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static int td_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                           uint32_t param2, struct zmk_input_processor_state *state) {
    const struct td_config *config = dev->config;
    struct td_data *data = dev->data;
    int ret = ZMK_INPUT_PROC_CONTINUE;

    if (event->type != INPUT_EV_KEY) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code == TD_DEFERRED_CLICK) {
        event->code = INPUT_BTN_0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    k_spinlock_key_t key = k_spin_lock(&data->lock);
    if (event->code == INPUT_BTN_TOUCH) {
        data->touching = event->value > 0;
        ret = td_handle_touch(data, event);
    } else if (event->code == INPUT_BTN_0) {
        ret = td_handle_left(config, data, event);
    }
    k_spin_unlock(&data->lock, key);

    return ret;
}

static int td_init(const struct device *dev) {
    struct td_data *data = dev->data;

    k_work_init_delayable(&data->click_work, td_click_work_cb);
    return 0;
}

static struct zmk_input_processor_driver_api td_driver_api = {
    .handle_event = td_handle_event,
};

#define TD_INST(n)                                                                                 \
    static struct td_data td_data_##n = {};                                                        \
    static const struct td_config td_config_##n = {                                                \
        .window_ms = DT_INST_PROP(n, window_ms),                                                   \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, td_init, NULL, &td_data_##n, &td_config_##n, POST_KERNEL,             \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &td_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TD_INST)
