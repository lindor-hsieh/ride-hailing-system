/* src/server/include/pricing_service.h */
#ifndef PRICING_SERVICE_H
#define PRICING_SERVICE_H

#include <stdint.h>
#include "../../common/include/shared_data.h"

/**
 * 計算動態定價和是否啟動溢價。
 * state 共享記憶體指標 (讀取忙碌狀態)
 * is_surge 輸出參數：1 表示啟動溢價，0 表示沒有
 * return 最終價格
 */
int calculate_surge_price(SharedState *state, int *is_surge);

#endif // PRICING_SERVICE_H