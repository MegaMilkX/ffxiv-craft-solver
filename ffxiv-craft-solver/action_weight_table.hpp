#pragma once

#include "actions.hpp"

void actionWeightTableInit();


float getActionWeight(ACTION prev_action, ACTION action);
void setActionWeight(ACTION prev_action, ACTION action, float weight);

void normalizeActionWeightTable();

bool serializeActionWeightTable(const char* path);
bool deserializeActionWeightTable(const char* path);

void printActionWeightTable();
