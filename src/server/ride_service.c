/* src/server/ride_service.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../common/include/shared_data.h"
#include "../../common/include/log_system.h"
#include "../include/ride_service.h"

// 引入演算法模組
#include "../include/dispatch_algorithms.h"

int handle_ride_request_logic(int client_id, char *resp_buffer, size_t buffer_len) {
    SharedState *state = g_shared_state;
    
    // 進入臨界區 (Critical Section)
    pthread_mutex_lock(&state->mutex);

    int is_vip = (client_id <= 10);
    int best_driver_index = -1;

    // 根據模式選擇派車演算法
    if (state->dispatch_mode == 0) {
        best_driver_index = find_driver_basic(state);
    } else {
        best_driver_index = find_driver_smart(state, is_vip);
    }

    // 處理匹配結果
    if (best_driver_index != -1) {
        Driver *d = &state->drivers[best_driver_index];
        
        // 1. 更新基本狀態
        d->is_available = 0;   // 設為忙碌
        d->rides_count++;
        d->fuel--;
        
        // 設定 A* 導航目標
        // 讓 map_monitor 知道這位司機有任務，需要啟動 A* 演算法
        d->has_target = 1;
        
        // 設定隨機目的地 (模擬乘客要去的終點)
        // 範圍控制在地圖可視範圍內 (Lat: +0~0.01, Lon: +0~0.02)
        // 這樣司機就會在地圖上開始繞過障礙物移動
        d->target_lat = 25.0330 + (rand() % 90) * 0.0001; 
        d->target_lon = 121.5654 + (rand() % 180) * 0.0001;

        // 2. 更新全域統計
        state->total_requests_handled++;
        state->total_success_requests++;
        
        // 計算顯示用的距離與車資
        double dist = calculate_distance(25.0330, 121.5654, d->lat, d->lon);
        double fare = 100.0 + (is_vip ? 50.0 : 0.0);
        state->total_revenue += (long)fare;

        pthread_mutex_unlock(&state->mutex);

        // 3. 準備回傳訊息
        snprintf(resp_buffer, buffer_len, 
            "Ride Confirmed! Driver ID: %d (Rating: %.1f, Dist: %.4f) [Mode: %s]", 
            d->driver_id, d->rating, dist, 
            state->dispatch_mode == 1 ? "SMART" : "BASIC");
            
        log_info("Dispatched Driver %d (Rate %.1f) to Client %d. Heading to (%.4f, %.4f)", 
                 d->driver_id, d->rating, client_id, d->target_lat, d->target_lon);
        return 0; // 成功
    } else {
        // 無車可用
        pthread_mutex_unlock(&state->mutex);
        snprintf(resp_buffer, buffer_len, "Error: No drivers available.");
        return -1; // 失敗
    }
}