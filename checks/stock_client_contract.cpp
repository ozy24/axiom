// Stock-client contract (AGENTS.md hard rule 2), checked at compile time on
// every platform.
//
// Stock clients run their own copy of cgame, which reads player stats and the
// bit-packed ammo and powerup counts by these exact indices. If any value here
// changes, a stock client shows the wrong HUD, or worse. These asserts pin
// every value to its stock number.
//
// Generated once from unmodified id code by tools/print_stock_contract.cpp.
// Do not regenerate to make a failure go away: a failure means the change
// breaks stock clients. Add new stats with the stat-slot allocator instead.

#include "g_local.h"

static_assert(AMMO_BULLETS == 0, "ammo_t layout changed");
static_assert(AMMO_SHELLS == 1, "ammo_t layout changed");
static_assert(AMMO_ROCKETS == 2, "ammo_t layout changed");
static_assert(AMMO_GRENADES == 3, "ammo_t layout changed");
static_assert(AMMO_CELLS == 4, "ammo_t layout changed");
static_assert(AMMO_SLUGS == 5, "ammo_t layout changed");
static_assert(AMMO_MAGSLUG == 6, "ammo_t layout changed");
static_assert(AMMO_TRAP == 7, "ammo_t layout changed");
static_assert(AMMO_FLECHETTES == 8, "ammo_t layout changed");
static_assert(AMMO_TESLA == 9, "ammo_t layout changed");
static_assert(AMMO_DISRUPTOR == 10, "ammo_t layout changed");
static_assert(AMMO_PROX == 11, "ammo_t layout changed");
static_assert(AMMO_MAX == 12, "ammo_t layout changed");
static_assert(POWERUP_SCREEN == 0, "powerup_t layout changed");
static_assert(POWERUP_SHIELD == 1, "powerup_t layout changed");
static_assert(POWERUP_AM_BOMB == 2, "powerup_t layout changed");
static_assert(POWERUP_QUAD == 3, "powerup_t layout changed");
static_assert(POWERUP_QUADFIRE == 4, "powerup_t layout changed");
static_assert(POWERUP_INVULNERABILITY == 5, "powerup_t layout changed");
static_assert(POWERUP_INVISIBILITY == 6, "powerup_t layout changed");
static_assert(POWERUP_SILENCER == 7, "powerup_t layout changed");
static_assert(POWERUP_REBREATHER == 8, "powerup_t layout changed");
static_assert(POWERUP_ENVIROSUIT == 9, "powerup_t layout changed");
static_assert(POWERUP_ADRENALINE == 10, "powerup_t layout changed");
static_assert(POWERUP_IR_GOGGLES == 11, "powerup_t layout changed");
static_assert(POWERUP_DOUBLE == 12, "powerup_t layout changed");
static_assert(POWERUP_SPHERE_VENGEANCE == 13, "powerup_t layout changed");
static_assert(POWERUP_SPHERE_HUNTER == 14, "powerup_t layout changed");
static_assert(POWERUP_SPHERE_DEFENDER == 15, "powerup_t layout changed");
static_assert(POWERUP_DOPPELGANGER == 16, "powerup_t layout changed");
static_assert(POWERUP_FLASHLIGHT == 17, "powerup_t layout changed");
static_assert(POWERUP_COMPASS == 18, "powerup_t layout changed");
static_assert(POWERUP_TECH1 == 19, "powerup_t layout changed");
static_assert(POWERUP_TECH2 == 20, "powerup_t layout changed");
static_assert(POWERUP_TECH3 == 21, "powerup_t layout changed");
static_assert(POWERUP_TECH4 == 22, "powerup_t layout changed");
static_assert(POWERUP_MAX == 23, "powerup_t layout changed");
static_assert(STAT_HEALTH_ICON == 0, "player_stat_t layout changed");
static_assert(STAT_HEALTH == 1, "player_stat_t layout changed");
static_assert(STAT_AMMO_ICON == 2, "player_stat_t layout changed");
static_assert(STAT_AMMO == 3, "player_stat_t layout changed");
static_assert(STAT_ARMOR_ICON == 4, "player_stat_t layout changed");
static_assert(STAT_ARMOR == 5, "player_stat_t layout changed");
static_assert(STAT_SELECTED_ICON == 6, "player_stat_t layout changed");
static_assert(STAT_PICKUP_ICON == 7, "player_stat_t layout changed");
static_assert(STAT_PICKUP_STRING == 8, "player_stat_t layout changed");
static_assert(STAT_TIMER_ICON == 9, "player_stat_t layout changed");
static_assert(STAT_TIMER == 10, "player_stat_t layout changed");
static_assert(STAT_HELPICON == 11, "player_stat_t layout changed");
static_assert(STAT_SELECTED_ITEM == 12, "player_stat_t layout changed");
static_assert(STAT_LAYOUTS == 13, "player_stat_t layout changed");
static_assert(STAT_FRAGS == 14, "player_stat_t layout changed");
static_assert(STAT_FLASHES == 15, "player_stat_t layout changed");
static_assert(STAT_CHASE == 16, "player_stat_t layout changed");
static_assert(STAT_SPECTATOR == 17, "player_stat_t layout changed");
static_assert(STAT_CTF_TEAM1_PIC == 18, "player_stat_t layout changed");
static_assert(STAT_CTF_TEAM1_CAPS == 19, "player_stat_t layout changed");
static_assert(STAT_CTF_TEAM2_PIC == 20, "player_stat_t layout changed");
static_assert(STAT_CTF_TEAM2_CAPS == 21, "player_stat_t layout changed");
static_assert(STAT_CTF_FLAG_PIC == 22, "player_stat_t layout changed");
static_assert(STAT_CTF_JOINED_TEAM1_PIC == 23, "player_stat_t layout changed");
static_assert(STAT_CTF_JOINED_TEAM2_PIC == 24, "player_stat_t layout changed");
static_assert(STAT_CTF_TEAM1_HEADER == 25, "player_stat_t layout changed");
static_assert(STAT_CTF_TEAM2_HEADER == 26, "player_stat_t layout changed");
static_assert(STAT_CTF_TECH == 27, "player_stat_t layout changed");
static_assert(STAT_CTF_ID_VIEW == 28, "player_stat_t layout changed");
static_assert(STAT_CTF_MATCH == 29, "player_stat_t layout changed");
static_assert(STAT_CTF_ID_VIEW_COLOR == 30, "player_stat_t layout changed");
static_assert(STAT_CTF_TEAMINFO == 31, "player_stat_t layout changed");
static_assert(STAT_WEAPONS_OWNED_1 == 32, "player_stat_t layout changed");
static_assert(STAT_WEAPONS_OWNED_2 == 33, "player_stat_t layout changed");
static_assert(STAT_AMMO_INFO_START == 34, "player_stat_t layout changed");
static_assert(STAT_AMMO_INFO_END == 40, "player_stat_t layout changed");
static_assert(STAT_POWERUP_INFO_START == 41, "player_stat_t layout changed");
static_assert(STAT_POWERUP_INFO_END == 43, "player_stat_t layout changed");
static_assert(STAT_KEY_A == 44, "player_stat_t layout changed");
static_assert(STAT_KEY_B == 45, "player_stat_t layout changed");
static_assert(STAT_KEY_C == 46, "player_stat_t layout changed");
static_assert(STAT_ACTIVE_WHEEL_WEAPON == 47, "player_stat_t layout changed");
static_assert(STAT_COOP_RESPAWN == 48, "player_stat_t layout changed");
static_assert(STAT_LIVES == 49, "player_stat_t layout changed");
static_assert(STAT_HIT_MARKER == 50, "player_stat_t layout changed");
static_assert(STAT_SELECTED_ITEM_NAME == 51, "player_stat_t layout changed");
static_assert(STAT_HEALTH_BARS == 52, "player_stat_t layout changed");
static_assert(STAT_ACTIVE_WEAPON == 53, "player_stat_t layout changed");
static_assert(STAT_LAST == 54, "player_stat_t layout changed");
static_assert(MAX_STATS == 64, "MAX_STATS changed");
