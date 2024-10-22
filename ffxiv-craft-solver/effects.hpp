#pragma once

#include <stdint.h>

struct GameState;

struct Effect {
	const char* name;
	bool is_stackable = false;
	void(*pfn_on_step)(GameState* state);
};

struct EffectState {
	uint16_t n_stacks;
	uint16_t n_charges;
};

enum EFFECT {
	E_NONE = -1,
	E_INNER_QUIET = 0,
	E_WASTE_NOT,
	E_VENERATION,
	E_GREAT_STRIDES,
	E_INNOVATION,
	E_FINAL_APPRAISAL,
	E_MUSCLE_MEMORY,
	E_MANIPULATION,
	E_TRAINED_PERFECTION,

	EFFECT_COUNT
};

extern Effect effects[];