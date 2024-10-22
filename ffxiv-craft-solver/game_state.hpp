#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>
#include <set>
#include "game_config.hpp"
#include "effects.hpp"
#include "game_state_handle.hpp"

#include "action_enum.hpp"


struct GameState {
	HGAME_STATE parent;

	int progress;
	int quality;
	int durability;
	int cp;

	int step;
	int used_action_idx;

	int combo_depth = 999999;

	EffectState effects[EFFECT_COUNT] = { 0 };
	int trained_perfection_charges = 1;

	long double score = .0;
	long double max_score = .0;
	long double sum_of_squared_score = .0;
	int cp_used_on_progress = 0;
	int durability_used_on_progress = 0;
	int cp_used_on_quality = 0;
	int durability_used_on_quality = 0;
	/*	using master's mend at less than 30 missing durability,
		immaculate mend at more than 5,
		waste not tick on actions not costing durability,
		manipulation tick at cap  */
	int wasted_durability = 0;
	int n_visits = 0;
	std::vector<HGAME_STATE> children;
	int next_action_to_explore = 0;

	std::set<int> actions_expanded;
	int n_possible_moves = INT_MAX;

	void inheritState(const GameState& other, bool keep_score = false) {
		progress = other.progress;
		quality = other.quality;
		durability = other.durability;
		cp = other.cp;
		step = other.step;
		used_action_idx = other.used_action_idx;
		memcpy(effects, other.effects, sizeof(effects));
		trained_perfection_charges = other.trained_perfection_charges;
		if (keep_score) {
			score = other.score;
			max_score = other.max_score;
			sum_of_squared_score = other.sum_of_squared_score;
			n_visits = other.n_visits;
		}
		cp_used_on_progress = other.cp_used_on_progress;
		durability_used_on_progress = other.durability_used_on_progress;
		cp_used_on_quality = other.cp_used_on_quality;
		durability_used_on_quality = other.durability_used_on_quality;
		wasted_durability = other.wasted_durability;
	}

	GameState& operator=(const GameState& other) {
		assert(false);
		inheritState(other);
		return *this;
	}

	void addInnerQuiet() {
		effects[E_INNER_QUIET].n_stacks = std::min(10, effects[E_INNER_QUIET].n_stacks + 1);
	}
	void addEffect(EFFECT e, int charges, int stacks) {
		if (e == E_INNER_QUIET) {
			effects[e].n_stacks = std::min(10, effects[E_INNER_QUIET].n_stacks + 1);
		} else {
			effects[e].n_charges = charges;
			effects[e].n_stacks = stacks;
		}
	}
};

struct ActionResult {
	float progress_increase;
	float quality_increase;
	int durability_decrease;
	int cp_cost;
};
