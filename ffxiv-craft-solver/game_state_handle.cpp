#include "game_state_handle.hpp"

#include <assert.h>
#include <array>
#include <set>

#include "game_state.hpp"


int MAX_STATES = 0;

//static std::array<GameState, MAX_STATES> state_pool;
static GameState* state_pool = 0;
static int insert_idx = 0;
static std::set<int> free_slots;


GameState* HGAME_STATE::deref() {
	return &state_pool[pool_idx];
}
const GameState* HGAME_STATE::deref() const {
	return &state_pool[pool_idx];
}

GameState* HGAME_STATE::operator->() {
	return &state_pool[pool_idx];
}
const GameState* HGAME_STATE::operator->() const {
	return &state_pool[pool_idx];
}
GameState& HGAME_STATE::operator*() {
	return state_pool[pool_idx];
}
const GameState& HGAME_STATE::operator*() const {
	return state_pool[pool_idx];
}
bool HGAME_STATE::operator==(const HGAME_STATE& other) const {
	return this->pool_idx == other.pool_idx;
}

static int n_allocated_states = 0;

void initGameStatePool(int count) {
	MAX_STATES = count;
	state_pool = new GameState[count];
}

HGAME_STATE createGameState(const GameState& other, bool keep_score) {
	if (!free_slots.empty()) {
		int slot = *free_slots.begin();
		free_slots.erase(slot);
		state_pool[slot].inheritState(other, keep_score);
		++n_allocated_states;
		return HGAME_STATE(slot);
	}

	if (insert_idx == MAX_STATES) {
		assert(false);
		return HGAME_STATE();
	}

	int pool_idx = insert_idx;
	state_pool[insert_idx].inheritState(other, keep_score);
	++n_allocated_states;
	return HGAME_STATE(insert_idx++);
}
void freeGameState(HGAME_STATE hstate) {
	assert(hstate.isValid());
	//state_pool[state->pool_idx] = GameState();
	//memset(hstate.deref(), 0xAB, sizeof(GameState));
	--n_allocated_states;
	free_slots.insert(hstate.getIdx());
}

int getAllocatedStatesCount() {
	return n_allocated_states;
}