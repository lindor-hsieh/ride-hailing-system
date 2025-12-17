/* src/server/include/resource_service.h */
#ifndef RESOURCE_SERVICE_H
#define RESOURCE_SERVICE_H

#include <stdint.h>
#include <stddef.h>
#include "../../common/include/shared_data.h"

/**
 * 檢查並更新客戶端的請求頻率 (Rate Limiting)。
 * client_id 客戶 ID
 * return 1 = 阻擋 (Blocked), 0 = 通行 (Allowed)
 */
int check_and_update_rate_limit(int client_id);

/**
 * 檢查司機是否可以接單 (Fuel / Refueling 檢查)。
 * state 共享記憶體指標
 * driver_index 司機在陣列中的索引
 * return 1 = 可以接單, 0 = 不能接單
 */
int check_if_driver_available(SharedState *state, int driver_index);

/**
 * 完成行程後，更新資源 (扣油、加業績、加營收)。
 * state 共享記憶體指標
 * driver_index 司機在陣列中的索引
 * fare 本次行程的價格
 */
void complete_ride_and_update_resources(SharedState *state, int driver_index, int fare);

#endif // RESOURCE_SERVICE_H