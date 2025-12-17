#ifndef DISPATCH_ALGORITHMS_H
#define DISPATCH_ALGORITHMS_H

#include "../../common/include/shared_data.h"

// 輔助：計算距離
double calculate_distance(double lat1, double lon1, double lat2, double lon2);

// 演算法策略 A: 基礎搜尋 (最近優先)
int find_driver_basic(SharedState *state);

// 演算法策略 B: 智慧搜尋 (VIP 高分優先 + 最近)
int find_driver_smart(SharedState *state, int is_vip);

#endif