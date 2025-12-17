/* src/server/include/pathfinding.h */
#ifndef PATHFINDING_H
#define PATHFINDING_H

// 定義地圖網格大小 (需與 map_monitor 一致)
#define MAP_WIDTH 40
#define MAP_HEIGHT 20

// 座標系統轉換參數
#define BASE_LAT 25.0330
#define BASE_LON 121.5654
#define SCALE_FACTOR 2000 

typedef struct {
    int x;
    int y;
} Point;

// 初始化地圖障礙物
void init_map_obstacles();

// 檢查某點是否為障礙物 (1=是, 0=否)
int is_obstacle(int x, int y);

// 核心函式：給定起點與終點，回傳「下一步」該走哪一格
// 如果無法到達或已到達，回傳起點本身
Point get_next_step_astar(double start_lat, double start_lon, double target_lat, double target_lon);

#endif