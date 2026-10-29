// Duel's play queue, as pure logic: the clients who asked to play, in join
// order (the adapter passes only those); the first two play, the rest wait
// as spectators. After a match the loser goes to
// the back, so the winner stays on.
//
// Clients are identified by a key that outlives their connection - a human's
// social ID, a bot's slot - so a player who reconnects at a map change, in
// whatever slot, gets their place back.
#pragma once

#include <string>
#include <vector>

class DuelQueue
{
  public:
	// Bring the queue up to date with the clients now present (any order):
	// newcomers join the back, the departed are dropped, the rest keep their place.
	// With `keep_absent`, absent clients keep their place instead of being
	// dropped: at a level change every client re-enters one by one, and the
	// order (the rotation the last match decided) must survive that.
	void Sync(const std::vector<std::string> &present, bool keep_absent = false);

	// The (up to) two clients who should be playing, front first. A front
	// place held by an absent client (keep_absent) is not handed on.
	std::vector<std::string> Players() const;

	// Whether `id` should be playing.
	bool IsPlayer(const std::string &id) const;

	// `id` stops waiting to play (spectates, or leaves a duel: a forfeit).
	void Leave(const std::string &id);

	// Match over: `loser` goes to the back of the queue.
	void Rotate(const std::string &loser);

	const std::vector<std::string> &Order() const
	{
		return order_;
	}

  private:
	std::vector<std::string> order_;
};
