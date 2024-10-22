#pragma once


struct GameContext {
	int base_progress_increase;
	int base_quality_increase;

	int max_cp;
	int target_progress;
	int target_quality;
	int max_durability;

	bool write_weight_table = false;
	bool use_weight_table = false;
};
