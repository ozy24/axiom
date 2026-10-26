// Generator for checks/stock_client_contract.cpp: prints each stock enum value
// as a static_assert. Run only against unmodified id code, from the repo root:
//
//   g++ -std=c++17 -w -DKEX_Q2_GAME -DKEX_Q2GAME_DYNAMIC \
//       -include platform/posix/compat.h -Irerelease \
//       -Ithird_party/fmt-10.2.1/include -Ithird_party/jsoncpp-1.9.6/include \
//       tools/print_stock_contract.cpp -o /tmp/print_contract && /tmp/print_contract
//
// The enumerator lists below were extracted from rerelease/bg_local.h.
#include "g_local.h"
#include <cstdio>
int main() {
    std::printf("static_assert(AMMO_BULLETS == %d, \"ammo_t layout changed\");\n", (int) AMMO_BULLETS);
    std::printf("static_assert(AMMO_SHELLS == %d, \"ammo_t layout changed\");\n", (int) AMMO_SHELLS);
    std::printf("static_assert(AMMO_ROCKETS == %d, \"ammo_t layout changed\");\n", (int) AMMO_ROCKETS);
    std::printf("static_assert(AMMO_GRENADES == %d, \"ammo_t layout changed\");\n", (int) AMMO_GRENADES);
    std::printf("static_assert(AMMO_CELLS == %d, \"ammo_t layout changed\");\n", (int) AMMO_CELLS);
    std::printf("static_assert(AMMO_SLUGS == %d, \"ammo_t layout changed\");\n", (int) AMMO_SLUGS);
    std::printf("static_assert(AMMO_MAGSLUG == %d, \"ammo_t layout changed\");\n", (int) AMMO_MAGSLUG);
    std::printf("static_assert(AMMO_TRAP == %d, \"ammo_t layout changed\");\n", (int) AMMO_TRAP);
    std::printf("static_assert(AMMO_FLECHETTES == %d, \"ammo_t layout changed\");\n", (int) AMMO_FLECHETTES);
    std::printf("static_assert(AMMO_TESLA == %d, \"ammo_t layout changed\");\n", (int) AMMO_TESLA);
    std::printf("static_assert(AMMO_DISRUPTOR == %d, \"ammo_t layout changed\");\n", (int) AMMO_DISRUPTOR);
    std::printf("static_assert(AMMO_PROX == %d, \"ammo_t layout changed\");\n", (int) AMMO_PROX);
    std::printf("static_assert(AMMO_MAX == %d, \"ammo_t layout changed\");\n", (int) AMMO_MAX);
    std::printf("static_assert(POWERUP_SCREEN == %d, \"powerup_t layout changed\");\n", (int) POWERUP_SCREEN);
    std::printf("static_assert(POWERUP_SHIELD == %d, \"powerup_t layout changed\");\n", (int) POWERUP_SHIELD);
    std::printf("static_assert(POWERUP_AM_BOMB == %d, \"powerup_t layout changed\");\n", (int) POWERUP_AM_BOMB);
    std::printf("static_assert(POWERUP_QUAD == %d, \"powerup_t layout changed\");\n", (int) POWERUP_QUAD);
    std::printf("static_assert(POWERUP_QUADFIRE == %d, \"powerup_t layout changed\");\n", (int) POWERUP_QUADFIRE);
    std::printf("static_assert(POWERUP_INVULNERABILITY == %d, \"powerup_t layout changed\");\n", (int) POWERUP_INVULNERABILITY);
    std::printf("static_assert(POWERUP_INVISIBILITY == %d, \"powerup_t layout changed\");\n", (int) POWERUP_INVISIBILITY);
    std::printf("static_assert(POWERUP_SILENCER == %d, \"powerup_t layout changed\");\n", (int) POWERUP_SILENCER);
    std::printf("static_assert(POWERUP_REBREATHER == %d, \"powerup_t layout changed\");\n", (int) POWERUP_REBREATHER);
    std::printf("static_assert(POWERUP_ENVIROSUIT == %d, \"powerup_t layout changed\");\n", (int) POWERUP_ENVIROSUIT);
    std::printf("static_assert(POWERUP_ADRENALINE == %d, \"powerup_t layout changed\");\n", (int) POWERUP_ADRENALINE);
    std::printf("static_assert(POWERUP_IR_GOGGLES == %d, \"powerup_t layout changed\");\n", (int) POWERUP_IR_GOGGLES);
    std::printf("static_assert(POWERUP_DOUBLE == %d, \"powerup_t layout changed\");\n", (int) POWERUP_DOUBLE);
    std::printf("static_assert(POWERUP_SPHERE_VENGEANCE == %d, \"powerup_t layout changed\");\n", (int) POWERUP_SPHERE_VENGEANCE);
    std::printf("static_assert(POWERUP_SPHERE_HUNTER == %d, \"powerup_t layout changed\");\n", (int) POWERUP_SPHERE_HUNTER);
    std::printf("static_assert(POWERUP_SPHERE_DEFENDER == %d, \"powerup_t layout changed\");\n", (int) POWERUP_SPHERE_DEFENDER);
    std::printf("static_assert(POWERUP_DOPPELGANGER == %d, \"powerup_t layout changed\");\n", (int) POWERUP_DOPPELGANGER);
    std::printf("static_assert(POWERUP_FLASHLIGHT == %d, \"powerup_t layout changed\");\n", (int) POWERUP_FLASHLIGHT);
    std::printf("static_assert(POWERUP_COMPASS == %d, \"powerup_t layout changed\");\n", (int) POWERUP_COMPASS);
    std::printf("static_assert(POWERUP_TECH1 == %d, \"powerup_t layout changed\");\n", (int) POWERUP_TECH1);
    std::printf("static_assert(POWERUP_TECH2 == %d, \"powerup_t layout changed\");\n", (int) POWERUP_TECH2);
    std::printf("static_assert(POWERUP_TECH3 == %d, \"powerup_t layout changed\");\n", (int) POWERUP_TECH3);
    std::printf("static_assert(POWERUP_TECH4 == %d, \"powerup_t layout changed\");\n", (int) POWERUP_TECH4);
    std::printf("static_assert(POWERUP_MAX == %d, \"powerup_t layout changed\");\n", (int) POWERUP_MAX);
    std::printf("static_assert(STAT_HEALTH_ICON == %d, \"player_stat_t layout changed\");\n", (int) STAT_HEALTH_ICON);
    std::printf("static_assert(STAT_HEALTH == %d, \"player_stat_t layout changed\");\n", (int) STAT_HEALTH);
    std::printf("static_assert(STAT_AMMO_ICON == %d, \"player_stat_t layout changed\");\n", (int) STAT_AMMO_ICON);
    std::printf("static_assert(STAT_AMMO == %d, \"player_stat_t layout changed\");\n", (int) STAT_AMMO);
    std::printf("static_assert(STAT_ARMOR_ICON == %d, \"player_stat_t layout changed\");\n", (int) STAT_ARMOR_ICON);
    std::printf("static_assert(STAT_ARMOR == %d, \"player_stat_t layout changed\");\n", (int) STAT_ARMOR);
    std::printf("static_assert(STAT_SELECTED_ICON == %d, \"player_stat_t layout changed\");\n", (int) STAT_SELECTED_ICON);
    std::printf("static_assert(STAT_PICKUP_ICON == %d, \"player_stat_t layout changed\");\n", (int) STAT_PICKUP_ICON);
    std::printf("static_assert(STAT_PICKUP_STRING == %d, \"player_stat_t layout changed\");\n", (int) STAT_PICKUP_STRING);
    std::printf("static_assert(STAT_TIMER_ICON == %d, \"player_stat_t layout changed\");\n", (int) STAT_TIMER_ICON);
    std::printf("static_assert(STAT_TIMER == %d, \"player_stat_t layout changed\");\n", (int) STAT_TIMER);
    std::printf("static_assert(STAT_HELPICON == %d, \"player_stat_t layout changed\");\n", (int) STAT_HELPICON);
    std::printf("static_assert(STAT_SELECTED_ITEM == %d, \"player_stat_t layout changed\");\n", (int) STAT_SELECTED_ITEM);
    std::printf("static_assert(STAT_LAYOUTS == %d, \"player_stat_t layout changed\");\n", (int) STAT_LAYOUTS);
    std::printf("static_assert(STAT_FRAGS == %d, \"player_stat_t layout changed\");\n", (int) STAT_FRAGS);
    std::printf("static_assert(STAT_FLASHES == %d, \"player_stat_t layout changed\");\n", (int) STAT_FLASHES);
    std::printf("static_assert(STAT_CHASE == %d, \"player_stat_t layout changed\");\n", (int) STAT_CHASE);
    std::printf("static_assert(STAT_SPECTATOR == %d, \"player_stat_t layout changed\");\n", (int) STAT_SPECTATOR);
    std::printf("static_assert(STAT_CTF_TEAM1_PIC == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TEAM1_PIC);
    std::printf("static_assert(STAT_CTF_TEAM1_CAPS == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TEAM1_CAPS);
    std::printf("static_assert(STAT_CTF_TEAM2_PIC == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TEAM2_PIC);
    std::printf("static_assert(STAT_CTF_TEAM2_CAPS == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TEAM2_CAPS);
    std::printf("static_assert(STAT_CTF_FLAG_PIC == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_FLAG_PIC);
    std::printf("static_assert(STAT_CTF_JOINED_TEAM1_PIC == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_JOINED_TEAM1_PIC);
    std::printf("static_assert(STAT_CTF_JOINED_TEAM2_PIC == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_JOINED_TEAM2_PIC);
    std::printf("static_assert(STAT_CTF_TEAM1_HEADER == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TEAM1_HEADER);
    std::printf("static_assert(STAT_CTF_TEAM2_HEADER == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TEAM2_HEADER);
    std::printf("static_assert(STAT_CTF_TECH == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TECH);
    std::printf("static_assert(STAT_CTF_ID_VIEW == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_ID_VIEW);
    std::printf("static_assert(STAT_CTF_MATCH == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_MATCH);
    std::printf("static_assert(STAT_CTF_ID_VIEW_COLOR == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_ID_VIEW_COLOR);
    std::printf("static_assert(STAT_CTF_TEAMINFO == %d, \"player_stat_t layout changed\");\n", (int) STAT_CTF_TEAMINFO);
    std::printf("static_assert(STAT_WEAPONS_OWNED_1 == %d, \"player_stat_t layout changed\");\n", (int) STAT_WEAPONS_OWNED_1);
    std::printf("static_assert(STAT_WEAPONS_OWNED_2 == %d, \"player_stat_t layout changed\");\n", (int) STAT_WEAPONS_OWNED_2);
    std::printf("static_assert(STAT_AMMO_INFO_START == %d, \"player_stat_t layout changed\");\n", (int) STAT_AMMO_INFO_START);
    std::printf("static_assert(STAT_AMMO_INFO_END == %d, \"player_stat_t layout changed\");\n", (int) STAT_AMMO_INFO_END);
    std::printf("static_assert(STAT_POWERUP_INFO_START == %d, \"player_stat_t layout changed\");\n", (int) STAT_POWERUP_INFO_START);
    std::printf("static_assert(STAT_POWERUP_INFO_END == %d, \"player_stat_t layout changed\");\n", (int) STAT_POWERUP_INFO_END);
    std::printf("static_assert(STAT_KEY_A == %d, \"player_stat_t layout changed\");\n", (int) STAT_KEY_A);
    std::printf("static_assert(STAT_KEY_B == %d, \"player_stat_t layout changed\");\n", (int) STAT_KEY_B);
    std::printf("static_assert(STAT_KEY_C == %d, \"player_stat_t layout changed\");\n", (int) STAT_KEY_C);
    std::printf("static_assert(STAT_ACTIVE_WHEEL_WEAPON == %d, \"player_stat_t layout changed\");\n", (int) STAT_ACTIVE_WHEEL_WEAPON);
    std::printf("static_assert(STAT_COOP_RESPAWN == %d, \"player_stat_t layout changed\");\n", (int) STAT_COOP_RESPAWN);
    std::printf("static_assert(STAT_LIVES == %d, \"player_stat_t layout changed\");\n", (int) STAT_LIVES);
    std::printf("static_assert(STAT_HIT_MARKER == %d, \"player_stat_t layout changed\");\n", (int) STAT_HIT_MARKER);
    std::printf("static_assert(STAT_SELECTED_ITEM_NAME == %d, \"player_stat_t layout changed\");\n", (int) STAT_SELECTED_ITEM_NAME);
    std::printf("static_assert(STAT_HEALTH_BARS == %d, \"player_stat_t layout changed\");\n", (int) STAT_HEALTH_BARS);
    std::printf("static_assert(STAT_ACTIVE_WEAPON == %d, \"player_stat_t layout changed\");\n", (int) STAT_ACTIVE_WEAPON);
    std::printf("static_assert(STAT_LAST == %d, \"player_stat_t layout changed\");\n", (int) STAT_LAST);
    std::printf("static_assert(MAX_STATS == %d, \"MAX_STATS changed\");\n", (int) MAX_STATS);
    return 0;
}
