/* keyboard.c - keyboard processing
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

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>

#include "cwc/config.h"
#include "cwc/desktop/idle.h"
#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/session_lock.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/seat.h"
#include "cwc/layout/bsp.h"
#include "cwc/server.h"
#include "cwc/signal.h"

static void on_kbd_modifiers(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd_group =
        wl_container_of(listener, kbd_group, modifiers_l);
    struct wlr_seat *wlr_seat    = kbd_group->seat->wlr_seat;
    struct wlr_keyboard *wlr_kbd = &kbd_group->wlr_kbd_group->keyboard;

    wlr_seat_set_keyboard(wlr_seat, wlr_kbd);
    wlr_seat_keyboard_notify_modifiers(wlr_seat, &wlr_kbd->modifiers);
}

static void on_kbd_key(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd = wl_container_of(listener, kbd, key_l);
    struct wlr_keyboard *wlr_kbd   = &kbd->wlr_kbd_group->keyboard;
    struct wlr_seat *wlr_seat      = kbd->seat->wlr_seat;
    struct wlr_keyboard_key_event *event = data;

    // translate libinput keycode -> xkbcommon
    int keycode = event->keycode + 8;

    // create an empty state to get untransformed keysym so that for keybinds we
    // don't need to account keyname change when the modifier is changed.
    // It will also look more readable e.g. MOD + "1" | MOD + SHIFT + "1"
    // while using transformed is MOD + "1" + MOD | SHIFT + "exclam".
    struct xkb_state *state = xkb_state_new(wlr_kbd->keymap);
    int keysym              = xkb_state_key_get_one_sym(state, keycode);
    xkb_state_unref(state);

    uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_kbd);
    bool handled       = 0;

    switch (event->state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        if (!server.session_lock->locked)
            handled |= keybind_kbd_execute(modifiers, keysym, true);

        wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier,
                                             wlr_seat);
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        // always notify released to client even when has keybind because there
        // is a case when you hold down a key and hitting keybind while still
        // holding it, the client still assume that key is pressed because
        // client doesn't get notified when the key is released
        if (!server.session_lock->locked)
            keybind_kbd_execute(modifiers, keysym, false);
        break;
    }

    if (!handled) {
        wlr_seat_set_keyboard(wlr_seat, wlr_kbd);
        wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec, event->keycode,
                                     event->state);
    }
}

static void _notify_focus_signal(struct wlr_surface *old_surface,
                                 struct wlr_surface *new_surface)
{

    struct cwc_toplevel *old = cwc_toplevel_try_from_wlr_surface(old_surface);
    struct cwc_toplevel *new = cwc_toplevel_try_from_wlr_surface(new_surface);

    if (new) {
        if (new->container->bsp_node)
            bsp_last_focused_update(new->container);

        if (cwc_toplevel_is_unmanaged(new))
            return;
    }

    // only emit signal when mapped
    if (old && cwc_toplevel_is_mapped(old)) {
        if (cwc_toplevel_is_unmanaged(old))
            return;

        cwc_toplevel_set_activated(old, false);
        cwc_object_emit_signal_simple("client::unfocus",
                                      g_config_get_lua_State(), old);
    }

    if (new &&cwc_toplevel_is_mapped(new)) {
        cwc_object_emit_signal_simple("client::focus", g_config_get_lua_State(),
                                      new);
    }
}

static void on_keyboard_focus_change(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd =
        wl_container_of(listener, kbd, seat_keyboard_focus_change_l);
    struct wlr_seat_keyboard_focus_change_event *event = data;
    struct cwc_seat *seat                              = event->seat->data;

    // check for exclusive focus
    if (server.session_lock->locked) {
        keyboard_focus_surface(
            seat, server.session_lock->locker->lock_surface->surface);
        return;
    } else if (seat->exclusive_kbd_interactive) {
        keyboard_focus_surface(
            seat, seat->exclusive_kbd_interactive->wlr_layer_surface->surface);
        return;
    }

    _notify_focus_signal(event->old_surface, event->new_surface);
}

static void kbd_group_apply_config(struct cwc_keyboard_group *kbd_group)
{
    struct wlr_keyboard *wlr_kbd = &kbd_group->wlr_kbd_group->keyboard;
    wlr_keyboard_set_repeat_info(wlr_kbd, g_config.repeat_rate,
                                 g_config.repeat_delay);
}

static void on_config_commit(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd_group =
        wl_container_of(listener, kbd_group, config_commit_l);

    kbd_group_apply_config(kbd_group);
}

struct cwc_keyboard_group *cwc_keyboard_group_create(struct cwc_seat *seat,
                                                     bool virtual)
{
    struct cwc_keyboard_group *kbd_group = calloc(1, sizeof(*kbd_group));
    kbd_group->wlr_kbd_group             = wlr_keyboard_group_create();
    kbd_group->seat                      = seat;
    struct wlr_keyboard *wlr_kbd         = &kbd_group->wlr_kbd_group->keyboard;

    kbd_group->modifiers_l.notify = on_kbd_modifiers;
    kbd_group->key_l.notify       = on_kbd_key;
    wl_signal_add(&kbd_group->wlr_kbd_group->keyboard.events.key,
                  &kbd_group->key_l);
    wl_signal_add(&kbd_group->wlr_kbd_group->keyboard.events.modifiers,
                  &kbd_group->modifiers_l);

    kbd_group->seat_keyboard_focus_change_l.notify = on_keyboard_focus_change;
    wl_signal_add(&seat->wlr_seat->keyboard_state.events.focus_change,
                  &kbd_group->seat_keyboard_focus_change_l);

    if (!virtual) {
        kbd_group->config_commit_l.notify = on_config_commit;
        wl_signal_add(&g_config.events.commit, &kbd_group->config_commit_l);
    }

    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap =
        xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlr_kbd, keymap);
    wlr_seat_set_keyboard(seat->wlr_seat, wlr_kbd);

    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);

    kbd_group_apply_config(kbd_group);

    return kbd_group;
}

void cwc_keyboard_group_destroy(struct cwc_keyboard_group *kbd_group)
{
    wl_list_remove(&kbd_group->modifiers_l.link);
    wl_list_remove(&kbd_group->key_l.link);

    wl_list_remove(&kbd_group->seat_keyboard_focus_change_l.link);

    // destroy_l is a wl_list union no need to worry
    wl_list_remove(&kbd_group->config_commit_l.link);

    wlr_keyboard_group_destroy(kbd_group->wlr_kbd_group);
    free(kbd_group);
}

void cwc_keyboard_group_add_device(struct cwc_keyboard_group *kbd_group,
                                   struct wlr_input_device *device)
{
    struct wlr_keyboard *wlr_kbd = wlr_keyboard_from_input_device(device);

    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap =
        xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(wlr_kbd, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);

    wlr_keyboard_group_add_keyboard(kbd_group->wlr_kbd_group, wlr_kbd);
}

inline void keyboard_focus_surface(struct cwc_seat *seat,
                                   struct wlr_surface *surface)
{
    struct wlr_keyboard *kbd = wlr_seat_get_keyboard(seat->wlr_seat);
    if (kbd && surface)
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, kbd->keycodes,
                                       kbd->num_keycodes, &kbd->modifiers);
    else
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, NULL, 0, NULL);
}

static void on_vkbd_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd_group =
        wl_container_of(listener, kbd_group, destroy_l);

    cwc_log(WLR_DEBUG, "destroying virtual keyboard: %p", kbd_group);

    cwc_keyboard_group_destroy(kbd_group);
}

static void on_new_vkbd(struct wl_listener *listener, void *data)
{
    struct wlr_virtual_keyboard_v1 *vkbd = data;
    struct cwc_seat *seat = vkbd->seat ? vkbd->seat->data : server.seat;
    struct cwc_keyboard_group *kbd_group =
        cwc_keyboard_group_create(seat, true);

    cwc_log(WLR_DEBUG, "new virtual keyboard: %p", kbd_group);

    if (vkbd->has_keymap)
        wlr_keyboard_set_keymap(&kbd_group->wlr_kbd_group->keyboard,
                                vkbd->keyboard.keymap);
    cwc_keyboard_group_add_device(kbd_group, &vkbd->keyboard.base);
    kbd_group->destroy_l.notify = on_vkbd_destroy;
    wl_signal_add(&vkbd->keyboard.base.events.destroy, &kbd_group->destroy_l);
}

void setup_keyboard(struct cwc_server *s)
{
    s->virtual_kbd_manager =
        wlr_virtual_keyboard_manager_v1_create(s->wl_display);
    s->new_vkbd_l.notify = on_new_vkbd;
    wl_signal_add(&s->virtual_kbd_manager->events.new_virtual_keyboard,
                  &s->new_vkbd_l);
}
