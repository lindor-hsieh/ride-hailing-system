/* src/server/resource_service.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

#include "resource_service.h"
#include "../../common/include/shared_data.h"
#include "../../common/include/log_system.h"

// 引用外部的全域變數
extern SharedState *g_shared_state;

//  A. 司機資源管理 (Fuel & 記帳)
/**
 * 檢查司機是否可以接單 (Fuel / Refueling 檢查)。
 * 這是 Ride Service 進行搶車判斷時調用的函式。
 * state 共享記憶體指標
 * driver_index 司機在陣列中的索引
 * return 1 = 可以接單 (Available), 0 = 不能接單 (No Fuel/Refueling)
 */
int check_if_driver_available(SharedState *state, int driver_index) {
    if (state->drivers[driver_index].is_refueling) {
        return 0; // 正在加油中
    }
    if (state->drivers[driver_index].fuel <= 0) {
        return 0; // 沒油了
    }
    return 1; // 可以接單
}

/**
 * 完成行程後，更新資源 (扣油、加業績、加營收)。
 * 這是 Ride Service 完成載客後調用的函式。
 * state 共享記憶體指標
 * driver_index 司機在陣列中的索引
 * fare 本次行程的價格
 */
void complete_ride_and_update_resources(SharedState *state, int driver_index, int fare) {
    state->drivers[driver_index].is_available = 1; // 釋放回空閒
    state->drivers[driver_index].rides_count++;    // 業績 +1
    state->total_revenue += fare;                 // 總營收累積

    // 扣油量 (Fuel System)
    if (state->drivers[driver_index].fuel > 0) {
        state->drivers[driver_index].fuel -= 1; 
    }
}

//  B. DoS 頻率限制邏輯 (Availability Security)
/**
 * 檢查並更新客戶端的請求頻率 (Rate Limiting)。
 * 這是 Dispatcher.c 在接受 Payload 後第一個調用的安全函式。
 * client_id 客戶 ID
 * return 1 = 阻擋 (Blocked), 0 = 通行 (Allowed)
 */
int check_and_update_rate_limit(int client_id) {
    if (client_id <= 0 || client_id >= 2000 || g_shared_state == NULL) {
        return 0; // 不處理無效 ID
    }

    int is_spam = 0;
    time_t now = time(NULL);
    
    // 必須鎖定，因為多個 Dispatcher 進程會同時寫入這個陣列
    pthread_mutex_lock(&g_shared_state->mutex);
    
    if (g_shared_state->client_last_seen[client_id] == now) {
        // 同一秒內，增加計數
        g_shared_state->client_req_count[client_id]++;
    } else {
        // 新的一秒，重置時間和計數
        g_shared_state->client_last_seen[client_id] = now;
        g_shared_state->client_req_count[client_id] = 1;
    }

    // 閾值：每秒超過 3 次即視為 DoS 攻擊
    if (g_shared_state->client_req_count[client_id] > 3) {
        is_spam = 1; 
    }
    
    pthread_mutex_unlock(&g_shared_state->mutex);
    
    return is_spam;
}