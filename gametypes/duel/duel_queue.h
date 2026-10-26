// Duel's play queue, as pure logic: every connected client in join order; the
// first two play, the rest wait as spectators. After a match the loser goes to
// the back, so the winner stays on.
#pragma once

#include <vector>

class DuelQueue
{
  public:
	// Bring the queue up to date with the clients now present (any order):
	// newcomers join the back, the departed are dropped, the rest keep their place.
	// With `keep_absent`, absent clients keep their place instead of being
	// dropped: at a level change every client re-enters one by one, and the
	// order (the rotation the last match decided) must survive that.
	void Sync(const std::vector<int> &present, bool keep_absent = false);

	// The (up to) two clients who should be playing, front first. A front
	// place held by an absent client (keep_absent) is not handed on.
	std::vector<int> Players() const;

	// Whether `id` should be playing.
	bool IsPlayer(int id) const;

	// Match over: `loser` goes to the back of the queue.
	void Rotate(int loser);

	const std::vector<int> &Order() const
	{
		return order_;
	}

  private:
	std::vector<int> order_;
};
