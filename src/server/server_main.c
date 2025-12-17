/* src/server/server_main.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "../../common/include/shared_data.h"
#include "../../common/include/log_system.h"
#include "../../common/include/net_wrapper.h"

// 引用 Coordinator 模組
#include "coordinator.h"

// 定義共享記憶體名稱
#define SHM_NAME "/ride_hailing_shm"

// 全域變數
SharedState *g_shared_state = NULL;
int g_shm_fd = -1;
volatile sig_atomic_t g_running = 1;

void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n[INFO] SIGINT received. Shutting down...\n");
        g_running = 0;
    }
}

// 寫入數據到檔案
void save_data_to_file() {
    FILE *fp = fopen("server.dat", "wb");
    if (fp) {
        fwrite(g_shared_state, sizeof(SharedState), 1, fp);
        fclose(fp);
        log_info("Data saved to server.dat");
    } else {
        log_error("Failed to save data to server.dat");
    }
}

// 從檔案載入數據
void load_data_from_file() {
    FILE *fp = fopen("server.dat", "rb");
    if (fp) {
        fread(g_shared_state, sizeof(SharedState), 1, fp);
        fclose(fp);
        log_info("Data loaded from server.dat");
    } else {
        log_info("No existing data found. Starting fresh.");
    }
}

void cleanup_resources() {
    save_data_to_file();
    if (g_shared_state != MAP_FAILED) {
        pthread_mutex_destroy(&g_shared_state->mutex);
        munmap(g_shared_state, sizeof(SharedState));
    }
    if (g_shm_fd != -1) {
        close(g_shm_fd);
        shm_unlink(SHM_NAME);
    }
    log_info("Resources cleaned up.");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <driver_count> [mode: 0=Basic, 1=Smart]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int driver_count = atoi(argv[2]);
    int mode = (argc >= 4) ? atoi(argv[3]) : 1; 

    // 初始化 Log 系統
    log_init("server.log");
    log_info("Server starting on port %d with %d drivers...", port, driver_count);
    log_info("Dispatch Logic Mode: %s", mode == 1 ? "SMART (VIP Priority)" : "BASIC (Distance Only)");

    signal(SIGINT, handle_signal);

    // 1. 設定 Shared Memory
    // 建立共享記憶體物件
    g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (g_shm_fd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }
    // 設定大小
    if (ftruncate(g_shm_fd, sizeof(SharedState)) == -1) {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }
    // 記憶體映射 (Map) 到行程空間
    g_shared_state = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shared_state == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // 1：調整初始化順序 (先 memset 再 mutex_init)
    
    // 強制設定 fp 為 NULL，永遠不讀取舊檔，確保每次都是乾淨啟動
    FILE *fp = NULL; 
    
    if (fp) {
        // 如果要讀檔，先讀進來
        load_data_from_file();
        // 讀檔後，因為 Mutex 狀態可能不對，建議還是要重新初始化 Mutex
    } else {
        log_info("Starting fresh (Ignoring old save file)...");
        
        // 1. 先清空記憶體！(這一步必須在 mutex_init 之前)
        memset(g_shared_state, 0, sizeof(SharedState));
        
        g_shared_state->driver_count = driver_count;
        g_shared_state->dispatch_mode = mode;

        // 初始化司機
        for (int i = 0; i < driver_count; i++) {
            g_shared_state->drivers[i].driver_id = 1000 + i + 1;
            g_shared_state->drivers[i].is_available = 1;
            
            // 重置回基地座標
            g_shared_state->drivers[i].lat = 25.0330 + (rand() % 100) * 0.0001; 
            g_shared_state->drivers[i].lon = 121.5654 + (rand() % 100) * 0.0001;
            
            g_shared_state->drivers[i].fuel = 10; 
            g_shared_state->drivers[i].is_refueling = 0;

            // 2：明確初始化新變數，防止 A* 演算法讀到垃圾值
            g_shared_state->drivers[i].has_target = 0;
            g_shared_state->drivers[i].target_lat = 0.0;
            g_shared_state->drivers[i].target_lon = 0.0;

            // 設定評分
            if (i < 2) {
                g_shared_state->drivers[i].rating = 4.9 + (rand() % 2) / 10.0; 
            } else {
                g_shared_state->drivers[i].rating = 3.5 + (rand() % 15) / 10.0; 
            }
            
            log_info("Driver %d inited. Rating: %.1f", g_shared_state->drivers[i].driver_id, g_shared_state->drivers[i].rating);
        }
    }

    // 2. 現在才初始化互斥鎖 (確保不會被 memset 清掉)
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    
    // 無論是讀檔還是全新，都重新初始化鎖，確保當前 Process 可用
    pthread_mutex_init(&g_shared_state->mutex, &attr);

    // 3. 建立 Server Socket
    int server_fd = create_server_socket(port);
    if (server_fd < 0) {
        cleanup_resources();
        exit(EXIT_FAILURE);
    }

    // 4. 啟動 Coordinator
    start_coordinator_process(server_fd);

    // 5. 等待結束
    cleanup_resources();
    close(server_fd);
    log_info("Server stopped.");

    return 0;
}