
#include <assert.h>
#include <stdio.h>
#include <exception>
#include <algorithm>
#include <vector>
#include <array>
#include <set>
#include <random>
#include <cmath>
#include <time.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "actions.hpp"
#include "timer.hpp"

#include "game_state_handle.hpp"
#include "action_weight_table.hpp"


void initGameState(const GameContext& cfg, GameState& state) {
	state.parent = HGAME_STATE();

	state.progress = 0;
	state.quality = 0;
	state.durability = cfg.max_durability;
	state.cp = cfg.max_cp;

	state.step = 0;
	state.used_action_idx = -1;
}

HGAME_STATE executeAction(const GameContext& ctx, HGAME_STATE hstate, ACTION action_idx, bool verbose = false) {
	const Action& action = actions[action_idx];

	if (hstate->durability <= 0 || hstate->progress >= ctx.target_progress) {
		return HGAME_STATE();
	}

	assert(action.pfn_is_executable);
	if (!action.pfn_is_executable(ctx, hstate.deref())) {
		return HGAME_STATE();
	}
	if (hstate->cp < action.cp_cost) {
		return HGAME_STATE();
	}

	HGAME_STATE new_state = createGameState(*hstate);
	if (!new_state.isValid()) {
		return HGAME_STATE();
	}
	++new_state->step;
	new_state->parent = hstate;

	float veneration_mul = new_state->effects[E_VENERATION].n_charges > 0 ? 0.5f : 0.f;
	float muscle_memory_mul = new_state->effects[E_MUSCLE_MEMORY].n_charges > 0 ? 1.f : .0f;

	float inner_quiet_mul = 1.0f + 0.1f * new_state->effects[E_INNER_QUIET].n_stacks;
	float great_strides_mul = new_state->effects[E_GREAT_STRIDES].n_charges > 0 ? 1.f : .0f;
	float innovation_mul = new_state->effects[E_INNOVATION].n_charges > 0 ? 1.5f : 1.f;

	assert(action.pfn_on_execute);
	ActionResult result = action.pfn_on_execute(ctx, action, new_state.deref());
	int p = result.progress_increase 
		+ result.progress_increase * veneration_mul 
		+ result.progress_increase * muscle_memory_mul;
	int q = result.quality_increase	* inner_quiet_mul * innovation_mul
		+ result.quality_increase * inner_quiet_mul * great_strides_mul;
		//+ result.quality_increase * inner_quiet_mul 
		//+ result.quality_increase * great_strides_mul 
		//+ result.quality_increase * innovation_mul;
	new_state->progress += p;
	new_state->quality += q;
	/*if (new_state->progress > ctx.target_progress) {
		new_state->progress = ctx.target_progress;
	}
	if (new_state->quality > ctx.target_quality) {
		new_state->quality = ctx.target_quality;
	}*/

	if (result.progress_increase > 0) {
		new_state->effects[E_MUSCLE_MEMORY].n_charges = 0;
	}
	if (result.quality_increase > 0) {
		new_state->effects[E_GREAT_STRIDES].n_charges = 0;
	}

	if (new_state->effects[E_FINAL_APPRAISAL].n_charges > 0) {
		if (new_state->progress >= ctx.target_progress) {
			new_state->progress = ctx.target_progress - 1;
			new_state->effects[E_FINAL_APPRAISAL].n_charges = 0;
		}
	}

	int wasted_durability = 0;
	if (action_idx == IMMACULATE_MEND) {
		wasted_durability += (ctx.max_durability - 5) - -result.durability_decrease;
	}
	if (action_idx == MASTERS_MEND) {
		wasted_durability += std::max(-result.durability_decrease, -result.durability_decrease - (ctx.max_durability - new_state->durability));
	}
	if (result.durability_decrease == 0 && new_state->effects[E_WASTE_NOT].n_charges > 0) {
		wasted_durability += 5; // Can be 10, but only if the only alternative is GW or PT. 5 is more common
	}
	new_state->wasted_durability += wasted_durability;

	int durability_decrease = 0;
	if (result.durability_decrease < 0) {
		new_state->durability = std::min(ctx.max_durability, new_state->durability - result.durability_decrease);
	} else if(new_state->effects[E_TRAINED_PERFECTION].n_stacks > 0 && result.durability_decrease > 0) {
		new_state->effects[E_TRAINED_PERFECTION].n_stacks--;
	} else if(new_state->effects[E_WASTE_NOT].n_charges > 0) {
		durability_decrease = result.durability_decrease / 2;
	} else {
		durability_decrease = result.durability_decrease;
	}
	new_state->durability -= durability_decrease;
	new_state->cp -= result.cp_cost;

	if (verbose) {
		printf("%s [", actionToString(action_idx));
		if (new_state->effects[E_INNER_QUIET].n_stacks) printf("IQ:%i", new_state->effects[E_INNER_QUIET].n_stacks);
		printf("]\n");
		if (q) printf("[->] Quality increases by %i\n", q);
		if (p) printf("[->] Progress increases by %i\n", p);
		if (durability_decrease) printf("[->] Durability decreases by %i\n", durability_decrease);
		if (result.cp_cost) printf("[->] CP decreases by %i\n", result.cp_cost);
	}

	// TODO: Not sure if Delicate Synthesis (increases both p and q) should be counted in these
	if (result.progress_increase > 0 && result.quality_increase == 0) {
		new_state->cp_used_on_progress += result.cp_cost;
		new_state->durability_used_on_progress += durability_decrease;
	}
	if (result.quality_increase > 0 && result.progress_increase == 0) {
		new_state->cp_used_on_quality += result.cp_cost;
		new_state->durability_used_on_quality += durability_decrease;
	}

	new_state->used_action_idx = action_idx;

	if (new_state->durability <= 0) {
		// If durability ran out - no effect handling
		return new_state;
	}

	// Apply 'manipulation' effect if present
	// NOTE: Manipulation's effect is not applied if manipulation was used again this turn
	if (new_state->effects[E_MANIPULATION].n_charges > 0 && action.effect != E_MANIPULATION) {
		new_state->durability = std::min(ctx.max_durability, new_state->durability + 5);
	}
	// Decrease active effects' charges
	if (action.effect != E_FINAL_APPRAISAL) {
		for (int i = 0; i < EFFECT_COUNT; ++i) {
			if (new_state->effects[i].n_charges > 0) {
				new_state->effects[i].n_charges--;
			}
		}
	}

	// Add action's effect
	if (action.effect != E_NONE) {
		new_state->addEffect(action.effect, action.effect_charges, action.effect_stacks);
	}

	if (action.isTouch()) {
		new_state->addInnerQuiet();
	}

	return new_state;
}


HGAME_STATE last_deadend_state = HGAME_STATE();

void deleteBranchImpl(HGAME_STATE state, int& count) {
	if (state->parent.isValid()) {
		deleteBranchImpl(state->parent, count);
	}
	freeGameState(state);
	++count;
}

void deleteBranch(HGAME_STATE state) {
	int count = 0;
	deleteBranchImpl(state, count);
}

HGAME_STATE copyBranchImpl(HGAME_STATE state, bool keep_score) {
	if (!state.isValid()) {
		return HGAME_STATE();
	}
	HGAME_STATE new_state = createGameState(*state, keep_score);
	new_state->parent = copyBranchImpl(state->parent, keep_score);
	return new_state;
}

HGAME_STATE copyBranch(HGAME_STATE state, bool keep_score = false) {
	return copyBranchImpl(state, keep_score);
}

int makeSequenceImpl(HGAME_STATE state, ACTION* seq, int max_len) {
	if (!state.isValid()) {
		return 0;
	}
	if (!state->parent.isValid()) {
		return 0;
	}
	int at = makeSequenceImpl(state->parent, seq, max_len);
	seq[at] = (ACTION)state->used_action_idx;
	return at + 1;
}
int makeSequence(HGAME_STATE state, ACTION* seq, int max_len) {
	int len = makeSequenceImpl(state, seq, max_len);
	return len;
}

static int macro_line_count = 0;
static int macro_part_count = 0;
void printMacroImpl(const HGAME_STATE state) {
	if (!state.isValid()) {
		printf("No successfull paths");
		return;
	}
	if (state->parent.isValid()) {
		printMacroImpl(state->parent);
	}
	if (state->used_action_idx == -1) {
		return;
	}
	if (macro_line_count == 14) {
		printf("/e Part %i complete <se.8>\n\n", macro_part_count);
		++macro_part_count;
		macro_line_count = 0;
	} 
	printf("/ac \"%s\" <wait.3>\n", actions[state->used_action_idx].name);
	/*printf("step %i: p: %i, q: %i, d: %i, cp: %i\n",
		state->step, state->progress, state->quality, state->durability, state->cp
	);*/
	++macro_line_count;
}
void printMacro(const HGAME_STATE state) {
	printMacroImpl(state);
	printf("\n");
	macro_line_count = 0;
	macro_part_count = 0;
}

void printActionArrayImpl(const HGAME_STATE state) {	
	if (!state.isValid()) {
		printf("No successfull paths");
		return;
	}
	if (state->parent.isValid()) {
		printActionArrayImpl(state->parent);
	}
	if (state->used_action_idx == -1) {
		return;
	}
	printf("\t%s,\n", actionToString((ACTION)state->used_action_idx));
}
void printActionArray(const HGAME_STATE state) {
	printf("ACTION seq[] = {\n");
	printActionArrayImpl(state);
	printf("};\n");
}
void printState(const GameContext& ctx, const GameState* state, int pool_idx) {
	printf("[%i] step %i: [%s] p: %i/%i, q: %i/%i, d: %i/%i, cp: %i/%i,",
		pool_idx,
		state->step, actionToString((ACTION)state->used_action_idx),
		state->progress, ctx.target_progress,
		state->quality, ctx.target_quality,
		state->durability, ctx.max_durability,
		state->cp, ctx.max_cp
	);
	printf("\n\tvisits: %i, score: %.6Lf, max_score: %.6Lf, p/cp: %.3Lf, p/d: %.3Lf, wd: %i,",
		state->n_visits,
		state->score,
		state->max_score,
		state->progress / (long double)state->cp_used_on_progress,
		state->progress / (long double)state->durability_used_on_progress,
		state->wasted_durability
	);
	printf("\n\tq/cp: %.3Lf, q/d: %.3Lf\n",
		state->quality / (long double)state->cp_used_on_quality,
		state->quality / (long double)state->durability_used_on_quality
	);
}
void printState(const GameContext& ctx, const HGAME_STATE state) {
	if (!state.isValid()) {
		printf("printState: invalid state handle\n");
		return;
	}
	printState(ctx, state.deref(), state.getIdx());
}

void printProgressBar(int value, int total) {
	float ratio = value / (float)total;
	const int max_blocks = 50;
	printf("[");
	for (int i = 0; i < max_blocks; ++i) {
		if (i / (float)max_blocks < ratio) {
			printf("#");
		} else {
			printf(" ");
		}
	}
	printf("] %i%%, %i/%i\n", (int)(ratio * 100), value, total);
}

void printElapsed(float sec) {
	int minutes = (int)sec / 60;
	float seconds = sec - minutes * 60;
	
	if (minutes) printf("%im ", minutes);
	if (seconds) printf("%.3fs ", seconds);
	printf("elapsed\n");
}

void printBranchCompactImpl(const HGAME_STATE state) {
	if (!state.isValid()) {
		return;
	}
	printBranchCompactImpl(state->parent);
	printf("%i, ", state->combo_depth);
}
void printBranchCompact(const HGAME_STATE state) {
	printBranchCompactImpl(state);
	printf("\n");
}

void getBranchLengthImpl(const HGAME_STATE state, int& count) {
	if (!state.isValid()) {
		return;
	}
	getBranchLengthImpl(state->parent, count);
	++count;
}
int getBranchLength(const HGAME_STATE state) {
	int count = 0;
	getBranchLengthImpl(state, count);
	return count;
}

static time_t latest_print_time = 0;
void printLatest(const GameContext& ctx) {
	if (time(0) - latest_print_time > 0) {
		printState(ctx, last_deadend_state);
		latest_print_time = time(0);
	}
}

bool storeLatestDeadendScored(const GameContext& ctx, HGAME_STATE state) {
	if (!last_deadend_state.isValid()) {
		last_deadend_state = copyBranch(state);
		printLatest(ctx);
		return true;
	}

	float score = std::min(ctx.target_progress, state->progress) * 0.45f + state->quality * 0.55f + state->durability + state->cp;
	float old_score = std::min(ctx.target_progress, last_deadend_state->progress) * 0.45f + last_deadend_state->quality * 0.55f + last_deadend_state->durability + last_deadend_state->cp;

	if (score > old_score) {
		deleteBranch(last_deadend_state);
		last_deadend_state = copyBranch(state);
		printLatest(ctx);
		return true;
	}
	return false;
}

bool storeLatestDeadend(const GameContext& ctx, HGAME_STATE state) {
	if (!last_deadend_state.isValid()) {
		last_deadend_state = copyBranch(state, true);
		printLatest(ctx);
		return true;
	}

	if (last_deadend_state->progress < ctx.target_progress) {
		/*if (state->progress == last_deadend_state->progress && state->cp > last_deadend_state->cp) {
			deleteBranch(last_deadend_state);
			last_deadend_state = copyBranch(state);
			printLatest(ctx);
			return true;
		}*/
		if (state->progress > last_deadend_state->progress) {
			deleteBranch(last_deadend_state);
			last_deadend_state = copyBranch(state, true);
			printLatest(ctx);
			return true;
		}
		return false;
	}

	if (state->progress < ctx.target_progress) {
		return false;
	}
	
	//if (last_deadend_state->quality < ctx.target_quality) {
		if (state->quality > last_deadend_state->quality) {
			deleteBranch(last_deadend_state);
			last_deadend_state = copyBranch(state, true);
			printLatest(ctx);
			return true;
		}
		else if (state->quality < last_deadend_state->quality) {
			return false;
		}
	//}
	/*
	if (state->quality < ctx.target_quality) {
		return false;
	}*/
	
	if (state->step < last_deadend_state->step) {
		deleteBranch(last_deadend_state);
		last_deadend_state = copyBranch(state, true);
		printLatest(ctx);
		return true;
	}

	return false;
}


void findSolution(const GameContext& ctx, HGAME_STATE state, int max_step) {
	if (state->durability <= 0) {
		storeLatestDeadend(ctx, state);
		freeGameState(state);
		return;
	}
	if (state->progress >= ctx.target_progress) {
		storeLatestDeadend(ctx, state);
		freeGameState(state);
		return;
	}
	
	if (state->step == max_step) {
		storeLatestDeadend(ctx, state);
		freeGameState(state);
		return;
	}
	
	for (int i = 0; i < ACTION_COUNT; ++i) {
		int action_idx = i;
		auto new_state = executeAction(ctx, state, (ACTION)action_idx);
		if (new_state.isValid()) {
			new_state->used_action_idx = action_idx;
			findSolution(ctx, new_state, max_step);
		}
	}
	

	freeGameState(state);
}


HGAME_STATE executeSequence(const GameContext& ctx, HGAME_STATE state, int max_step, const ACTION* seq, int seq_len, bool verbose = false) {
	HGAME_STATE new_state = HGAME_STATE();

	if (state->durability <= 0 || state->progress >= ctx.target_progress) {
		return new_state;
	}

	for (int i = 0; i < seq_len; ++i) {
		if (state->step >= max_step) {
			break;
		}

		HGAME_STATE tmp_new_state = executeAction(ctx, state, seq[i], verbose);
		if (!tmp_new_state.isValid()) {
			break;
		}
		new_state = tmp_new_state;
		new_state->combo_depth = i;
		new_state->used_action_idx = seq[i];

		if (new_state->durability <= 0) {
			break;
		}
		if (new_state->progress >= ctx.target_progress) {
			break;
		}
		state = new_state;
	}

	return new_state;
}

void assignActionWeightsFromTable(const GameContext& ctx, HGAME_STATE state, float* weights) {
	if (state->step == 0) {
		std::fill(weights, weights + ACTION_COUNT, .0f);
		weights[MUSCLE_MEMORY] = 1.f;
		weights[REFLECT] = 1.f;
		return;
	}
	for (int i = 0; i < ACTION_COUNT; ++i) {
		weights[i] *= getActionWeight((ACTION)state->used_action_idx, (ACTION)i);
	}
}

void assignActionWeightsManual(const GameContext& ctx, HGAME_STATE state, float* weights) {	
	/*
		BASIC_SYNTHESIS,
		BASIC_TOUCH,
		MASTERS_MEND,
		OBSERVE,
		WASTE_NOT,
		VENERATION,
		STANDARD_TOUCH,
		GREAT_STRIDES,
		INNOVATION,
		FINAL_APPRAISAL,
		WASTE_NOT_II,
		BYREGOTS_BLESSING,
		MUSCLE_MEMORY,
		CAREFUL_SYNTHESIS,
		MANIPULATION,
		PRUDENT_TOUCH,
		ADVANCED_TOUCH,
		REFLECT,
		PREPARATORY_TOUCH,
		GROUNDWORK,
		DELICATE_SYNTHESIS,
		PRUDENT_SYNTHESIS,
		TRAINED_FINESSE,
		REFINED_TOUCH,
		IMMACULATE_MEND,
		TRAINED_PERFECTION
	*/
	/*
		E_INNER_QUIET = 0,
		E_WASTE_NOT,
		E_VENERATION,
		E_GREAT_STRIDES,
		E_INNOVATION,
		E_FINAL_APPRAISAL,
		E_MUSCLE_MEMORY,
		E_MANIPULATION,
		E_TRAINED_PERFECTION,
	*/

	if (state->step == 0) {
		std::fill(weights, weights + ACTION_COUNT, .0f);
		weights[MUSCLE_MEMORY] = 1.f;
		weights[REFLECT] = 1.f;
	}

	float mm_cppd = actions[MASTERS_MEND].cp_cost / 30.L;
	float im_cppd = actions[IMMACULATE_MEND].cp_cost / (float)(ctx.max_durability - 10);
	if (mm_cppd < im_cppd) {
		weights[IMMACULATE_MEND] *= .0f;
	} else {
		weights[MASTERS_MEND] *= .0f;
	}

	if (state->trained_perfection_charges > 0) {
		weights[TRAINED_PERFECTION] *= 1.5f;
	} else {
		weights[TRAINED_PERFECTION] *= .0f;
	}
	//weights[BASIC_SYNTHESIS] *= 0.0f;

	//if (state->progress < (ctx.target_progress - ctx.base_progress_increase * 3.0f)) {
		weights[FINAL_APPRAISAL] *= .0f;
	//}

	if (state->used_action_idx == BASIC_TOUCH) {
		weights[STANDARD_TOUCH] *= 2.f;
		weights[BASIC_TOUCH] *= .0f;
	}
	if (state->used_action_idx == OBSERVE || state->used_action_idx == STANDARD_TOUCH) {
		weights[ADVANCED_TOUCH] *= 2.f;
		weights[STANDARD_TOUCH] *= .0f;
		weights[OBSERVE] *= .0f;
	}

	if (ctx.max_durability - state->durability <= 30 || state->durability > 15) {
		weights[IMMACULATE_MEND] *= .0f;
	}
	if (ctx.max_durability - state->durability < 30) {
		weights[MASTERS_MEND] *= .0f;
	}
	
	if (state->effects[E_WASTE_NOT].n_charges > 0) {
		weights[WASTE_NOT] *= .0f;
		weights[WASTE_NOT_II] *= .0f;
	}

	/*
	if (state->effects[E_INNER_QUIET].n_stacks > 0) {
		weights[BASIC_TOUCH] *= 1.5f;
		weights[STANDARD_TOUCH] *= 1.5f;
		weights[PRUDENT_TOUCH] *= 1.5f;
		weights[ADVANCED_TOUCH] *= 1.5f;
		weights[PREPARATORY_TOUCH] *= 1.5f;
		weights[DELICATE_SYNTHESIS] *= 1.5f;
		weights[TRAINED_FINESSE] *= 1.5f;
		weights[REFINED_TOUCH] *= 1.5f;
	}*/
	if (state->effects[E_INNER_QUIET].n_stacks >= 10) {
		weights[GREAT_STRIDES] *= 1.5f;
		weights[BYREGOTS_BLESSING] *= 1.5f;
	} else {
		weights[BYREGOTS_BLESSING] *= .0f;
	}
	if (state->effects[E_VENERATION].n_charges > 0) {
		weights[VENERATION] *= .0f;
		weights[INNOVATION] *= .0f;

		weights[GROUNDWORK] *= 1.5f;
		weights[BASIC_SYNTHESIS] *= 1.5f;
		weights[CAREFUL_SYNTHESIS] *= 1.5f;
		weights[DELICATE_SYNTHESIS] *= 1.5f;
		weights[PRUDENT_SYNTHESIS] *= 1.5f;
	}
	if (state->effects[E_GREAT_STRIDES].n_charges > 0) {
		weights[BYREGOTS_BLESSING] *= 1.5f;
	}
	if (state->effects[E_INNOVATION].n_charges > 0) {
		weights[INNOVATION] *= .0f;
		weights[VENERATION] *= .0f;
		
		weights[BYREGOTS_BLESSING] *= 1.3f;
		weights[BASIC_TOUCH] *= 1.5f;
		weights[STANDARD_TOUCH] *= 1.5f;
		weights[PRUDENT_TOUCH] *= 1.5f;
		weights[ADVANCED_TOUCH] *= 1.5f;
		weights[PREPARATORY_TOUCH] *= 1.5f;
		weights[DELICATE_SYNTHESIS] *= 1.5f;
		weights[PRUDENT_SYNTHESIS] *= 1.5f;
		weights[TRAINED_FINESSE] *= 1.5f;
		weights[REFINED_TOUCH] *= 1.5f;
	}
	if (state->effects[E_MUSCLE_MEMORY].n_charges > 0) {
		weights[VENERATION] *= 1.5f;
		weights[GROUNDWORK] *= 1.5f;
	}
	if (state->effects[E_TRAINED_PERFECTION].n_stacks > 0) {
		weights[GROUNDWORK] *= 1.5f;
		weights[PREPARATORY_TOUCH] *= 1.5f;
	}
	/*
	for (int i = 0; i < ACTION_COUNT; ++i) {
		weights[i] += 0.2f * ((rand() % 100) * 0.01f) - 0.1f;
	}*/
}

void assignActionWeights(const GameContext& ctx, HGAME_STATE state, float* weights) {
	if (ctx.use_weight_table) {
		assignActionWeightsFromTable(ctx, state, weights);
	} else {
		assignActionWeightsManual(ctx, state, weights);
	}
}

int selectRandomAction(const GameContext& ctx, HGAME_STATE state, float* weights) {
	static std::random_device rd;
	static std::mt19937 mt(rd());
	std::uniform_int_distribution<int> dist(0, 999);

	assignActionWeights(ctx, state, weights);

	bool all_zero = true;
	for (int i = 0; i < ACTION_COUNT; ++i) {
		if (weights[i] > .0f) {
			all_zero = false;
			break;
		}
	}
	if (all_zero) {
		return -1;
	}

	std::discrete_distribution<int> distr(weights, weights + ACTION_COUNT);
	int selected_action = distr(mt);

	return selected_action;
}

int selectBestAction(const GameContext& ctx, HGAME_STATE state, float* weights) {
	assignActionWeights(ctx, state, weights);

	typedef std::pair<float, ACTION> pair_t;
	pair_t sorted[ACTION_COUNT];
	for (int i = 0; i < ACTION_COUNT; ++i) {
		sorted[i].first = weights[i];
		sorted[i].second = (ACTION)i;
	}
	std::sort(sorted, sorted + ACTION_COUNT, [](auto a, auto b)->bool { return a.first > b.first; });

	if (sorted[0].first < FLT_EPSILON) {
		return -1;
	}
	return sorted[0].second;
}

HGAME_STATE executeRandomSequence(const GameContext& ctx, HGAME_STATE state, int max_step, int max_seq, int& total_durability_spent) {
	HGAME_STATE new_state = HGAME_STATE();

	if (state->durability <= 0 || state->progress >= ctx.target_progress) {
		return new_state;
	}

	const int MAX_SEQUENCE = max_seq;

	for (int i = 0; i < MAX_SEQUENCE; ++i) {
		if (state->step >= max_step) {
			break;
		}
		if (state->durability <= 0) {
			break;
		}
		if (state->progress >= ctx.target_progress) {
			break;
		}

		int action_idx = -1;
		float weights[ACTION_COUNT];
		std::fill(weights, weights + ACTION_COUNT, 1.f);
		
		for (int j = 0; j < ACTION_COUNT; ++j) {
			const Action& action = actions[j];
			HGAME_STATE st = executeAction(ctx, state, (ACTION)j);
			if (!st.isValid()) {
				weights[j] = .0f;
				continue;
			}/*
			if (st->durability <= 0 && st->progress < ctx.target_progress) {
				weights[j] = .0f;
			}*/
			freeGameState(st);/*
			if (state->cp < action.cp_cost) {
				continue;
			}*/
		}

		action_idx = selectRandomAction(ctx, state, weights);
		if (action_idx == -1) {
			break;
		}
		

		HGAME_STATE tmp_new_state = executeAction(ctx, state, (ACTION)action_idx);
		if (!tmp_new_state.isValid()) {
			break;
		}
		new_state = tmp_new_state;
		new_state->combo_depth = i;
		new_state->used_action_idx = action_idx;

		if (state->durability > new_state->durability) {
			total_durability_spent += state->durability - new_state->durability;
		}

		//state->children.push_back(new_state);
		state->actions_expanded.insert(action_idx);
		state = new_state;
	}

	return new_state;
}

HGAME_STATE freeComboBranchImpl(HGAME_STATE state, int depth, int& count) {
	if (!state.isValid()) {
		return HGAME_STATE();
	}
	if (depth != state->combo_depth) {
		return state;
	}

	HGAME_STATE new_head = freeComboBranchImpl(state->parent, depth - 1, count);
	
	freeGameState(state);
	++count;
	return new_head;
}
HGAME_STATE freeComboBranch(HGAME_STATE state) {
	int count = 0;
	HGAME_STATE new_head = freeComboBranchImpl(state, state->combo_depth, count);
	return new_head;
}

void findSolutionWithCombos(const GameContext& ctx, HGAME_STATE state, int max_step) {
	if (state->durability <= 0) {
		storeLatestDeadend(ctx, state);
		freeComboBranch(state);
		return;
	}
	if (state->progress >= ctx.target_progress) {
		storeLatestDeadend(ctx, state);
		freeComboBranch(state);
		return;
	}

	if (state->step == max_step) {
		storeLatestDeadend(ctx, state);
		freeComboBranch(state);
		return;
	}

	for (int i = 0; i < COMBO_COUNT; ++i) {
		auto new_state = executeSequence(ctx, state, max_step, combos[i].data(), combos[i].size());
		if (new_state.isValid()) {
			findSolutionWithCombos(ctx, new_state, max_step);
			//freeComboBranch(new_state);
		}
	}

	freeComboBranch(state);
}

void removeFromSequence(ACTION* seq, int len, int remove_at) {
	if (remove_at == len - 1) {
		// TODO: ???
		return;
	}
	for (int i = remove_at; i < len; ++i) {
		seq[i] = seq[i + 1];
	}
}

void fillRandomSequence(ACTION* seq, int len) {
	static std::random_device rd;
	static std::mt19937 mt(rd());
	static std::uniform_int_distribution<int> dist(0, ACTION_COUNT - 1);
	for (int i = 0; i < len; ++i) {
		int action_idx = dist(mt);
		seq[i] = (ACTION)action_idx;
	}
}

void fillRandomSequence2(const GameContext& ctx, const HGAME_STATE state, ACTION* seq, int len) {
	static std::random_device rd;
	static std::mt19937 mt(rd());
	static std::uniform_int_distribution<int> dist(0, ACTION_COUNT - 1);

	EFFECT effects_[EFFECT_COUNT];
	memcpy(effects_, state->effects, std::min(sizeof(effects_), sizeof(state->effects)));

	for (int i = 0; i < len; ++i) {
		int action_idx = dist(mt);
		seq[i] = (ACTION)action_idx;
	}
}

void propagateScoreImpl(const GameContext& ctx, HGAME_STATE state, long double eval, long double max_score, int visits) {
	if (state->parent.isValid()) {
		propagateScoreImpl(ctx, state->parent, eval, max_score, visits);
	}
	state->score += eval;
	state->max_score = std::max(state->max_score, max_score);
	state->sum_of_squared_score += std::powl(eval, 2.L);
	state->n_visits += visits;

	if (ctx.write_weight_table && state->parent.isValid()) {
		ACTION prev = (ACTION)state->parent->used_action_idx;
		ACTION a = (ACTION)state->used_action_idx;
		float w = getActionWeight(prev, a);
		setActionWeight(prev, a, std::max(w, (float)state->max_score));
	}
}
void propagateScore(const GameContext& ctx, HGAME_STATE state, long double eval, long double max_score, int visits) {
	if (state->parent.isValid()) {
		propagateScoreImpl(ctx, state->parent, eval, max_score, visits);
	}
	state->score = eval;
	state->max_score = std::max(state->max_score, max_score);
	state->sum_of_squared_score += std::powl(eval, 2.L);
	state->n_visits += visits;

	if (ctx.write_weight_table && state->parent.isValid()) {
		ACTION prev = (ACTION)state->parent->used_action_idx;
		ACTION a = (ACTION)state->used_action_idx;
		float w = getActionWeight(prev, a);
		setActionWeight(prev, a, std::max(w, (float)state->max_score));
	}
}

void monteCarloSearch(const GameContext& ctx, HGAME_STATE state, int depth = 0) {
	std::vector<HGAME_STATE>& children = state->children;

	for (int i = 0; i < ACTION_COUNT; ++i) {
		if (i == FINAL_APPRAISAL || i == OBSERVE) {
			continue;
		}
		HGAME_STATE child = executeAction(ctx, state, (ACTION)i);
		if (child.isValid()) {
			children.push_back(child);
		}
	}


	const int MAX_SEQ_LEN = 36;
	ACTION seq[MAX_SEQ_LEN];
	int ACTUAL_MAX_SEQ_LEN = MAX_SEQ_LEN - depth;
	long double depth_ratio = ACTUAL_MAX_SEQ_LEN / (long double)MAX_SEQ_LEN;
	long double depth_ratio2 = depth_ratio * depth_ratio;

	const int MAX_ITERATIONS = 15'000;
	const int N_ITERATIONS = MAX_ITERATIONS * (ACTUAL_MAX_SEQ_LEN / (long double)MAX_SEQ_LEN);

	for (int i = 0; i < children.size(); ++i) {
		long double total_score = .0;

		for (int j = 0; j < N_ITERATIONS; ++j) {
			fillRandomSequence(seq, ACTUAL_MAX_SEQ_LEN);
			HGAME_STATE head = executeSequence(ctx, children[i], ACTUAL_MAX_SEQ_LEN, seq, ACTUAL_MAX_SEQ_LEN);
			int tds = 0;
			//HGAME_STATE head = executeRandomSequence(ctx, children[i], std::min(state->step + 6, MAX_SEQ_LEN), tds);
			if (!head.isValid()) {
				total_score -= 1000.L;
				//total_score *= .5L;
				continue;
			}
			
			//if (head->progress >= ctx.target_progress) {
				//long double q = head->quality;
				//total_eval = total_eval + q * std::min(1.0L, std::max(0.2L * depth_ratio2, head->progress / (long double)ctx.target_progress));
				long double dq = std::min(ctx.target_quality, head->quality) - state->quality;
				long double dp = std::min(ctx.target_progress, head->progress) - state->progress;
				long double dcp = state->cp - head->cp;
				long double ds = head->step - state->step;
				if (tds == 0) {
					tds = 1;
				}
				if (dcp) {
					dcp = ctx.max_cp;
				}
				
				total_score += (dq + dp) / tds + (dq + dp) / (dcp * .1f);
				//total_score += ((dq + dp) - tds - ds) * (1.L / ds);// - dcp * 1.L;
				//total_score *= .5L;
				/*if (dcp > .0) {
					total_eval += (dq + dp) - dcp * 1.L;
				}*/
			//}
			freeComboBranch(head);
		}
		//children[i]->evaluation = total_eval;
		propagateScore(ctx, children[i], total_score, total_score, 0);
	}

	std::sort(children.begin(), children.end(), [](const HGAME_STATE& l, const HGAME_STATE& r)->bool{
		return l->score > r->score;
	});
	const int MAX_CHILDREN = 1;
	for (int i = MAX_CHILDREN; i < children.size(); ++i) {
		freeGameState(children[i]);
	}
	children.resize(std::min(MAX_CHILDREN, (int)children.size()));

	static time_t last_time = 0;
	if (time(0) - last_time > 1) {
		last_time = time(0);
		for (int i = 0; i < std::min(MAX_CHILDREN, (int)children.size()); ++i) {
			printMacro(children[i]);
			printf("----------\n");
		}
		for (int i = 0; i < std::min(MAX_CHILDREN, (int)children.size()); ++i) {
			printState(ctx, children[i]);
		}

		printf("==========\n");
		for (int i = 0; i < std::min(MAX_CHILDREN, (int)children.size()); ++i) {
			monteCarloSearch(ctx, children[i], depth + 1);
		}
	}
	/*
	if (children.empty()) {
		// TODO:
		return;
	}
	printMacro(children[0]);
	printf("==========\n");
	
	monteCarloSearch(ctx, children[0], depth + 1);*/
}

static int total_playouts = 0;
static double long best_score = .0L;
long double calcUCT(HGAME_STATE parent, HGAME_STATE child, long double in_explore_constant, float max_score_weight_, int depth, int max_depth) {
	//long double score = child->wins;
	//long double win_ratio = score / (child->wins + child->losses);
	long double max_score_weight = max_score_weight_;
	long double depth_ratio = std::min(1.0L, 1.0L - depth / (long double)max_depth);
	long double C = 1.0L * in_explore_constant;
	long double average_score = child->score / (long double)child->n_visits;
	long double max_score = child->max_score;
	long double exploitation = max_score * max_score_weight + average_score * (1.0L - max_score_weight);
	long double NUM = std::log(parent->n_visits);
	long double DENOM = (long double)child->n_visits;
	long double exploration = C * std::sqrtl(NUM / DENOM);
	long double UCT = exploitation + exploration;

	// single player term
	long double D = 1000.0L;
	long double SP = std::sqrtl((child->sum_of_squared_score - child->n_visits * std::powl(max_score, 2.L) + D) / (child->n_visits));

	//long double UCT = (child->max_score) + (.0L * std::sqrtl(std::log(parent->n_visits) / (long double)child->n_visits));
	return UCT;// + SP;
}

HGAME_STATE monteCarloSelect(const GameContext& ctx, HGAME_STATE state, int max_depth, float explore_constant, float max_score_weight, int depth = 0) {
	if (depth > 50) {
		printf("! SELECT IS TOO DEEP: %i\n", depth);
		printState(ctx, state->parent);
		printState(ctx, state);
	}

	++state->n_visits;

	if (state->children.empty()) {
		return state;
	}
	if (state->n_possible_moves > 0) {
		return state;
	}

	//long double depth_mul = std::max(0.0L, (1.0L - depth / 25.0L));
	float C = explore_constant;// * (0.2f + 0.8f * (1.0f - depth / (float)max_depth));
	HGAME_STATE result = state->children[rand() % state->children.size()];
	long double max_uct = calcUCT(state, result, C, max_score_weight, depth, max_depth);
	for (auto& ch : state->children) {
		long double UCT = calcUCT(state, ch, C, max_score_weight, depth, max_depth);
		if (UCT >= max_uct) {
			max_uct = UCT;
			result = ch;
		}
	}
	if (result.isValid()) {
		return monteCarloSelect(ctx, result, max_depth, explore_constant, max_score_weight, depth + 1);
	}
	return result;
}

static int n_deleted_states = 0;
HGAME_STATE monteCarloSelect2(const GameContext& ctx, HGAME_STATE state, int max_depth, float explore_constant, float max_score_weight, int depth = 0) {
	++state->n_visits;

	if (state->children.empty() && state->n_possible_moves == 0) {
		if (state->progress < ctx.target_progress) {
			return HGAME_STATE();
		}
		if(state->progress >= ctx.target_progress
			&& last_deadend_state.isValid()
			&& last_deadend_state->progress >= ctx.target_progress
			&& state->quality < last_deadend_state->quality
		) {
			return HGAME_STATE();
		}
	}

	if (state->children.empty()) {
		return state;
	}
	if (state->n_possible_moves > 0) {
		return state;
	}

	float C_bonus = .0f;//.4f * std::min(1.f, (1.f - depth / ((float)max_depth * .5f)));
	float C = explore_constant * (1.f + C_bonus);
	typedef std::pair<float, HGAME_STATE> pair_t;
	std::vector<pair_t> sorted(state->children.size());
	for (int i = 0; i < state->children.size(); ++i) {
		auto& ch = state->children[i];
		sorted[i].first = calcUCT(state, ch, C, max_score_weight, depth, max_depth);
		sorted[i].second = ch;
	}
	std::sort(sorted.begin(), sorted.end(), [](auto a, auto b)->bool { return a.first > b.first; });;

	for (int i = 0; i < sorted.size(); ++i) {
		float uct = sorted[i].first;
		auto ch = sorted[i].second;
		HGAME_STATE selected = monteCarloSelect2(ctx, ch, max_depth, C, max_score_weight, depth + 1);
		if (selected.isValid()) {
			return selected;
		}

		auto pos = std::find(state->children.begin(), state->children.end(), ch);
		state->children.erase(pos);
		freeGameState(ch);
		++n_deleted_states;
	}

	return HGAME_STATE();
}

long double monteCarloScore(const GameContext& ctx, HGAME_STATE state) {
	long double mm_cppd = actions[MASTERS_MEND].cp_cost / 30.L;
	long double im_cppd = actions[IMMACULATE_MEND].cp_cost / (long double)(ctx.max_durability - 10);
	long double durability_effective_cp_value = std::min(mm_cppd, im_cppd);
	long double durability_as_cp_used_on_progress
		= durability_effective_cp_value * (state->durability_used_on_progress);
	long double durability_as_cp_used_on_quality
		= durability_effective_cp_value * (state->durability_used_on_quality);

	long double total_cp_used_on_progress = state->cp_used_on_progress + durability_as_cp_used_on_progress;
	int capped_progress = std::min(ctx.target_progress, state->progress);
	long double worst_progress_per_cp = ctx.target_progress / (long double)ctx.max_cp;
	long double progress_per_cp = total_cp_used_on_progress == 0 ? .0L : capped_progress / total_cp_used_on_progress;
	long double ppcp_ratio = progress_per_cp / worst_progress_per_cp;

	long double total_cp_used_on_quality = state->cp_used_on_quality + durability_as_cp_used_on_quality;
	long double worst_quality_per_cp = ctx.target_quality / (long double)ctx.max_cp;
	long double quality_per_cp = total_cp_used_on_quality = 0 ? .0L : state->quality / total_cp_used_on_quality;
	long double qpcp_ratio = quality_per_cp / worst_quality_per_cp;

	int wasted_progress = std::max(0, state->progress - ctx.target_progress);
	long double wp_ratio = 1.0L - wasted_progress / (ctx.base_progress_increase * actions[GROUNDWORK].progress_efficiency);

	long double p_score = 0.45L * std::min(1.0L, state->progress / (long double)ctx.target_progress);
	long double q_score = std::min(1.0L, state->quality / (long double)ctx.target_quality);
	long double q_mul = 1.0L + 2.0L * std::min(1.0L, state->quality / (long double)ctx.target_quality);
	long double cp_score = 0.05L * std::min(1.0L, 1.0L - state->cp / (long double)ctx.max_cp);
	long double d_score = 0.05L * std::min(1.0L, state->durability / (long double)ctx.max_durability);
	long double finish_bonus = state->progress >= ctx.target_progress ? 1.0L : .0L;
	//long double score = (p_score + q_score + d_score + cp_score);
	long double score = (q_score * q_score * ppcp_ratio) * finish_bonus;// *q_mul;

	/*
	long double p_score = 0.40L * std::min(1.0L, state->progress / (long double)ctx.target_progress);
	long double q_score = 0.50L * std::min(1.0L, state->quality / (long double)ctx.target_quality);
	long double cp_score = 0.05L * std::min(1.0L, state->cp / (long double)ctx.max_cp);
	long double d_score = 0.05L * std::min(1.0L, state->durability / (long double)ctx.max_durability);
	long double score = p_score + q_score + cp_score + d_score;*/
	return score;
}

void insertComboBranchAsChildren(HGAME_STATE head) {
	if (!head->parent.isValid()) {
		return;
	}

	head->parent->children.push_back(head);
	head->parent->actions_expanded.insert(head->used_action_idx);

	if (head->parent->combo_depth < head->combo_depth) {
		insertComboBranchAsChildren(head->parent);
	}
}

void monteCarloSimulate(const GameContext& ctx, HGAME_STATE state, int max_steps) {
	const int MAX_STEPS = max_steps;
	const int SEQ_ARRAY_LEN = 50;
	ACTION seq[SEQ_ARRAY_LEN];
	int MAX_SEQ_LEN = MAX_STEPS;// -state->step;
	const int MAX_ITERATIONS = 1;
	const int N_ITERATIONS = 1;// MAX_ITERATIONS* std::min(1.0L, 0.2L + (MAX_SEQ_LEN / (long double)MAX_STEPS));

	long double total_score = .0;
	long double max_score = .0;

	for (int j = 0; j < N_ITERATIONS; ++j) {
		int tds = 0;
		//fillRandomSequence(seq, MAX_SEQ_LEN);
		//fillRandomSequence2(ctx, state, seq, MAX_SEQ_LEN);
		//HGAME_STATE head = executeSequence(ctx, state, MAX_STEPS, seq, MAX_SEQ_LEN);
		HGAME_STATE head = executeRandomSequence(ctx, state, MAX_STEPS, MAX_SEQ_LEN, tds);
		if (!head.isValid()) {
			//total_score -= 1000.L;
			//total_score *= .5L;
			continue;
		}
		
		long double score = monteCarloScore(ctx, head);

		if (best_score < score) {
			best_score = score;
		}

		if(head->progress >= ctx.target_progress) {
			storeLatestDeadend(ctx, head);
		}
		total_score += score;
		if (score > max_score) {
			max_score = score;
		}
		
		propagateScore(ctx, head, total_score, max_score, 0);
		state->n_visits++;

		++total_playouts;
		insertComboBranchAsChildren(head);
		//freeComboBranch(head);
	}
}

bool monteCarloExpandAndSimulate(const GameContext& ctx, HGAME_STATE state, int max_steps) {
	bool any_expansions = false;
	for (int i = 0; i < ACTION_COUNT; ++i) {
		/*if (i == FINAL_APPRAISAL || i == OBSERVE) {
			continue;
		}
		if (i == WASTE_NOT || i == WASTE_NOT_II && state->effects[E_WASTE_NOT].n_charges > 1) {
			continue;
		}
		if (i == VENERATION && state->effects[E_VENERATION].n_charges > 1) {
			continue;
		}
		if (i == INNOVATION && state->effects[E_INNOVATION].n_charges > 1) {
			continue;
		}
		if (i == MANIPULATION && state->effects[E_MANIPULATION].n_charges > 1) {
			continue;
		}
		if (i == BYREGOTS_BLESSING && state->effects[E_GREAT_STRIDES].n_charges == 0) {
			continue;
		}*/
		HGAME_STATE child = executeAction(ctx, state, (ACTION)i);
		if (child.isValid()) {
			any_expansions = true;
			state->children.push_back(child);
			monteCarloSimulate(ctx, child, max_steps);
		}
	}
	return any_expansions;
}
bool monteCarloExpandAndSimulate2(const GameContext& ctx, HGAME_STATE state, int max_steps) {
	float weights[ACTION_COUNT];
	std::fill(weights, weights + ACTION_COUNT, 1.f);

	if (state->step >= max_steps) {
		state->n_possible_moves = 0;
		return false;
	}

	for (int j = 0; j < ACTION_COUNT; ++j) {
		const Action& action = actions[j];
		if (state->actions_expanded.count(j)) {
			weights[j] = .0f;
			continue;
		}
		HGAME_STATE st = executeAction(ctx, state, (ACTION)j);
		if (!st.isValid()) {
			weights[j] = .0f;
			continue;
		}
		if (st->durability <= 0 && st->progress < ctx.target_progress) {
			weights[j] = .0f;
		}
		freeGameState(st);
	}

	int action_idx = selectBestAction(ctx, state, weights);
	if (action_idx == -1) {
		state->n_possible_moves = 0;
		return false;
	}

	int possible_moves = 0;
	for (int i = 0; i < ACTION_COUNT; ++i) {
		if (weights[i] > .0f) {
			++possible_moves;
		}
	}

	if (possible_moves - (int)state->children.size() <= 0) {
		assert(possible_moves - (int)state->children.size() == 0);
		state->n_possible_moves = 0;
		return false;
	}

	HGAME_STATE child = executeAction(ctx, state, (ACTION)action_idx);
	if (child.isValid()) {
		state->children.push_back(child);
		state->actions_expanded.insert(action_idx);
		possible_moves -= state->children.size();
		state->n_possible_moves = possible_moves;
		if (child->progress >= ctx.target_progress) {
			storeLatestDeadend(ctx, child);
		}
		monteCarloSimulate(ctx, child, max_steps);
		return true;
	}

	state->actions_expanded.insert(action_idx);
	possible_moves -= state->children.size();
	state->n_possible_moves = possible_moves;
	return false;
}

struct MonteCarloResult {
	HGAME_STATE best_leaf;
	float useless_selection_ratio;
};

MonteCarloResult monteCarloSearch2(const GameContext& ctx, HGAME_STATE state_, int n_iterations, int max_steps, float exploration_constant, float max_score_weight) {
	total_playouts = 0;
	n_deleted_states = 0;
	/*
	{
		HGAME_STATE child = executeAction(ctx, state, ACTION::MUSCLE_MEMORY);
		if (child.isValid()) {
			state->children.push_back(child);
			monteCarloSimulate(ctx, child);
		}
		child = executeAction(ctx, state, ACTION::REFLECT);
		if (child.isValid()) {
			state->children.push_back(child);
			monteCarloSimulate(ctx, child);
		}
	}*/

	int n_useless_selections = 0;
	const int N_ITERATIONS = n_iterations;
	HGAME_STATE st_selected = HGAME_STATE();
	HGAME_STATE state = state_;
	for(int i = 0; i < N_ITERATIONS; ++i) {
		st_selected = monteCarloSelect2(ctx, state, max_steps, exploration_constant, max_score_weight);

		assert(st_selected.isValid());

		static time_t last_time = 0;
		if (time(0) - last_time > 1) {
			last_time = time(0);
			printf("==========\n");
			printMacro(st_selected);
			printState(ctx, st_selected);
			printProgressBar(i, N_ITERATIONS);
		}

		if (st_selected->progress < ctx.target_progress && st_selected->durability <= 0) {
			++n_useless_selections;
			//long double score = 0;// monteCarloScore(ctx, st_selected);
			//propagateScore(ctx, st_selected, score, score, 0);
			continue;
		}

		if (st_selected->progress >= ctx.target_progress && st_selected->durability <= 0) {
			++n_useless_selections;
			//long double score = monteCarloScore(ctx, st_selected);
			//propagateScore(st_selected, score, score, 0);
			continue;
		}

		if (!monteCarloExpandAndSimulate2(ctx, st_selected, max_steps)) {
			++n_useless_selections;
			//--i;
			continue;
		}
	}

	st_selected = monteCarloSelect(ctx, state, max_steps, .0f, 1.0f);

	return MonteCarloResult{ 
		.best_leaf = st_selected, 
		.useless_selection_ratio = n_useless_selections / ((float)N_ITERATIONS)
	};
}

int countBadDeadends(const GameContext& ctx, HGAME_STATE state) {
	if (state->children.empty() && state->n_possible_moves == 0) {
		if (state->progress < ctx.target_progress) {
			return 1;
		}
		if (state->progress >= ctx.target_progress
			&& last_deadend_state.isValid()
			&& last_deadend_state->progress >= ctx.target_progress
			&& state->quality < last_deadend_state->quality
			) {
			return 1;
		}
	}

	int count = 0;
	for (int i = 0; i < state->children.size(); ++i) {
		count += countBadDeadends(ctx, state->children[i]);
	}
	return count;
}

#define TEST_SCORE(seq) { HGAME_STATE st = executeSequence(ctx, state_, 45, seq, sizeof(seq) / sizeof(seq[0])); \
long double score = monteCarloScore(ctx, st); \
st->score = score; \
printState(ctx, st); }

void testScoring(const GameContext& ctx, HGAME_STATE state_) {
	ACTION seq[] = {
		MUSCLE_MEMORY,
		WASTE_NOT_II,
		MANIPULATION,
		VENERATION,
		GROUNDWORK,
		GROUNDWORK,
		DELICATE_SYNTHESIS,
		GROUNDWORK,
		PREPARATORY_TOUCH,
		PREPARATORY_TOUCH,
		TRAINED_PERFECTION,
		INNOVATION,
		PREPARATORY_TOUCH,
		DELICATE_SYNTHESIS,
		BASIC_TOUCH,
		DELICATE_SYNTHESIS,
		GREAT_STRIDES,
		INNOVATION,
		BYREGOTS_BLESSING,
		BASIC_SYNTHESIS
	};
	ACTION seq2[] = {
		MUSCLE_MEMORY,
		VENERATION,
		WASTE_NOT,
		GROUNDWORK,
		GROUNDWORK,
		GROUNDWORK,
		BASIC_TOUCH,
		VENERATION,
		DELICATE_SYNTHESIS,
		IMMACULATE_MEND,
		DELICATE_SYNTHESIS,
		TRAINED_PERFECTION,
		PREPARATORY_TOUCH,
		INNOVATION,
		PRUDENT_TOUCH,
		STANDARD_TOUCH,
		BASIC_TOUCH,
		STANDARD_TOUCH,
		INNOVATION,
		BASIC_TOUCH,
		GREAT_STRIDES,
		BYREGOTS_BLESSING,
		CAREFUL_SYNTHESIS,
	};
	ACTION seq3[] = { // NOTE: Weird results: p: 7610/7500, q: 9834/16500, d: -5/70, cp: 3/598
		MUSCLE_MEMORY,
		TRAINED_PERFECTION,
		VENERATION,
		GROUNDWORK,
		WASTE_NOT,
		DELICATE_SYNTHESIS,
		GROUNDWORK,
		VENERATION,
		DELICATE_SYNTHESIS,
		DELICATE_SYNTHESIS,
		DELICATE_SYNTHESIS,
		BASIC_SYNTHESIS,
		IMMACULATE_MEND,
		INNOVATION,
		PRUDENT_TOUCH,
		BASIC_TOUCH,
		STANDARD_TOUCH,
		ADVANCED_TOUCH,
		INNOVATION,
		STANDARD_TOUCH,
		ADVANCED_TOUCH,
		GREAT_STRIDES,
		BYREGOTS_BLESSING,
		BASIC_SYNTHESIS,
	};
	ACTION seq4[] = {
		MUSCLE_MEMORY,
		GROUNDWORK,
		MASTERS_MEND,
		WASTE_NOT_II,
		TRAINED_PERFECTION,
		VENERATION,
		GROUNDWORK,
		DELICATE_SYNTHESIS,
		DELICATE_SYNTHESIS,
		GROUNDWORK,
		WASTE_NOT,
		INNOVATION,
		PREPARATORY_TOUCH,
		PREPARATORY_TOUCH,
		STANDARD_TOUCH,
		ADVANCED_TOUCH,
		VENERATION,
		PRUDENT_SYNTHESIS,
		CAREFUL_SYNTHESIS,
	};

	TEST_SCORE(seq);
	TEST_SCORE(seq2);
	TEST_SCORE(seq3);
	TEST_SCORE(seq4);
}

// Grade 2 Gemdraught of Intelligence
GameContext ctx = {
	.base_progress_increase = 259,
	.base_quality_increase = 256,
	.max_cp = 598,
	.target_progress = 7500,
	.target_quality = 16500,
	.max_durability = 70
};
// Grade 2 Gemsap of Mind
/*GameContext ctx = {
	.base_progress_increase = 259,
	.base_quality_increase = 256,
	.max_cp = 598,
	.target_progress = 4125,
	.target_quality = 12000,
	.max_durability = 35
};*/
// Commanding Craftsman's Tisane
/*GameContext ctx = {
	.base_progress_increase = 309,
	.base_quality_increase = 368,
	.max_cp = 598,
	.target_progress = 5400,
	.target_quality = 10200,
	.max_durability = 80
};*//*
// Enchanted High Durium Ink
GameContext ctx = {
	.base_progress_increase = 403,
	.base_quality_increase = 473,
	.max_cp = 598,
	.target_progress = 1000,
	.target_quality = 5200,
	.max_durability = 40
};*/
/*
// Sanctified Water
GameContext ctx = {
	.base_progress_increase = 304,
	.base_quality_increase = 361,
	.max_cp = 598,
	.target_progress = 2850,
	.target_quality = 10600,
	.max_durability = 40
};*/

void onBreak() {
	printMacro(last_deadend_state);
	printState(ctx, last_deadend_state);
	printf("allocated states: %i\n", getAllocatedStatesCount());
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
	switch (fdwCtrlType) {
	case CTRL_C_EVENT:
		onBreak();
		exit(1);
		return TRUE;
	}
	return FALSE;
}

int main() {
	initGameStatePool(32'000'000);
	actionWeightTableInit();

	timerBegin();
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	HGAME_STATE root_state = createGameState(GameState());
	initGameState(ctx, *root_state);

	//testScoring(ctx, root_state);

	//monteCarloSearch(ctx, root_state);
	
	//printActionWeightTable();

	deserializeActionWeightTable("weight_table_best.bin");
	//printActionWeightTable();
	
	MonteCarloResult result = monteCarloSearch2(ctx, root_state, 2'000'000, 26, 3.0f, 0.3f);
	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	printActionArray(result.best_leaf);
	printMacro(result.best_leaf);
	printState(ctx, result.best_leaf);
	printf("Deadend selection ratio: %.3Lf\n", result.useless_selection_ratio);
	printf("Deleted states: %i\n", n_deleted_states);
	printf("Bad deadends: %i\n", countBadDeadends(ctx, root_state));
	printf("Root visits: %i\n", root_state->n_visits);
	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	
	printActionArray(last_deadend_state);
	printMacro(last_deadend_state);
	printState(ctx, last_deadend_state);

	printf("allocated states: %i\n", getAllocatedStatesCount());
	printElapsed(timerEnd());
	return 0;
}