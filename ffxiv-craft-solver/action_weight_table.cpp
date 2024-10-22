#include "action_weight_table.hpp"


static float* table = 0;

void actionWeightTableInit() {
    table = new float[ACTION_COUNT * ACTION_COUNT];
    std::fill(table, table + ACTION_COUNT * ACTION_COUNT, 0.0f);
}

float getActionWeight(ACTION prev_action, ACTION action) {
    return table[prev_action * ACTION_COUNT + action];
}
void setActionWeight(ACTION prev_action, ACTION action, float weight) {
    table[prev_action * ACTION_COUNT + action] = weight;
}

void normalizeActionWeightTable() {
    // TODO:
}

bool serializeActionWeightTable(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    // TODO: Check success
    fwrite(table, ACTION_COUNT * ACTION_COUNT * sizeof(table[0]), 1, f);
    fclose(f);
    return true;
}

bool deserializeActionWeightTable(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    // TODO: Check success
    fread(table, ACTION_COUNT * ACTION_COUNT * sizeof(table[0]), 1, f);
    fclose(f);
    return true;
}

void printActionWeightTable() {
    for (int i = 0; i < ACTION_COUNT; ++i) {
        printf("%s: ", actionToString((ACTION)i));
        for (int j = 0; j < ACTION_COUNT; ++j) {
            printf("%.2f ", table[i * ACTION_COUNT + j]);
        }
        printf("\n");
    }
}