/* src/server/insecure_server.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h> 

// 引入共用模組
#include "../../common/include/net_wrapper.h"
#include "../../common/include/log_system.h"
#include "../../common/include/shared_data.h"

// 宣告 coordinator.c 中的函式
extern void ipc_init(int driver_count);
extern void ipc_cleanup();
extern void start_coordinator_process_insecure(int server_fd); 

// 在這裡「定義」變數 (移除 extern)，給予實體空間
SharedState *g_shared_state = NULL; 
volatile sig_atomic_t g_running = 1; 

/**
 * 伺服器程式的總入口點 (Insecure Version)。
 */
int main(int argc, char *argv[]) {
    // 1. 檢查參數
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <driver_count>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int driver_count = atoi(argv[2]);

    // 2. 初始化日誌系統
    log_init(NULL); 
    log_warn("🚨 INSECURE SERVER STARTING (NO ENCRYPTION/CHECKSUM/RATE LIMIT) 🚨"); 
    log_info("Server starting on port %d with %d drivers...", port, driver_count);

    // 3. 建立監聽 Socket
    int server_fd = create_server_socket(port);
    if (server_fd < 0) {
        log_error("Failed to create server socket. Exiting.");
        exit(EXIT_FAILURE);
    }
    
    // 4. 初始化 IPC (共享記憶體與鎖)
    // 這裡會呼叫 coordinator.c 裡的 ipc_init，它會使用我們上面定義的 g_shared_state
    ipc_init(driver_count);

    // 5. 啟動 Coordinator-Dispatcher 機制
    start_coordinator_process_insecure(server_fd); 

    // 6. 清理
    ipc_cleanup(); 
    close(server_fd);
    log_warn("Insecure Server shutdown completed.");

    return 0;
}