#include <stdio.h>
#include <math.h>
#include "include/dispatch_algorithms.h"

// 定義 VIP 門檻
#define VIP_RATING_THRESHOLD 4.8

// 實作距離計算
double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    return sqrt(pow(lat1 - lat2, 2) + pow(lon1 - lon2, 2));
}

// 實作策略 A: Basic
int find_driver_basic(SharedState *state) {
    int best_index = -1;
    double min_dist = 1000000.0;

    for (int i = 0; i < state->driver_count; i++) {
        Driver *d = &state->drivers[i];
        if (d->is_available && !d->is_refueling && d->fuel > 0) {
            double dist = calculate_distance(25.0330, 121.5654, d->lat, d->lon);
            if (dist < min_dist) {
                min_dist = dist;
                best_index = i;
            }
        }
    }
    return best_index;
}

// 實作策略 B: Smart
int find_driver_smart(SharedState *state, int is_vip) {
    int best_index = -1;
    double min_dist = 1000000.0;

    // 1. VIP 優先篩選層
    if (is_vip) {
        for (int i = 0; i < state->driver_count; i++) {
            Driver *d = &state->drivers[i];
            if (d->is_available && !d->is_refueling && d->fuel > 0) {
                if (d->rating >= VIP_RATING_THRESHOLD) { 
                    double dist = calculate_distance(25.0330, 121.5654, d->lat, d->lon);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_index = i;
                    }
                }
            }
        }
        if (best_index != -1) return best_index;
    }

    // 2. 降級到普通搜尋
    return find_driver_basic(state);
}