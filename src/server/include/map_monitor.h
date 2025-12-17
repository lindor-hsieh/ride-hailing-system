/* src/server/include/map_monitor.h */
#ifndef MAP_MONITOR_H
#define MAP_MONITOR_H

// 地圖執行緒的入口函式
// 必須由 coordinator.c 呼叫 pthread_create 啟動
void *map_monitor_thread(void *arg);

#endif // MAP_MONITOR_H