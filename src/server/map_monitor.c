/* src/server/map_monitor.c - Fast Cycle & Strict Fuel Version */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "../../common/include/shared_data.h"
#include "../include/map_monitor.h"
#include "../include/pathfinding.h" 

extern SharedState *g_shared_state;
extern volatile sig_atomic_t g_running; 

#define MAP_WIDTH 40
#define MAP_HEIGHT 20
#define BASE_LAT 25.0330
#define BASE_LON 121.5654
#define SCALE_FACTOR 2000 

void *map_monitor_thread(void *arg) {
    (void)arg;
    char map[MAP_HEIGHT][MAP_WIDTH];
    srand(time(NULL) + getpid());
    setvbuf(stdout, NULL, _IONBF, 0);

    init_map_obstacles();

    while (g_running) {
        if (g_shared_state) {
            pthread_mutex_lock(&g_shared_state->mutex);
            
            for (int i = 0; i < g_shared_state->driver_count; i++) {
                Driver *d = &g_shared_state->drivers[i];

                int gy = (int)((d->lat - BASE_LAT) * SCALE_FACTOR);
                int gx = (int)((d->lon - BASE_LON) * SCALE_FACTOR);
                
                // 1. 防卡牆 (重生機制)
                if (is_obstacle(gx, gy) || gx < 0 || gx >= MAP_WIDTH || gy < 0 || gy >= MAP_HEIGHT) {
                    d->lat = BASE_LAT; d->lon = BASE_LON;
                    d->has_target = 0; d->is_available = 1; d->is_refueling = 0;
                }

                // 2. 導航與抵達邏輯 (大幅加速行程完成)
                if (d->has_target) {
                    double dist = sqrt(pow(d->lat - d->target_lat, 2) + pow(d->lon - d->target_lon, 2));
                    
                    // 1: 放寬判定距離 (0.01 約等於 20 格寬，只要開到附近就算送達)
                    int arrived = (dist < 0.01);

                    // 2: 大幅提高「行程結束」機率 (2% -> 20%)
                    // 這模擬了短程載客，讓車子能更快變回 Available 接下一單
                    if (!arrived && (rand() % 100) < 20) { 
                        arrived = 1; 
                    }

                    if (arrived) { 
                        d->has_target = 0; 
                        d->is_available = 1; // 關鍵：行程結束，立刻變空車 (Green D)
                        d->is_refueling = 0;
                        d->lat = d->target_lat; // 瞬移到目的地
                        d->lon = d->target_lon;
                    }

                    // 耗油模擬 (10% 機率扣油)
                    if (d->fuel > 0 && (rand() % 100) < 10) {
                        d->fuel--;
                    }
                }

                // 3. 油量管理 (嚴格執行：真的沒油才去加)
                // 修正 3: 只有在 (Available 且 Fuel <= 0) 時才去加油
                if (d->is_available && d->fuel <= 0 && !d->is_refueling) {
                    d->is_refueling = 1; 
                    d->is_available = 0; 
                    d->has_target = 0;
                }
                
                if (d->is_refueling) {
                    if (d->fuel < 10) d->fuel += 2; // 加油速度
                    else { 
                        d->is_refueling = 0; 
                        d->is_available = 1; // 加滿了，變回空車
                    }
                }

                // 4. 殭屍車清除 (Failsafe)
                // 確保不會有車子卡在 Busy 狀態但沒目標
                if (!d->is_available && !d->is_refueling && !d->has_target) {
                    d->is_available = 1;
                }

                // 5. 移動核心
                if (d->has_target && !d->is_refueling) {
                    Point next = get_next_step_astar(d->lat, d->lon, d->target_lat, d->target_lon);
                    
                    // 原地踏步偵測 (Stuck) -> 直接算抵達
                    if (next.x == gx && next.y == gy) {
                        d->has_target = 0; d->is_available = 1; 
                    } else {
                        d->lat = BASE_LAT + (double)next.y / SCALE_FACTOR;
                        d->lon = BASE_LON + (double)next.x / SCALE_FACTOR;
                    }
                } 
                else if (d->is_available || d->is_refueling) {
                    // 隨機漫步
                    double old_lat = d->lat; double old_lon = d->lon;
                    d->lat += ((rand() % 3) - 1) * 0.0005;
                    d->lon += ((rand() % 3) - 1) * 0.0005;
                    
                    int new_gy = (int)((d->lat - BASE_LAT) * SCALE_FACTOR);
                    int new_gx = (int)((d->lon - BASE_LON) * SCALE_FACTOR);
                    if (is_obstacle(new_gx, new_gy)) {
                        d->lat = old_lat; d->lon = old_lon;
                    }
                }
            }
            pthread_mutex_unlock(&g_shared_state->mutex);

            // --- 繪圖邏輯 ---
            for (int y = 0; y < MAP_HEIGHT; y++) {
                for (int x = 0; x < MAP_WIDTH; x++) {
                    if (is_obstacle(x, y)) map[y][x] = '#'; 
                    else map[y][x] = '.';
                }
            }

            for (int i = 0; i < g_shared_state->driver_count; i++) {
                Driver *d = &g_shared_state->drivers[i];
                int y = (int)((d->lat - BASE_LAT) * SCALE_FACTOR); 
                int x = (int)((d->lon - BASE_LON) * SCALE_FACTOR);

                if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                    if (d->has_target) map[y][x] = '>';     
                    else if (d->is_refueling) map[y][x] = 'F'; 
                    else if (d->is_available) map[y][x] = 'D'; 
                    else map[y][x] = 'X';                      
                }
            }

            printf("\033[2J\033[H"); 
            printf("==== [ SMART CITY MAP: FAST CYCLE VERSION ] ====\n");
            printf(" Mode: %-5s | Deals: %-4lu | Revenue: $%-5ld\n", 
                   g_shared_state->dispatch_mode == 1 ? "SMART" : "BASIC",
                   g_shared_state->total_success_requests,
                   g_shared_state->total_revenue);
            printf("----------------------------------------------\n");
            
            for (int y = MAP_HEIGHT - 1; y >= 0; y--) { 
                printf("  ");
                for (int x = 0; x < MAP_WIDTH; x++) {
                    char c = map[y][x];
                    if (c == '#') printf("\033[1;30m█\033[0m"); 
                    else if (c == '>') printf("\033[1;33m%c\033[0m", c); 
                    else if (c == 'D') printf("\033[1;32m%c\033[0m", c);      
                    else if (c == 'X') printf("\033[1;31m%c\033[0m", c); 
                    else if (c == 'F') printf("\033[1;34m%c\033[0m", c); 
                    else printf("\033[1;30m%c\033[0m", c); 
                }
                // Legend
                if (y == MAP_HEIGHT - 1) printf("  [LEGEND]");
                if (y == MAP_HEIGHT - 2) printf("  \033[1;33m>\033[0m: Moving");
                if (y == MAP_HEIGHT - 3) printf("  \033[1;32mD\033[0m: Available");
                if (y == MAP_HEIGHT - 4) printf("  \033[1;31mX\033[0m: Busy");
                if (y == MAP_HEIGHT - 5) printf("  \033[1;34mF\033[0m: Refueling (Fuel=0)");
                printf("\n");
            }

            printf("-------------------------------------------------------------------\n");
            printf(" ID    | Status      | Fuel (0-10) | Rating | Target\n");
            printf("-------|-------------|-------------|--------|----------------------\n");
            
            for (int i = 0; i < g_shared_state->driver_count; i++) {
                if (i >= 10) { printf(" ... (%d more drivers)\n", g_shared_state->driver_count - 10); break; }
                
                Driver *d = &g_shared_state->drivers[i];
                char status_str[30];
                char fuel_bar[12] = {0};
                char target_info[30] = " - ";

                if (d->has_target) {
                    sprintf(status_str, "\033[1;33mNavigating\033[0m");
                    sprintf(target_info, "(%.3f, %.3f)", d->target_lat, d->target_lon);
                }
                else if (d->is_refueling) sprintf(status_str, "\033[1;34mRefueling \033[0m");
                else if (d->is_available) sprintf(status_str, "\033[1;32mAvailable \033[0m");
                else sprintf(status_str, "\033[1;31mBusy      \033[0m");

                for(int k=0; k<10; k++) fuel_bar[k] = (k < d->fuel) ? '#' : '.';
                
                printf(" %-5d | %-19s | [%s] |  %.1f   | %s\n", 
                       d->driver_id, status_str, fuel_bar, d->rating, target_info);
            }
            printf("===================================================================\n");
            fflush(stdout); 
        }
        usleep(200000); 
    }
    return NULL;
}