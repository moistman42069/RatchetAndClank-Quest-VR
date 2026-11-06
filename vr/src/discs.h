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
    uint32_t follow_cam; /* g_current_follow_cam: the camera record in use */
    uint32_t item_defs;  /* the item table: 0x4C bytes each, the joint they hang from at +0x0C */
    uint32_t joint_lists; /* nine words: the lists of the hero's class those joints are */
    /* The executable's, used as they are: by item id, the ammo (s32 each, weapon_ammo_counts) and
     * whether it is owned (a byte each, item_available); Rerac-vr's NTSC 0x13d428 / 0x13d4c0. */
    uint32_t ammo;
    uint32_t owned;
    /* Level 12's program's: the planet buster's camera is on (Rerac-vr's NTSC CARRIER_CAM). */
    uint32_t hoven_death_cam;
    /* Level 0's program's: the last line test's result, and the test itself by its name. */
    uint32_t coll_result;
    const char* coll_line;
    /* The quick select: level 0's table of its 8 slots, the save's 8 items, and the pad. */
    uint32_t quick_slots;
    uint32_t quick_items;
    uint32_t pad_state;
    /* The executable's: the three fonts' glyph tables (normal, small, large), the effect textures'
     * table (theirs are 1, 2 and 3) and the word holding where the level's texture payload is. */
    uint32_t fonts[3];
    uint32_t effect_defs;
    uint32_t texture_payload;
    /* Level 0's program's: its normal font's glyph table, the small and the large next (0x3A0 apart),
     * which text in a level prints with (Rerac-vr's rc-formats font.rs: NTSC level 00 0x1c3150). */
    uint32_t level_fonts;
    /* The executable's menu system (current page at +0x04), through relocate_data. */
    uint32_t menu_system;
    /* A menu list's update and draw (update_menu_entry_actions, render_localized_ui_entry_list),
     * by name: each level places them at its own address. */
    const char* list_update;
    const char* list_draw;
    /* The executable's: the bolts he has (gBolts). */
    uint32_t bolts;
    /* A sound of a moby's class played on it, by name (Moby_PlaySound). */
    const char* play_sound;
    /* The wrench's sparks at the last line test's point, by name (spawn_wrench_impact_effects). */
    const char* wrench_sparks;
    /* A moby's animation one tick on, by name (MobyAnimAdvance), as the hero's update does a hand
     * item's. */
    const char* anim_advance;
    /* The pause menu's other widgets, by name: the item grid's update, the lists of toggles and of
     * choices, the Sound page. */
    const char* menu_grid;
    const char* menu_toggles;
    const char* menu_choices;
    const char* menu_sound;
    /* Their other draws, by name (Rerac-vr's pause/pointer.rs): the ship's planet list (draw_menu_text_list
     * at the planet select), the galaxy map, the quit box, the memory card's slots and an icon list. */
    const char* menu_planet_list;
    const char* menu_galaxy;
    const char* menu_quit;
    const char* menu_slots;
    const char* menu_icons;
    /* The galaxy map's planets (draw_level_selection_map): through relocate_data, 16 bytes a level (x, y
     * in the map's box, the label's offset); the executable's bytes a level, visited and unlocked. */
    uint32_t planet_markers;
    uint32_t planets_visited;
    uint32_t planets_unlocked;
    /* The ship's destination and the planet list's level of each row, through relocate_data
     * (update_menu_entry_actions, list flag 0x20). */
    uint32_t planet_dest;
    uint32_t planet_order;
    /* The Sound page's, the executable's: the effects' and the music's volume (0..1024), and the mixer
     * they are given to (group volumes at +0x48). */
    uint32_t volumes[2];
    uint32_t mixer;
    /* A help message's text by its id, by name (get_help_message_text). */
    const char* message_text;
    /* The memory card's question, a menu widget (draw_prompt_box). */
    uint32_t prompt_widget;
    /* The message boxes' labels: this disc's ids above the US disc's (0x524F the US 0x524A's, freeze.c). */
    int32_t label_shift;
    /* The game's PAL video mode (it spaces the save slots) and the slot its Save and Load pages take. */
    uint32_t pal_mode;
    uint32_t memcard_slot;
    /* The Help icon list's step, in 16ths of a pixel (draw_menu_icon_list). */
    int32_t icon_step;
    /* The game's mode (5 the vendor), and level 0's address of the vendor's state (update_vendor_menu). */
    uint32_t game_mode;
    uint32_t vendor_menu;
    /* Shopping on foot: the level's draw, by name. */
    const char* draw_world;
    const char* hero_show;         /* HeroClearObjectFlags */
    const char* moby_matrix;       /* moby_build_matrix */
    const char* camera_script;     /* CameraScript */
    const char* camera_script_end; /* CameraScript2 */
    const char* camera_position;   /* set_camera_position */
    const char* camera_rotation;   /* set_camera_rotation */
    const char* sounds_on;         /* snd_ContinueAllSoundsInGroup */
    const char* music_on;          /* music_Unpause */
    /* The screens' margins (capture_bounds_adjustments, four floats a screen), and how the game's camera puts a
     * point on the canvas (project_graphics_bounds_float: the matrix under camera_position, view_context,
     * screen_extent). */
    uint32_t screen_margins;
    uint32_t camera_matrix;
    uint32_t view_context;
    uint32_t screen_extent;
    /* HeroEnterMoveState differs by level and has no one name: where level 0's close_vendor_menu (at this address,
     * this long) calls it, for function_called. */
    uint32_t vendor_close;
    uint32_t vendor_close_size;
    uint32_t hero_set_state;
    /* Level 0's program's: the quick select ring's width and height (the draw's, Hud_QuickSelectDraw). */
    uint32_t ring_size;
    /* A HUD icon's frame by its icon and variant, by name (get_icon_frame). */
    const char* icon_frame;
    /* The weapon's ammo tile, the hand slot's item's (update_weapon_hud_state), by name, and level 0's two words it
     * keeps of it (the item and its queued tile), for a second tile. */
    const char* weapon_hud;
    uint32_t weapon_hud_words;
    /* A HUD box's value by its tile, by name (Hud_SetBarValueById): 0 lets a box go off the screen. */
    const char* hud_bar;
    /* The glints (a bolt's twinkle; append_billboard_batch, Glints_Draw draws them on the screen): their sixteen
     * records, the render list's cursor the draw writes them at, and by name the effect textures (GetEffectTex) and
     * the world quad's draw (fast_draw_quad_real) that put them in the world instead. */
    uint32_t glints;
    uint32_t render_cursor;
    const char* effect_texture;
    const char* world_quad;
    /* The Cheats page (pause, VR Options, Cheats): the game's cheat bytes (gCheats, one a cheat: 0 the others' big
     * heads, 1 Ratchet's, 2 the trippy contrails, 3 Clank's, 7 the enemies'), level 0's item table (0x18 bytes an item
     * id, its most rounds a u16 at +0x0E; ammo-max's item_table), and by name the game's GiveItem and UnlockPlanet
     * (rac1.item.give, rac1.planet.unlock). */
    uint32_t cheats;
    uint32_t item_table;
    const char* give_item;
    const char* unlock_planet;
    uint32_t moby_grid;  /* the moby grid's cells, through relocate_data (hand/game_moby_grid.c's exe_grid) */
    /* The HUD's channel records (hud_chan_slots, thirteen of 0x90 bytes; queue_animation_update): the ammo box's is
     * the first, +0x04 the shown box's value (0 slides it off), +0x0C its ammo word, +0x64 the newest handle, +0x68 a
     * next box waiting. */
    uint32_t hud_channels;
    /* The game's black bars (draw_letterbox_bars): the word that wants them (letterbox_enabled; the NTSC decomp's
     * occl_chain_active), and after it their height in lines, 0 to 24 (letterbox_bar_height). */
    uint32_t letterbox;
    /* The pause menu's map screen, its update by name (DrawMapScreen): Cross its missions page, Circle the shown
     * planet's infobot, L1 and R1 the planet before and after. */
    const char* menu_map;
    /* A voice slot released (release_voice_slot, sound_KillChannel): the wrench's whirl, while it rests. */
    const char* voice_release;
} quest_disc;

/* turn_step is the game's g_frame_time (0.02 s a tick on PAL, 1/60 on NTSC-U), frame_scale its
 * g_frame_scale_sq (mobyutil.c's set_video_timing). */
static const quest_disc k_quest_discs[] = {
    {"pal", 0x0013F450u, 0x0015EE64u, 0x0015EE6Cu, 0x0015F6A8u, 0x00166ED8u, 0x00166F00u, 0x00179BC0u, 0x001E7F90u, 0x0013D530u, 0x0013D5C8u, 0x00161A20u, 0x00173F40u, "func_L00_001EFFF0", 0x0017DBC0u, 0x00141FA0u, 0x0013CA40u,
     {0x001DF3D0u, 0x001DF770u, 0x001DFB10u}, 0x0018D540u, 0x0015F560u, 0x001C34D0u, 0x001D5F70u, "func_0021BB90", "func_0021C1B0", 0x0015EE98u, "func_0022ED80", "func_L00_002A92C8", "func_L00_002514B8", "func_0021C840", "func_00221E60", "func_002222F8", "func_0021DB30",
     "func_0021E4B0", "func_00222978", "func_0021F898", "func_00224C30", "func_0021AEF8", 0x001DE1A8u, 0x0013DE60u, 0x0013DE48u,
     0x001A0414u, 0x001602E0u, {0x0015EEF0u, 0x0015EEECu}, 0x0013E650u, "func_001FE540", 0x001D29D0u, 5, 0x0015EE80u, 0x0015EF34u, 0x252, 0x0015F6E8u, 0x001CA7C0u,
     "func_001F3D78", "func_L00_00233950", "func_L00_00251E30", "func_L00_002EBF50", "func_L00_002EC0C8",
     "func_L00_002EBE88", "func_L00_002EBEE0", "func_0012E558", "func_00216F28",
     0x001E6518u, 0x00187140u, 0x0018CE00u, 0x0013E600u, 0x0029BED8u, 404u, 0x00222B80u, 0x0015F7F8u, "func_00200198",
     "func_L00_0023B750", 0x0015FA44u, "func_001FFDA0", 0x0018EE00u, 0x00161000u, "func_001F4868", "func_001F7EF8",
     0x0015EEB0u, 0x001C43B0u, "func_L00_002618D8", "func_L00_00261848", 0x001B7A60u, 0x00199C60u, 0x0015F544u, "func_0021CE60", "func_0022EAB0"},
    {"ntsc", 0x0013F350u, 0x0015ED64u, 0x0015ED6Cu, 0x0015F5C4u, 0x00166DD8u, 0x00166E00u, 0x00179AC0u, 0x001E7C10u, 0x0013D428u, 0x0013D4C0u, 0x001619A0u, 0x00173E40u, "func_L00_001EFC70", 0x0017DAC0u, 0x00141EA0u, 0x0013C940u,
     {0x001DF050u, 0x001DF3F0u, 0x001DF790u}, 0x0018D440u, 0x0015F460u, 0x001C3150u, 0x001D5BF0u, "FUN_0021abf8", "FUN_0021b1c8", 0x0015ED98u, "FUN_0022da68", "FUN_L00_002a8028", "FUN_0020d580", "FUN_0021b858", "FUN_00220e28", "FUN_002212b8", "FUN_0021cb30",
     "FUN_0021d4a8", "FUN_00221930", "FUN_0021e890", "FUN_002239e0", "FUN_00219fa0", 0x001DDE28u, 0x0013DD58u, 0x0013DD40u,
     0x001A0314u, 0x001601E0u, {0x0015EDF0u, 0x0015EDECu}, 0x0013E550u, "FUN_001fdd10", 0x001D2650u, 0, 0x0015ED80u, 0x0015EE34u, 0x260, 0x0015F604u, 0x001CA4C0u,
     "FUN_001f39d0", "FUN_L00_002330d0", "FUN_L00_00250df8", "FUN_L00_002eaaa0", "FUN_L00_002eac18",
     "FUN_L00_002ea9d8", "FUN_L00_002eaa30", "FUN_0012e418", "music_unpause",
     0x001E6218u, 0x00187040u, 0x0018CD00u, 0x0013E500u, 0x0029AB70u, 404u, 0x002223F8u, 0x0015F718u, "FUN_001ff960",
     "FUN_L00_0023adb8", 0x0015F96Cu, "FUN_001ff570", 0x0018ED00u, 0x00160F00u, "FUN_001f44b8", "FUN_001f7d30",
     0x0015EDB0u, 0x001C40B0u, "FUN_L00_00260860", "FUN_L00_002607d0", 0x001B76E0u, 0x00199B60u, 0x0015F444u, "FUN_0021be60", "FUN_0022d798"},
};

#endif /* QUEST_DISCS_H */
