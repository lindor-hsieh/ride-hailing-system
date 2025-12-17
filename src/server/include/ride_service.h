/* src/server/include/ride_service.h */
#ifndef RIDE_SERVICE_H
#define RIDE_SERVICE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * 處理叫車請求的核心業務邏輯 (協調者)。
 * 由 dispatcher.c 呼叫。
 * client_id 客戶 ID
 * response_msg 回覆訊息緩衝區
 * msg_len 緩衝區長度
 * return 0 = 成功, -1 = 失敗 (無車)
 */
int handle_ride_request_logic(int client_id, char *response_msg, size_t msg_len);

#endif // RIDE_SERVICE_H