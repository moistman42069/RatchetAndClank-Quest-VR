/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 *
 * What the mod uses of the game on each disc: lock-strafe's table (OpenRAC/rac1-mods, discs.h),
 * cut to the words this mod reads. The PAL disc's are games/rac1's; the NTSC-U disc's come from
 * OpenRAC's port/game/rac1-ntsc/host/rac1_version.h, which maps the one to the other.
 */
#ifndef QUEST_DISCS_H
#define QUEST_DISCS_H

#include <stdint.h>

typedef struct quest_disc {
    const char* version; /* openrac_api.version */
    uint32_t hero;       /* the hero (Hero): the executable's, used as it is */
    /* The executable's, through relocate_data. */
    uint32_t frame_scale; /* the tick's length, for the standing turn's rates */
    uint32_t turn_step;   /* the turn's step, for its limit */
    /* Level 0's program's, through relocate_level_data(0, ...). */
    uint32_t level_mode; /* the game mode: 0 in play */
    uint32_t camera_yaw; /* the camera's yaw */
} quest_disc;

static const quest_disc k_quest_discs[] = {
    {"pal", 0x0013F450u, 0x0015EE64u, 0x0015EE6Cu, 0x0015F6A8u, 0x00166ED8u},
    {"ntsc", 0x0013F350u, 0x0015ED64u, 0x0015ED6Cu, 0x0015F5C4u, 0x00166DD8u},
};

#endif /* QUEST_DISCS_H */
