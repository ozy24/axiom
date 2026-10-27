// Engine-facing part of eyecam (docs/design/eyecam.md): stock's chase cam
// (g_chase.cpp) picks and follows the target; this puts a first-person
// chaser at the target's eye with their view, gun and blends, and hides the
// target's body from that chaser only (SVF_INSTANCED, through the engine's
// Entity_IsVisibleToPlayer).
#include "g_local.h"

#include "core/eyecam.h"
#include "core/eyecam_hooks.h"

namespace
{
EyeCamPrefs prefs;
std::bitset<EyeCamPrefs::kSlots> applied; // this chaser has the target's view copied in

int Slot(const edict_t *ent)
{
	return (int)(ent - g_edicts) - 1;
}

bool Watching(const edict_t *ent)
{
	const edict_t *target = ent->client->chase_target;
	return deathmatch->integer && target && target->inuse && target->client && prefs.FirstPerson(Slot(ent));
}

// What the target sees, copied into the chaser's player state.
void CopyView(edict_t *ent, const edict_t *target)
{
	player_state_t &ps = ent->client->ps;
	const player_state_t &from = target->client->ps;
	ps.pmove.origin = from.pmove.origin;
	ps.pmove.velocity = from.pmove.velocity;
	ps.pmove.viewheight = from.pmove.viewheight;
	ps.viewoffset = from.viewoffset;
	ps.kick_angles = from.kick_angles;
	ps.gunangles = from.gunangles;
	ps.gunoffset = from.gunoffset;
	ps.gunindex = from.gunindex;
	ps.gunskin = from.gunskin;
	ps.gunframe = from.gunframe;
	ps.gunrate = from.gunrate;
	ps.screen_blend = from.screen_blend;
	ps.damage_blend = from.damage_blend;
	ps.rdflags = from.rdflags;
	applied.set(Slot(ent));
}

// Back to a spectator's own view: stock's third-person chase or free flight
// recompute the rest each frame.
void Release(edict_t *ent)
{
	player_state_t &ps = ent->client->ps;
	ps.pmove.viewheight = 0;
	ps.kick_angles = ps.gunangles = ps.gunoffset = {};
	ps.gunindex = ps.gunskin = ps.gunframe = 0;
	ps.gunrate = 0;
	ps.screen_blend = ps.damage_blend = {};
	ps.rdflags &= ~RDF_UNDERWATER;
	applied.reset(Slot(ent));
}
} // namespace

bool Core_EyeCam_Place(edict_t *ent, edict_t *target)
{
	if (!Watching(ent))
		return false;
	gclient_t *cl = ent->client;

	target->svflags |= SVF_INSTANCED; // its body is hidden from this chaser (Core_EyeCam_Visible)
	ent->s.origin = target->s.origin;
	cl->ps.pmove.pm_type = PM_FREEZE;
	// The view is the server's, not the chaser's input: no delta, no prediction of its own.
	cl->ps.pmove.delta_angles = {};
	cl->ps.pmove.pm_flags &= ~(PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION);
	if (target->deadflag)
	{
		// stock's death view, from where they fell
		cl->ps.viewangles[ROLL] = 40;
		cl->ps.viewangles[PITCH] = -15;
		cl->ps.viewangles[YAW] = target->client->killer_yaw;
	}
	else
	{
		cl->ps.viewangles = target->client->ps.viewangles;
		cl->v_angle = target->client->v_angle;
		AngleVectors(cl->v_angle, cl->v_forward, nullptr, nullptr);
	}
	CopyView(ent, target);
	ent->viewheight = 0;
	gi.linkentity(ent);
	return true;
}

void Core_EyeCam_SpectatorView(edict_t *ent)
{
	if (Watching(ent))
		CopyView(ent, ent->client->chase_target);
	else if (applied.test(Slot(ent)))
		Release(ent);
}

bool Core_EyeCam_Visible(const edict_t *ent, const edict_t *viewer)
{
	if (!ent->client || !viewer->client || viewer->client->chase_target != ent)
		return true;
	return !Watching(viewer);
}

namespace
{
void Flip(edict_t *ent)
{
	const bool first = prefs.Toggle(Slot(ent));
	gi.LocClient_Print(ent, PRINT_HIGH, first ? "Eyecam on: first person.\n" : "Eyecam off: third person.\n");
	ent->client->update_chase = true;
}
} // namespace

bool Core_EyeCam_Use(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (!deathmatch->integer || !cl->chase_target || cl->menu || !(cl->latched_buttons & BUTTON_USE))
		return false;
	cl->latched_buttons &= ~BUTTON_USE;
	Flip(ent);
	return true;
}

bool Core_EyeCam_Command(edict_t *ent, const char *cmd)
{
	if (Q_strcasecmp(cmd, "eyecam"))
		return false;
	Flip(ent);
	return true;
}

void Core_EyeCam_Disconnect(edict_t *ent)
{
	prefs.Forget(Slot(ent));
	applied.reset(Slot(ent));
}
