/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT offsetkey_behavior_mouse_wheel_tick

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_mouse_wheel_tick_config {
    uint16_t acceleration_threshold_ms;
    uint8_t acceleration_max;
};

struct behavior_mouse_wheel_tick_data {
    int64_t last_event_at_ms;
    uint8_t acceleration_level;
};

static int16_t scale_axis(int32_t value, uint8_t multiplier) {
    return (int16_t)CLAMP(value * multiplier, INT16_MIN, INT16_MAX);
}

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_mouse_wheel_tick_config *config = dev->config;
    struct behavior_mouse_wheel_tick_data *data = dev->data;

    uint8_t acceleration_max = MAX(config->acceleration_max, 1);
    int64_t now = k_uptime_get();
    int64_t elapsed = now - data->last_event_at_ms;

    if (config->acceleration_threshold_ms > 0 && data->last_event_at_ms > 0 &&
        elapsed <= config->acceleration_threshold_ms) {
        data->acceleration_level = MIN(data->acceleration_level + 1, acceleration_max);
    } else {
        data->acceleration_level = 1;
    }
    data->last_event_at_ms = now;

    int16_t horizontal = scale_axis((int32_t)binding->param1, data->acceleration_level);
    int16_t vertical = scale_axis((int32_t)binding->param2, data->acceleration_level);

    LOG_DBG("Mouse wheel tick x=%d y=%d acceleration=%d", horizontal, vertical,
            data->acceleration_level);

    zmk_hid_mouse_scroll_set(horizontal, vertical);
    int ret = zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_scroll_set(0, 0);

    return ret;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_mouse_wheel_tick_driver_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

#define MOUSE_WHEEL_TICK_INST(n)                                                                  \
    static struct behavior_mouse_wheel_tick_data behavior_mouse_wheel_tick_data_##n = {};         \
    static const struct behavior_mouse_wheel_tick_config                                           \
        behavior_mouse_wheel_tick_config_##n = {                                                   \
            .acceleration_threshold_ms = DT_INST_PROP(n, acceleration_threshold_ms),                \
            .acceleration_max = DT_INST_PROP(n, acceleration_max),                                  \
        };                                                                                         \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_mouse_wheel_tick_data_##n,                    \
                            &behavior_mouse_wheel_tick_config_##n, POST_KERNEL,                    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_mouse_wheel_tick_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MOUSE_WHEEL_TICK_INST)
