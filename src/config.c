/* config.c - configuration management
 *
 * Copyright (C) 2024 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cairo.h>
#include <libinput.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cwc/config.h"
#include "cwc/util.h"

void cwc_config_init()
{
    cwc_config_set_default();
    g_config.CWC_PRIVATE.old_config = malloc(sizeof(struct cwc_config));
    memcpy(g_config.CWC_PRIVATE.old_config, &g_config, sizeof(g_config));
    wl_signal_init(&g_config.events.commit);
}

void cwc_config_commit()
{
    cwc_log(CWC_INFO, "config committed");
    wl_signal_emit(&g_config.events.commit, g_config.CWC_PRIVATE.old_config);
    memcpy(g_config.CWC_PRIVATE.old_config, &g_config, sizeof(g_config));
}

void cwc_config_set_default()
{
    g_config.border_color_rotation_degree = 0;
    g_config.border_color_focus           = NULL;
    g_config.border_color_normal =
        cairo_pattern_create_rgba(127 / 255.0, 127 / 255.0, 127 / 255.0, 1);
    g_config.useless_gaps = 0;
    g_config.border_width = 1;

    g_config.warp_cursor_to_edge_on_resize = false;
    g_config.move_cursor_on_focus          = false;

    g_config.cursor_size      = 24;
    g_config.sensitivity      = 0.0;
    g_config.scroll_method    = LIBINPUT_CONFIG_SCROLL_2FG;
    g_config.click_method     = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
    g_config.send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
    g_config.accel_profile    = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
    g_config.tap_button_map   = LIBINPUT_CONFIG_TAP_MAP_LRM;

    g_config.tap_to_click            = true;
    g_config.tap_and_drag            = true;
    g_config.drag_lock               = true;
    g_config.natural_scrolling       = false;
    g_config.disable_while_typing    = true;
    g_config.left_handed             = false;
    g_config.middle_button_emulation = false;

    g_config.repeat_rate  = 30;
    g_config.repeat_delay = 400;
}

void cwc_config_set_cairo_pattern(cairo_pattern_t **dst, cairo_pattern_t *src)
{
    if (g_config.border_color_focus)
        cairo_pattern_destroy(*dst);

    *dst = cairo_pattern_reference(src);
}

void cwc_config_set_number_positive(int *dest, int src)
{
    *dest = MAX(0, src);
}
