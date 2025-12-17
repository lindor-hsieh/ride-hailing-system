#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <pthread.h>
#include <stdint.h>
#include <time.h> 

#define MAX_DRIVERS 256
#define MAX_PENDING_RIDES 128

// 司機狀態
typedef struct {
    uint32_t driver_id;
    double lat;
    double lon;
    uint8_t is_available; 
    int rides_count;
    int fuel; 
    uint8_t is_refueling;

    // 司機評分 (1.0 - 5.0)
    double rating; 

    // A* 導航目標系統
    // 讓 map_monitor 知道司機要往哪裡走
    uint8_t has_target;    // 1 = 正在前往某處 (需要繞過障礙物), 0 = 無目標 (隨機漫步)
    double target_lat;     // 目標緯度
    double target_lon;     // 目標經度

} Driver;

// 訂單/行程狀態
typedef struct {
    uint32_t ride_id;
    uint32_t client_id;
    double start_lat;
    double start_lon;
    uint8_t status;       
} Ride;

// 主共享記憶體結構
typedef struct {
    // 1. Process-Shared Mutex (互斥鎖)
    // 用於保護這塊記憶體，防止多個 Dispatcher 同時修改導致 Race Condition
    pthread_mutex_t mutex

    // 2. 司機狀態陣列
    // 儲存所有司機的位置 (lat, lon)、狀態 (Available/Navigating)、評分與油量
    Driver drivers[MAX_DRIVERS];
    int driver_count;

    // 3. 訂單佇列 (結構保留)
    Ride pending_rides[MAX_PENDING_RIDES];
    int ride_count;

    // 所有 Dispatcher 處理完訂單後，都會更新這裡的數字
    uint64_t total_requests_handled;
    uint64_t total_success_requests;
    long total_revenue; // 總營收 (用於計算 Surge Pricing 門檻)

    // 5. 資安防護資料 (Security / DoS Protection)
    // 記錄每個 Client IP 最後連線時間與請求次數，用於 Rate Limiting
    time_t client_last_seen[2000]; 
    int client_req_count[2000];    

    // 派車演算法模式 (0=Basic, 1=Smart)
    int dispatch_mode;

} SharedState;

extern SharedState *g_shared_state;

#endif