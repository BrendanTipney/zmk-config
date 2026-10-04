/*
 * Null Keys visualiser stream.
 *
 * Streams physical key positions and layer state to the host over the
 * zmk-raw-hid module so the Null Keys overlay can show exactly which key is
 * pressed and which layers are active.
 *
 * Streaming only happens while a host is subscribed: the host sends a
 * SUBSCRIBE report every couple of seconds, and the stream stops once
 * CONFIG_NULL_KEYS_VIZ_HOST_TIMEOUT_MS passes without one. This keeps the
 * radio quiet (and the battery happy) when the overlay isn't running.
 *
 * Report layout (little endian, zero padded to CONFIG_RAW_HID_REPORT_SIZE):
 *   [0] 'N'  [1] 'K'  [2] type  [3] sequence
 *   KEY   (0x02): [4] position  [5] pressed      [6..9] layer state  [10..13] uptime ms
 *   LAYER (0x03): [4] layer     [5] active       [6..9] layer state  [10..13] uptime ms
 *   HELLO (0x01): [4] protocol  [5] default layer [6..9] layer state [10..13] uptime ms
 * Host -> keyboard:
 *   [0] 'N'  [1] 'K'  [2] 0x81 SUBSCRIBE
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#include <raw_hid/events.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define NK_MAGIC_0 'N'
#define NK_MAGIC_1 'K'
#define NK_PROTOCOL_VERSION 1
#define NK_PAYLOAD_LEN 16

enum nk_message_type {
    NK_MSG_HELLO = 0x01,
    NK_MSG_KEY = 0x02,
    NK_MSG_LAYER = 0x03,
    NK_MSG_KEYMAP_ID = 0x04,
    NK_REQ_SUBSCRIBE = 0x81,
};

#ifndef NULL_KEYS_KEYMAP_ID
#define NULL_KEYS_KEYMAP_ID "unknown"
#endif
#define NK_KEYMAP_ID_LEN 12

struct nk_report {
    uint8_t data[NK_PAYLOAD_LEN];
};

K_MSGQ_DEFINE(nk_queue, sizeof(struct nk_report), CONFIG_NULL_KEYS_VIZ_QUEUE_SIZE, 1);

static int64_t last_subscribe_ms;
static bool subscribed;
static uint8_t sequence;

static bool host_listening(void) {
    if (subscribed &&
        k_uptime_get() - last_subscribe_ms > CONFIG_NULL_KEYS_VIZ_HOST_TIMEOUT_MS) {
        LOG_DBG("Null Keys host timed out, pausing stream");
        subscribed = false;
    }
    return subscribed;
}

/* Reports are sent from the system work queue so key processing never waits on USB/BLE. */
static void nk_send_work_handler(struct k_work *work) {
    struct nk_report report;
    while (k_msgq_get(&nk_queue, &report, K_NO_WAIT) == 0) {
        raise_raw_hid_sent_event(
            (struct raw_hid_sent_event){.data = report.data, .length = sizeof(report.data)});
    }
}

static K_WORK_DEFINE(nk_send_work, nk_send_work_handler);

static void nk_queue_report(uint8_t type, uint8_t a, uint8_t b) {
    struct nk_report report = {0};
    report.data[0] = NK_MAGIC_0;
    report.data[1] = NK_MAGIC_1;
    report.data[2] = type;
    report.data[3] = sequence++;
    report.data[4] = a;
    report.data[5] = b;
    sys_put_le32(zmk_keymap_layer_state(), &report.data[6]);
    sys_put_le32((uint32_t)k_uptime_get(), &report.data[10]);

    if (k_msgq_put(&nk_queue, &report, K_NO_WAIT) != 0) {
        LOG_WRN("Null Keys report queue full, dropping report");
        return;
    }
    k_work_submit(&nk_send_work);
}

/* Which keymap source this firmware was built from; the overlay compares it with its file. */
static void nk_queue_keymap_id(void) {
    struct nk_report report = {0};
    report.data[0] = NK_MAGIC_0;
    report.data[1] = NK_MAGIC_1;
    report.data[2] = NK_MSG_KEYMAP_ID;
    report.data[3] = sequence++;
    strncpy((char *)&report.data[4], NULL_KEYS_KEYMAP_ID, NK_KEYMAP_ID_LEN);

    if (k_msgq_put(&nk_queue, &report, K_NO_WAIT) != 0) {
        LOG_WRN("Null Keys report queue full, dropping keymap id");
        return;
    }
    k_work_submit(&nk_send_work);
}

static bool is_subscribe_request(const uint8_t *data, uint8_t length) {
    /* Some host stacks leave the (zero) report ID in front of the payload. */
    for (uint8_t offset = 0; offset <= 1 && offset + 3 <= length; offset++) {
        if (data[offset] == NK_MAGIC_0 && data[offset + 1] == NK_MAGIC_1 &&
            data[offset + 2] == NK_REQ_SUBSCRIBE) {
            return true;
        }
    }
    return false;
}

static int nk_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *position = as_zmk_position_state_changed(eh);
    if (position) {
        if (host_listening()) {
            nk_queue_report(NK_MSG_KEY, (uint8_t)position->position, position->state);
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_layer_state_changed *layer = as_zmk_layer_state_changed(eh);
    if (layer) {
        if (host_listening()) {
            nk_queue_report(NK_MSG_LAYER, layer->layer, layer->state);
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct raw_hid_received_event *received = as_raw_hid_received_event(eh);
    if (received && is_subscribe_request(received->data, received->length)) {
        bool was_subscribed = host_listening();
        last_subscribe_ms = k_uptime_get();
        subscribed = true;
        if (!was_subscribed) {
            LOG_INF("Null Keys host subscribed");
        }
        /* Every subscribe gets a HELLO so the host can resync layer state. */
        nk_queue_report(NK_MSG_HELLO, NK_PROTOCOL_VERSION, zmk_keymap_layer_default());
        nk_queue_keymap_id();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(null_keys_viz, nk_listener);
ZMK_SUBSCRIPTION(null_keys_viz, zmk_position_state_changed);
ZMK_SUBSCRIPTION(null_keys_viz, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(null_keys_viz, raw_hid_received_event);
