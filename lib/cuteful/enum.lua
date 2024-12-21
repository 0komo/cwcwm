---------------------------------------------------------------------------
--- Constants extracted from C code.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2024
-- @license GPLv3
-- @module cuteful.enum
---------------------------------------------------------------------------

local bit = require("bit")

local enum = {

    --- Keyboard modifier constant mapped from `wlr_keyboard.h`
    --
    -- @table modifier
    modifier = {
        NONE  = 0,
        SHIFT = bit.lshift(1, 0),
        CAPS  = bit.lshift(1, 1),
        CTRL  = bit.lshift(1, 2),
        ALT   = bit.lshift(1, 3),
        MOD2  = bit.lshift(1, 4),
        MOD3  = bit.lshift(1, 5),
        LOGO  = bit.lshift(1, 6), -- Aka super/mod4/window key
        MOD5  = bit.lshift(1, 7),
    },

    --- Yoink'd from Linux `input-event-codes.h`
    --
    -- @table mouse_btn
    mouse_btn = {
        LEFT    = 0x110,
        RIGHT   = 0x111,
        MIDDLE  = 0x112,
        FORWARD = 0x115, -- Aka mouse5
        BACK    = 0x116, -- Aka mouse4
    },

    --- Extracted from wlr_direction `wlr_output_layout.h`
    --
    -- @table direction
    direction = {
        UP    = bit.lshift(1, 0),
        DOWN  = bit.lshift(1, 1),
        LEFT  = bit.lshift(1, 2),
        RIGHT = bit.lshift(1, 3),
    },

    --- Pointer constant used for configuring pointer device from `libinput.h`
    -- Ref: <https://wayland.freedesktop.org/libinput/doc/latest/api/group__config.html>
    --
    --@table pointer
    pointer = {
        SCROLL_NO_SCROLL                       = 0,
        SCROLL_2FG                             = bit.lshift(1, 0),
        SCROLL_EDGE                            = bit.lshift(1, 1),
        SCROLL_ON_BUTTON_DOWN                  = bit.lshift(1, 2),

        CLICK_METHOD_NONE                      = 0,
        CLICK_METHOD_BUTTON_AREAS              = bit.lshift(1, 0),
        CLICK_METHOD_CLICKFINGER               = bit.lshift(1, 1),

        SEND_EVENTS_ENABLED                    = 0,
        SEND_EVENTS_DISABLED                   = bit.lshift(1, 0),
        SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE = bit.lshift(1, 1),

        ACCEL_PROFILE_FLAT                     = bit.lshift(1, 0),
        ACCEL_PROFILE_ADAPTIVE                 = bit.lshift(1, 1),
        ACCEL_PROFILE_CUSTOM                   = bit.lshift(1, 2),

        TAP_MAP_LRM                            = 0,
        TAP_MAP_LMR                            = 1,
    },

    --- layout_mode enum extracted from cwc `types.h`.
    --
    -- @table layout_mode
    layout_mode = {
        FLOATING = 0,
        MASTER   = 1,
        BSP      = 2,
        LENGTH   = 3,
    },

}

return enum
