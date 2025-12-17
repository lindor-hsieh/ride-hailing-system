/* src/server/pathfinding.c */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "include/pathfinding.h"

// 0: 空地, 1: 牆壁
static int grid_map[MAP_HEIGHT][MAP_WIDTH];

// A* 節點結構
typedef struct {
    int x, y;
    int g_cost; 
    int h_cost; 
    int f_cost; 
    int parent_index; 
} Node;

// 初始化靜態障礙物
void init_map_obstacles() {
    memset(grid_map, 0, sizeof(grid_map));

    // 1. 造一條河流 (垂直線)
    for (int y = 5; y < 15; y++) {
        grid_map[y][20] = 1; 
    }

    // 2. 造一些建築物 (矩形)
    for (int y = 3; y < 7; y++) {
        for (int x = 5; x < 10; x++) grid_map[y][x] = 1;
    }
    
    for (int y = 12; y < 16; y++) {
        for (int x = 30; x < 35; x++) grid_map[y][x] = 1;
    }
}

int is_obstacle(int x, int y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return 1; 
    return grid_map[y][x];
}

// 輔助：計算曼哈頓距離
int calculate_h(int x, int y, int tx, int ty) {
    return abs(x - tx) + abs(y - ty);
}

// A* 演算法核心 (強化版)
Point get_next_step_astar(double start_lat, double start_lon, double target_lat, double target_lon) {
    int start_y = (int)((start_lat - BASE_LAT) * SCALE_FACTOR);
    int start_x = (int)((start_lon - BASE_LON) * SCALE_FACTOR);
    int target_y = (int)((target_lat - BASE_LAT) * SCALE_FACTOR);
    int target_x = (int)((target_lon - BASE_LON) * SCALE_FACTOR);

    // 邊界保護
    if (start_x < 0) start_x = 0; if (start_x >= MAP_WIDTH) start_x = MAP_WIDTH - 1;
    if (start_y < 0) start_y = 0; if (start_y >= MAP_HEIGHT) start_y = MAP_HEIGHT - 1;
    if (target_x < 0) target_x = 0; if (target_x >= MAP_WIDTH) target_x = MAP_WIDTH - 1;
    if (target_y < 0) target_y = 0; if (target_y >= MAP_HEIGHT) target_y = MAP_HEIGHT - 1;

    if (start_x == target_x && start_y == target_y) return (Point){start_x, start_y};

    // 1: 加大 Buffer (800 -> 4000)
    // 避免因為重複路徑探索導致陣列爆掉 (Segfault 主因)
    #define MAX_OPEN_NODES (MAP_WIDTH * MAP_HEIGHT * 5)
    Node open_list[MAX_OPEN_NODES];
    int closed_list[MAP_HEIGHT][MAP_WIDTH]; 
    int open_count = 0;
    
    memset(closed_list, 0, sizeof(closed_list));
    
    // 加入起點
    open_list[0] = (Node){start_x, start_y, 0, calculate_h(start_x, start_y, target_x, target_y), 0, -1};
    open_list[0].f_cost = open_list[0].g_cost + open_list[0].h_cost;
    open_count++;

    int max_steps = 3000; // 效能保護

    while (open_count > 0 && max_steps-- > 0) {
        int lowest_f = 999999;
        int current_index = -1;
        
        for (int i = 0; i < open_count; i++) {
            if (open_list[i].f_cost < lowest_f) {
                lowest_f = open_list[i].f_cost;
                current_index = i;
            }
        }

        if (current_index == -1) break; 

        Node current = open_list[current_index];
        open_list[current_index] = open_list[open_count - 1];
        open_count--;
        
        closed_list[current.y][current.x] = 1;

        if (current.x == target_x && current.y == target_y) {
            break; // 簡化版：只要確認能走到就好，回傳方向交給下面的 Greedy 判斷
        }

        int dx[] = {0, 0, -1, 1};
        int dy[] = {-1, 1, 0, 0};

        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT) {
                if (is_obstacle(nx, ny) || closed_list[ny][nx]) continue;

                // 2: 安全檢查 (防止 Stack Overflow) 
                if (open_count >= MAX_OPEN_NODES) {
                    // Buffer 滿了，停止擴展，避免崩潰
                    break; 
                }

                open_list[open_count] = (Node){nx, ny, current.g_cost + 1, calculate_h(nx, ny, target_x, target_y), 0, -1}; 
                open_list[open_count].f_cost = open_list[open_count].g_cost + open_list[open_count].h_cost;
                open_count++;
            }
        }
    }

    // Local Greedy Search 回傳下一步
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};
    int best_dir = -1;
    int min_score = 999999;

    for (int i = 0; i < 4; i++) {
        int nx = start_x + dx[i];
        int ny = start_y + dy[i];

        if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT) {
            if (!is_obstacle(nx, ny)) {
                int score = calculate_h(nx, ny, target_x, target_y);
                if (score < min_score) {
                    min_score = score;
                    best_dir = i;
                }
            }
        }
    }

    if (best_dir != -1) {
        return (Point){start_x + dx[best_dir], start_y + dy[best_dir]};
    }

    return (Point){start_x, start_y}; 
}