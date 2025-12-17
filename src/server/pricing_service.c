/* src/server/pricing_service.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pricing_service.h"
#include "../../common/include/log_system.h"

// 實作動態定價邏輯
/**
 * 計算當前的動態定價 (Surge Price)。
 * state 共享記憶體指標
 * is_surge 輸出參數：1 表示啟動溢價，0 表示沒有
 * return 最終價格 (例如：100 或 200)
 */
int calculate_surge_price(SharedState *state, int *is_surge) {
    *is_surge = 0; // 預設沒有溢價
    int base_fare = 100;
    
    // 檢查是否有司機
    if (state->driver_count == 0) {
        return base_fare; 
    }

    // 1. 計算忙碌司機數量
    int busy_drivers = 0;
    for (int i = 0; i < state->driver_count; i++) {
        // 忙碌的定義：不空閒 (is_available == 0) 且不在加油中 (is_refueling == 0)
        // 這裡只需要計算載客中的車，因為加油中的車不影響「供需緊張」的定義
        if (!state->drivers[i].is_available && !state->drivers[i].is_refueling) {
            busy_drivers++;
        }
    }

    // 2. 計算忙碌比例
    double total_drivers = (double)state->driver_count;
    double busy_ratio = busy_drivers / total_drivers;
    
    int final_fare = base_fare;

    // 3. 判斷是否啟動溢價 (閾值：70%)
    if (busy_ratio > 0.7) {
        final_fare = base_fare * 2; // 價格翻倍
        *is_surge = 1;
        // log_debug("Surge Price Activated! Busy Ratio: %.2f", busy_ratio);
    } 

    return final_fare;
}