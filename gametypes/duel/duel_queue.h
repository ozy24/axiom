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
	void Sync(const std::vector<int> &present);

	// The (up to) two clients who should be playing, front first.
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
