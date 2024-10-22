#pragma once

#include "game_state.hpp"
#include "action_enum.hpp"
#include "effects.hpp"

typedef uint32_t ACTION_FLAGS;

constexpr ACTION_FLAGS ACTION_FLAG_SYNTHESIS	= 0x01;
constexpr ACTION_FLAGS ACTION_FLAG_TOUCH		= 0x02;
constexpr ACTION_FLAGS ACTION_FLAG_ACTION		= 0x04;

struct Action {
	const char* name;
	const char* enum_name;

	int cp_cost;
	int durability_cost;

	float progress_efficiency;
	float quality_efficiency;

	ACTION_FLAGS flags;

	EFFECT effect = E_NONE;
	int effect_charges = 0;
	int effect_stacks = 0;

	bool(*pfn_is_executable)(const GameContext&, const GameState*) = [](const GameContext&, const GameState*)->bool { return true; };
	ActionResult(*pfn_on_execute)(const GameContext&, const Action&, GameState*)
		= [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			return ActionResult{
				.progress_increase = (ctx.base_progress_increase * this_action.progress_efficiency),
				.quality_increase = (ctx.base_quality_increase * this_action.quality_efficiency),
				.durability_decrease = this_action.durability_cost,
				.cp_cost = this_action.cp_cost
			};
		};

	bool isSynthesis() const { return flags & ACTION_FLAG_SYNTHESIS; }
	bool isTouch() const { return flags & ACTION_FLAG_TOUCH; }
	bool isAction() const { return flags & ACTION_FLAG_ACTION; }
};

constexpr Action actions[] = {
	{
		.name = "Basic Synthesis",
		.cp_cost = 0,
		.durability_cost = 10,
		.progress_efficiency = 1.2f,
		.quality_efficiency = .0f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_SYNTHESIS
	},
	{
		.name = "Basic Touch",
		.cp_cost = 18,
		.durability_cost = 10,
		.progress_efficiency = .0f,
		.quality_efficiency = 1.f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH
	},
	{
		.name = "Master's Mend",
		.cp_cost = 88,
		.durability_cost = -30,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->durability < ctx.max_durability;
		}
	},
	{
		.name = "Observe",
		.cp_cost = 7,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0
	},
	{
		.name = "Waste Not",
		.cp_cost = 56,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_WASTE_NOT,
		.effect_charges = 4
	},
	{
		.name = "Veneration",
		.cp_cost = 18,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_VENERATION,
		.effect_charges = 4
	},
	{
		.name = "Standard Touch",
		.cp_cost = 32,
		.durability_cost = 10,
		.progress_efficiency = .0f,
		.quality_efficiency = 1.25f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH,
		.pfn_on_execute = [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			int combo_cp_cost = this_action.cp_cost;
			if (state->used_action_idx == ACTION::BASIC_TOUCH) {
				combo_cp_cost = 18;
			}
			return ActionResult{
				.progress_increase = (ctx.base_progress_increase * this_action.progress_efficiency),
				.quality_increase = (ctx.base_quality_increase * this_action.quality_efficiency),
				.durability_decrease = this_action.durability_cost,
				.cp_cost = combo_cp_cost
			};
		}
	},
	{
		.name = "Great Strides",
		.cp_cost = 32,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_GREAT_STRIDES,
		.effect_charges = 3
	},
	{
		.name = "Innovation",
		.cp_cost = 18,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_INNOVATION,
		.effect_charges = 4
	},
	{
		.name = "Final Appraisal",
		.cp_cost = 1,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_FINAL_APPRAISAL,
		.effect_charges = 5
	},
	{
		.name = "Waste Not II",
		.cp_cost = 98,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_WASTE_NOT,
		.effect_charges = 8
	},
	{
		.name = "Byregot's Blessing",
		.cp_cost = 24,
		.durability_cost = 10,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f, // Handled by on_execute()
		.flags = ACTION_FLAG_ACTION,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->effects[E_INNER_QUIET].n_stacks > 0;
		},
		.pfn_on_execute = [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			int n_inner_quiet = state->effects[E_INNER_QUIET].n_stacks;
			state->effects[E_INNER_QUIET].n_stacks = 0;
			return ActionResult{
				.progress_increase = 0,
				.quality_increase = (ctx.base_quality_increase * (1.f + .2f * n_inner_quiet)),
				.durability_decrease = this_action.durability_cost,
				.cp_cost = this_action.cp_cost
			};
		}
	},
	{
		.name = "Muscle Memory",
		.cp_cost = 6,
		.durability_cost = 10,
		.progress_efficiency = 3.f,
		.quality_efficiency = .0f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_SYNTHESIS,
		.effect = E_MUSCLE_MEMORY,
		.effect_charges = 5,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->step == 0;
		}
	},
	{
		.name = "Careful Synthesis",
		.cp_cost = 7,
		.durability_cost = 10,
		.progress_efficiency = 1.8f,
		.quality_efficiency = .0f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_SYNTHESIS
	},
	{
		.name = "Manipulation",
		.cp_cost = 96,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_MANIPULATION,
		.effect_charges = 8
	},
	{
		.name = "Prudent Touch",
		.cp_cost = 25,
		.durability_cost = 5,
		.progress_efficiency = .0f,
		.quality_efficiency = 1.f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->effects[E_WASTE_NOT].n_charges <= 0;
		}
	},
	{
		.name = "Advanced Touch",
		.cp_cost = 46,
		.durability_cost = 10,
		.progress_efficiency = .0f,
		.quality_efficiency = 1.5f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH,
		.pfn_on_execute = [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			int combo_cp_cost = this_action.cp_cost;
			// TODO: ADD OBSERVE
			if (state->used_action_idx == ACTION::STANDARD_TOUCH
				|| state->used_action_idx == ACTION::OBSERVE) {
				combo_cp_cost = 18;
			}
			return ActionResult{
				.progress_increase = (ctx.base_progress_increase * this_action.progress_efficiency),
				.quality_increase = (ctx.base_quality_increase * this_action.quality_efficiency),
				.durability_decrease = this_action.durability_cost,
				.cp_cost = combo_cp_cost
			};
		}
	},
	{
		.name = "Reflect",
		.cp_cost = 6,
		.durability_cost = 10,
		.progress_efficiency = .0f,
		.quality_efficiency = 3.f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH,
		.effect = E_INNER_QUIET,
		.effect_stacks = 1,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->step == 0;
		}
	},
	{
		.name = "Preparatory Touch",
		.cp_cost = 40,
		.durability_cost = 20,
		.progress_efficiency = .0f,
		.quality_efficiency = 2.f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH,
		.effect = E_INNER_QUIET,
		.effect_stacks = 1
	},
	{
		.name = "Groundwork",
		.cp_cost = 18,
		.durability_cost = 20,
		.progress_efficiency = 3.6f,
		.quality_efficiency = .0f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_SYNTHESIS,
		.pfn_on_execute = [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			int durability_cost = this_action.durability_cost;
			if (state->effects[E_WASTE_NOT].n_charges > 0) {
				durability_cost = durability_cost / 2;
			}
			float efficiency_mul = (std::min(durability_cost, state->durability) / (float)durability_cost);
			float base_progress = ctx.base_progress_increase * this_action.progress_efficiency;
			float progress = base_progress * efficiency_mul;
			return ActionResult{
				.progress_increase = progress,
				.quality_increase = 0,
				.durability_decrease = this_action.durability_cost,
				.cp_cost = this_action.cp_cost
			};
			//state->progress += progress;
		}
	},
	{
		.name = "Delicate Synthesis",
		.cp_cost = 32,
		.durability_cost = 10,
		.progress_efficiency = 1.5f,
		.quality_efficiency = 1.f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH | ACTION_FLAG_SYNTHESIS
	},
	{
		.name = "Prudent Synthesis",
		.cp_cost = 18,
		.durability_cost = 10,
		.progress_efficiency = 1.8f,
		.quality_efficiency = .0f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_SYNTHESIS,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->effects[E_WASTE_NOT].n_charges <= 0;
		}
	},
	{
		.name = "Trained Finesse",
		.cp_cost = 32,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = 1.f,
		.flags = ACTION_FLAG_ACTION,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->effects[E_INNER_QUIET].n_stacks == 10;
		}
	},
	{
		.name = "Refined Touch",
		.cp_cost = 24,
		.durability_cost = 10,
		.progress_efficiency = .0f,
		.quality_efficiency = 1.f,
		.flags = ACTION_FLAG_ACTION | ACTION_FLAG_TOUCH,
		.pfn_on_execute = [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			if (state->used_action_idx == ACTION::BASIC_TOUCH) {
				state->addInnerQuiet();
			}
			return ActionResult{
				.progress_increase = (ctx.base_progress_increase * this_action.progress_efficiency),
				.quality_increase = (ctx.base_quality_increase * this_action.quality_efficiency),
				.durability_decrease = this_action.durability_cost,
				.cp_cost = this_action.cp_cost
			};
		}

	},
	{
		.name = "Immaculate Mend",
		.cp_cost = 112,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return ctx.max_durability - state->durability > 30;
		},
		.pfn_on_execute = [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			return ActionResult{
				.progress_increase = 0,
				.quality_increase = 0,
				.durability_decrease = -(ctx.max_durability - state->durability),
				.cp_cost = this_action.cp_cost
			};
			//state->durability = ctx.max_durability;
		}
	},
	{
		.name = "Trained Perfection",
		.cp_cost = 0,
		.durability_cost = 0,
		.progress_efficiency = .0f,
		.quality_efficiency = .0f,
		.flags = 0,
		.effect = E_TRAINED_PERFECTION,
		.effect_stacks = 1,
		.pfn_is_executable = [](const GameContext& ctx, const GameState* state)->bool {
			return state->trained_perfection_charges > 0;
		},
		.pfn_on_execute = [](const GameContext& ctx, const Action& this_action, GameState* state)->ActionResult {
			state->trained_perfection_charges--;
			return ActionResult{
				.progress_increase = .0f,
				.quality_increase = .0f,
				.durability_decrease = 0,
				.cp_cost = this_action.cp_cost
			};
		}
	}
};
constexpr int ACTION_ARRAY_COUNT = sizeof(actions) / sizeof(actions[0]);
static_assert(ACTION_COUNT == ACTION_ARRAY_COUNT, "Action count mismatch");


const std::vector<ACTION> combos[] = {
	{
		MANIPULATION
	},
	{
		BASIC_SYNTHESIS,
		BASIC_SYNTHESIS,
		BASIC_SYNTHESIS,
	},
	{
		MUSCLE_MEMORY,
		GROUNDWORK
	},
	{
		REFLECT,
		PREPARATORY_TOUCH,
	},
	{
		GROUNDWORK
	},
	{
		CAREFUL_SYNTHESIS,
		CAREFUL_SYNTHESIS,
		CAREFUL_SYNTHESIS
	},
	{
		DELICATE_SYNTHESIS,
		DELICATE_SYNTHESIS,
		DELICATE_SYNTHESIS
	},
	{
		IMMACULATE_MEND
	},/*
	{
		MASTERS_MEND
	},*/
	{
		BASIC_TOUCH,
		BASIC_TOUCH,
		BASIC_TOUCH
	},
	{
		BYREGOTS_BLESSING
	},
};
constexpr int COMBO_COUNT = sizeof(combos) / sizeof(combos[0]);