// The arena reset's movers (docs/design/match-flow.md, step 4): doors, plats,
// buttons, trains, rotating and water brushes, their triggers, timers and the
// area portals doors open, put back as they were when the level began.
//
// Each of them keeps its whole state in its own entity (its position, its
// moveinfo, what it does next and when), so a copy of those fields taken once
// the level is up, and copied back when the match goes live, restarts them.
// Taken half a second in: id's movers finish setting up in their first think
// (a door spawns its trigger, a train finds its first corner), and a copy taken
// before that would set them up twice. A mover destroyed since (a broken
// func_explosive, a used trigger_once) stays gone: only the map reload brings
// those back.
#include "g_local.h"

#include "core/movers_hooks.h"

#include <cstring>
#include <vector>

namespace
{
constexpr gtime_t kSnapshotAt = 500_ms;

// moveinfo_t without its curve: id's owns the curve's memory (so it can't be
// default-constructed or copied whole), and the curve is fixed at spawn.
struct Move
{
	vec3_t start_origin, start_angles, end_origin, end_angles, end_angles_reversed;
	int32_t sound_start = 0, sound_middle = 0, sound_end = 0;
	float accel = 0, speed = 0, decel = 0, distance = 0, wait = 0;
	move_state_t state = STATE_TOP;
	bool reversing = false;
	vec3_t dir, dest;
	float current_speed = 0, move_speed = 0, next_speed = 0, remaining_distance = 0, decel_distance = 0;
	save_moveinfo_endfunc_t endfunc;
	save_moveinfo_blocked_t blocked;
	vec3_t curve_ref;
	size_t curve_frame = 0;
	uint8_t subframe = 0, num_subframes = 0;
	size_t num_frames_done = 0;
};

struct MoverState
{
	uint32_t index = 0;
	int32_t spawn_count = 0; // the same entity, not a new one in its slot
	vec3_t origin, old_origin, angles, velocity, avelocity;
	Move moveinfo;
	save_think_t think;
	save_touch_t touch;
	save_use_t use;
	gtime_t think_in; // nextthink - the snapshot's time; 0 = none
	bool thinks = false;
	edict_t *target_ent = nullptr;
	int32_t sound = 0, frame = 0, skinnum = 0;
	effects_t effects = EF_NONE;
	renderfx_t renderfx = RF_NONE;
	solid_t solid = SOLID_NOT;
	svflags_t svflags = SVF_NONE;
	int32_t health = 0, count = 0;
	bool takedamage = false;
};

std::vector<MoverState> snapshot;
bool taken = false;
gtime_t last_time;

// Map entities with state of their own to put back: every func_ and trigger_
// entity (path_corners, targets and the like never change).
bool Tracked(const edict_t *ent)
{
	if (!ent->inuse || ent->client || !ent->classname)
		return false;
	return !strncmp(ent->classname, "func_", 5) || !strncmp(ent->classname, "trigger_", 8);
}

// Between moveinfo_t and Move, either way: the same fields by name.
template <typename To, typename From> void CopyMoveinfo(To &to, const From &from)
{
	to.start_origin = from.start_origin;
	to.start_angles = from.start_angles;
	to.end_origin = from.end_origin;
	to.end_angles = from.end_angles;
	to.end_angles_reversed = from.end_angles_reversed;
	to.sound_start = from.sound_start;
	to.sound_middle = from.sound_middle;
	to.sound_end = from.sound_end;
	to.accel = from.accel;
	to.speed = from.speed;
	to.decel = from.decel;
	to.distance = from.distance;
	to.wait = from.wait;
	to.state = from.state;
	to.reversing = from.reversing;
	to.dir = from.dir;
	to.dest = from.dest;
	to.current_speed = from.current_speed;
	to.move_speed = from.move_speed;
	to.next_speed = from.next_speed;
	to.remaining_distance = from.remaining_distance;
	to.decel_distance = from.decel_distance;
	to.endfunc = from.endfunc;
	to.blocked = from.blocked;
	to.curve_ref = from.curve_ref;
	to.curve_frame = from.curve_frame;
	to.subframe = from.subframe;
	to.num_subframes = from.num_subframes;
	to.num_frames_done = from.num_frames_done;
}

void Take()
{
	snapshot.clear();
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
	{
		const edict_t *ent = g_edicts + i;
		if (!Tracked(ent))
			continue;
		MoverState &m = snapshot.emplace_back();
		m.index = i;
		m.spawn_count = ent->spawn_count;
		m.origin = ent->s.origin;
		m.old_origin = ent->s.old_origin;
		m.angles = ent->s.angles;
		m.velocity = ent->velocity;
		m.avelocity = ent->avelocity;
		CopyMoveinfo(m.moveinfo, ent->moveinfo);
		m.think = ent->think;
		m.touch = ent->touch;
		m.use = ent->use;
		m.thinks = ent->nextthink > 0_ms;
		m.think_in = m.thinks ? ent->nextthink - level.time : 0_ms;
		m.target_ent = ent->target_ent;
		m.sound = ent->s.sound;
		m.frame = ent->s.frame;
		m.skinnum = ent->s.skinnum;
		m.effects = ent->s.effects;
		m.renderfx = ent->s.renderfx;
		m.solid = ent->solid;
		m.svflags = ent->svflags;
		m.health = ent->health;
		m.count = ent->count;
		m.takedamage = ent->takedamage;
	}
	taken = true;
}
} // namespace

void Core_Movers_Frame()
{
	if (!deathmatch->integer)
		return;
	if (level.time < last_time) // a new level
		taken = false;
	last_time = level.time;
	if (!taken && level.time >= kSnapshotAt)
		Take();
}

int Core_Movers_Reset()
{
	if (!taken)
		return 0;
	int reset = 0;
	for (const MoverState &m : snapshot)
	{
		edict_t *ent = g_edicts + m.index;
		if (!ent->inuse || ent->spawn_count != m.spawn_count)
			continue; // destroyed since, or its slot reused
		ent->s.origin = m.origin;
		ent->s.old_origin = m.old_origin;
		ent->s.angles = m.angles;
		ent->velocity = m.velocity;
		ent->avelocity = m.avelocity;
		CopyMoveinfo(ent->moveinfo, m.moveinfo);
		ent->think = m.think;
		ent->touch = m.touch;
		ent->use = m.use;
		ent->nextthink = m.thinks ? level.time + m.think_in : 0_ms;
		ent->target_ent = m.target_ent;
		ent->s.sound = m.sound;
		ent->s.frame = m.frame;
		ent->s.skinnum = m.skinnum;
		ent->s.effects = m.effects;
		ent->s.renderfx = m.renderfx;
		ent->solid = m.solid;
		ent->svflags = m.svflags;
		ent->health = m.health;
		ent->count = m.count;
		ent->takedamage = m.takedamage;
		ent->activator = nullptr;
		if (!strcmp(ent->classname, "func_areaportal"))
			gi.SetAreaPortalState(ent->style, ent->count != 0); // a door's portal, closed with it
		gi.linkentity(ent);
		++reset;
	}
	return reset;
}
