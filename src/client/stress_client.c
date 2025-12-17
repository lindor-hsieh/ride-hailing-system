/* src/client/stress_client.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h> 

#include "../../common/include/protocol.h"
#include "../../common/include/net_wrapper.h"
#include "../../common/include/log_system.h"

#define SECRET_KEY "RIDE_HAILING_2025_SECURE_KEY"

// 宣告在 client_core.c 中定義的核心函式 (外部引用)
extern int perform_ride_request(int sock_fd, int client_id, char *msg_buffer);
extern double get_time_ms();
extern void get_time_str(char *buffer, size_t size);


// 狀態碼定義
typedef enum {
    STATUS_INIT,
    STATUS_SUCCESS,
    STATUS_FAIL_NO_DRIVER,
    STATUS_FAIL_DOS,
    STATUS_FAIL_OTHER
} ClientResultStatus;

// 每個 Client 的最終結果追蹤結構
typedef struct {
    int client_id;
    ClientResultStatus final_status;
} ClientStatusEntry;


// 統計結構

typedef struct {
    pthread_mutex_t lock;
    long total_requests;
    long success_count;
    long fail_count;
    double total_latency_ms;
} ClientStats;

typedef struct {
    int client_id;
    char *server_ip;
    int server_port;
    ClientStats *stats;
    ClientStatusEntry *status_list; // 指向 Client 狀態陣列的指針
    int requests_per_thread; 
} ThreadArgs;


// 輔助函式：輸出狀態表格

void print_client_status_table(ClientStatusEntry *status_list, int count) {
    printf("\n\n=== Client Request Summary Table (%d Clients) ===\n", count);
    printf("+----------+------------------------+\n");
    printf("| Client ID| Final Status           |\n");
    printf("+----------+------------------------+\n");

    for (int i = 0; i < count; i++) {
        char *status_str;
        switch (status_list[i].final_status) {
            case STATUS_SUCCESS:
                status_str = "✅ SUCCESS";
                break;
            case STATUS_FAIL_DOS:
                status_str = "⚠️ FAILED (DoS Blocked)";
                break;
            case STATUS_FAIL_NO_DRIVER:
                status_str = "❌ FAILED (No Driver/Conn)";
                break;
            default:
                status_str = "❓ INCOMPLETE";
                break;
        }
        printf("| %-8d | %-22s |\n", status_list[i].client_id, status_str);
    }
    printf("+----------+------------------------+\n");
}


// 壓力測試專用區

void *stress_client_thread_func(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int sock_fd; 
    char time_buf[32];
    char msg_buffer[1024]; 
    int success_local_count = 0; // 追蹤該執行緒是否有成功
    
    // 使用 rand() 初始化
    srand(time(NULL) ^ args->client_id); 

    for (int i = 0; i < args->requests_per_thread; i++) {
        sock_fd = connect_to_server(args->server_ip, args->server_port);
        if (sock_fd < 0) {
            // ... (連線失敗邏輯不變) ...
            usleep(100 * 1000); 
            continue; 
        }

        // 設定 Timeout
        struct timeval tv = {2, 0};
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        double start = get_time_ms(); 
        
        int result = perform_ride_request(sock_fd, args->client_id, msg_buffer); 
        
        close(sock_fd); 

        double end = get_time_ms(); 

        pthread_mutex_lock(&args->stats->lock);
        args->stats->total_requests++;
        if (result == 0) {
            args->stats->success_count++;
            args->stats->total_latency_ms += (end - start);
            success_local_count++; // 該執行緒成功計數
        } else {
            args->stats->fail_count++;
        }
        pthread_mutex_unlock(&args->stats->lock);

        if (result == 0) {
            // Log 輸出 (前 30 人)
            if (args->client_id <= 30) {
                get_time_str(time_buf, sizeof(time_buf)); 
                printf("[%s] Client %3d: %s\n", time_buf, args->client_id, msg_buffer);
            }
            usleep(200 * 1000); 
        } else if (result == -2) {
            // DoS 阻擋 Log (前 30 人)
            if (args->client_id <= 30) {
                 printf("\033[1;33m[Client %d] Blocked! Retrying.\033[0m\n", args->client_id); 
            }
            // 更新最終狀態為 DoS 阻擋 (如果這是第一個失敗)
            if (args->status_list[args->client_id - 1].final_status == STATUS_INIT) {
                args->status_list[args->client_id - 1].final_status = STATUS_FAIL_DOS;
            }
            usleep((rand() % 500 + 500) * 1000);
        } else {
            // 失敗重試 Log (前 30 人)
            if (args->client_id <= 30) {
                 printf("[Client %d] Request failed/no driver. Retrying.\n", args->client_id); 
            }
            // 更新最終狀態為 No Driver
            if (args->status_list[args->client_id - 1].final_status == STATUS_INIT) {
                args->status_list[args->client_id - 1].final_status = STATUS_FAIL_NO_DRIVER;
            }
            usleep((rand() % 300 + 200) * 1000); 
        }
    }
    
    // 執行緒結束時，設定最終狀態
    pthread_mutex_lock(&args->stats->lock); 
    if (success_local_count > 0) { 
        args->status_list[args->client_id - 1].final_status = STATUS_SUCCESS;
    } else if (args->status_list[args->client_id - 1].final_status == STATUS_INIT) {
        // 如果狀態仍然是 INIT，表示所有請求都失敗了，最終設為 No Driver
        args->status_list[args->client_id - 1].final_status = STATUS_FAIL_NO_DRIVER;
    }
    pthread_mutex_unlock(&args->stats->lock);

    free(args);
    return NULL;
}


int main(int argc, char *argv[]) {
    // 忽略 SIGPIPE 訊號
    signal(SIGPIPE, SIG_IGN); 

    if (argc < 4) {
        printf("Usage: %s <Server IP> <Port> <Num Clients>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int num_clients = atoi(argv[3]); 
    int requests_per_client = 10; // 每個客戶嘗試 10 次 (可調)

    if (num_clients <= 0) num_clients = 1;
    if (num_clients > 5000) num_clients = 5000;

    log_init(NULL); // 初始化 log 系統
    log_info("Starting stress test: %d clients, %d requests each...", num_clients, requests_per_client);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_clients);
    ClientStats stats = {PTHREAD_MUTEX_INITIALIZER, 0, 0, 0, 0.0};
    
    // 創建 Client 狀態追蹤陣列
    ClientStatusEntry *status_list = malloc(sizeof(ClientStatusEntry) * num_clients);
    if (!status_list) {
        perror("malloc status_list failed");
        return 1;
    }
    
    double start_time = get_time_ms(); 

    for (int i = 0; i < num_clients; i++) {
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->client_id = i + 1;
        args->server_ip = server_ip;
        args->server_port = server_port;
        args->stats = &stats;
        args->requests_per_thread = requests_per_client;
        
        // 初始化 Client 狀態並傳遞指針
        status_list[i].client_id = i + 1;
        status_list[i].final_status = STATUS_INIT;
        args->status_list = status_list;

        pthread_create(&threads[i], NULL, stress_client_thread_func, args);
    }

    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }

    double end_time = get_time_ms(); 
    
    // 輸出詳細狀態表格
    print_client_status_table(status_list, num_clients);

    printf("\n=== Stress Test Summary ===\n");
    printf("Total Duration   : %.2f ms\n", end_time - start_time);
    printf("Total Requests   : %ld\n", stats.total_requests);
    printf("Successful Rides : %ld\n", stats.success_count);
    printf("Failed Requests  : %ld\n", stats.fail_count);
    if (stats.success_count > 0) {
        printf("Avg Latency      : %.2f ms\n", stats.total_latency_ms / stats.success_count);
    }
    printf("===========================\n");

    free(status_list); // 釋放狀態陣列
    free(threads);
    log_cleanup();
    return 0;
}