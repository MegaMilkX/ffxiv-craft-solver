#pragma once

#include "common.hpp"

struct GameState;

class HGAME_STATE {
	int pool_idx;
public:
	HGAME_STATE()
		: pool_idx(-1) {}
	HGAME_STATE(int idx)
		: pool_idx(idx) {}

	int getIdx() const { return pool_idx; }
	/*
	operator bool() const {
		return pool_idx != -1;
	}*/

	bool isValid() const { return pool_idx >= 0; }

	GameState* deref();
	const GameState* deref() const;

	GameState* operator->();
	const GameState* operator->() const;
	GameState& operator*();
	const GameState& operator*() const;
	bool operator==(const HGAME_STATE& other) const;
};


void initGameStatePool(int count);

HGAME_STATE createGameState(const GameState& other, bool keep_score = false);
void freeGameState(HGAME_STATE hstate);

int getAllocatedStatesCount();
