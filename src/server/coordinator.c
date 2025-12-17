/* src/server/coordinator.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>      
#include <sys/stat.h>   
#include <string.h>
#include <time.h> 
#include <stdint.h> 
#include "../../common/include/net_wrapper.h"
#include "../../common/include/protocol.h"

#include "../../common/include/shared_data.h"
#include "../../common/include/log_system.h"
#include "../include/map_monitor.h" 

#define DATA_FILE "server.dat"
#define WORKER_COUNT 100 
#define BASE_LAT 25.0330
#define BASE_LON 121.5654

// 1：加上 extern，避免與 server_main.c 衝突
extern SharedState *g_shared_state;
extern volatile sig_atomic_t g_running;
// 這是本檔案私有的，保持 static
static pid_t workers[WORKER_COUNT];
static int g_server_fd = -1;

// 外部函式宣告
extern void dispatcher_loop(int server_fd); 
extern void dispatcher_loop_insecure(int server_fd);

// 前向宣告
void save_state();
int load_state();
void ipc_cleanup();
void handle_sigint(int sig);

// A. 資料持久化
void save_state() {
    int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        log_error("Failed to save state to %s: %s", DATA_FILE, strerror(errno));
        return;
    }
    if (write(fd, g_shared_state, sizeof(SharedState)) != sizeof(SharedState)) {
        log_error("Error writing state to file: %s", strerror(errno));
    } else {
        log_info("✅ System state saved to %s", DATA_FILE);
    }
    close(fd);
}

int load_state() {
    int fd = open(DATA_FILE, O_RDONLY);
    if (fd == -1) return 0;
    log_info("🔄 Found save file. Loading state...");
    if (read(fd, g_shared_state, sizeof(SharedState)) != sizeof(SharedState)) {
        log_warn("Save file corrupted. Re-initializing.");
        close(fd);
        return 0;
    }
    close(fd);
    log_info("✅ State loaded!");
    return 1;
}

// B. IPC 初始化
void ipc_init(int driver_count) {
    g_shared_state = mmap(NULL, sizeof(SharedState), 
                          PROT_READ | PROT_WRITE, 
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (g_shared_state == MAP_FAILED) {
        log_error("mmap failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int loaded = load_state();

    if (!loaded) {
        log_info("Initializing fresh system state...");
        memset(g_shared_state, 0, sizeof(SharedState));
        
        if (driver_count > MAX_DRIVERS) driver_count = MAX_DRIVERS;
        g_shared_state->driver_count = driver_count;
        
        for (int i = 0; i < g_shared_state->driver_count; i++) {
            g_shared_state->drivers[i].driver_id = 1000 + i; 
            g_shared_state->drivers[i].is_available = 1;     
            g_shared_state->drivers[i].lat = BASE_LAT + (i * 0.001); 
            g_shared_state->drivers[i].lon = BASE_LON + (i * 0.001);
            g_shared_state->drivers[i].fuel = 10; 
        }
    } else {
        // 恢復狀態時重置不可持久化的欄位
        for (int i = 0; i < g_shared_state->driver_count; i++) {
            g_shared_state->drivers[i].is_available = 1; 
        }
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); 
    pthread_mutex_init(&g_shared_state->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    log_info("IPC initialized.");
}

void ipc_cleanup() {
    if (g_shared_state) {
        pthread_mutex_destroy(&g_shared_state->mutex);
        munmap(g_shared_state, sizeof(SharedState));
        g_shared_state = NULL;
    }
}

// C. Coordinator 流程
void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
    g_running = 0; 
    log_info("Received SIGINT. Shutting down...");
    for (int i = 0; i < WORKER_COUNT; i++) {
        if (workers[i] > 0) kill(workers[i], SIGTERM);
    }
    while (wait(NULL) > 0);
    if (g_shared_state != NULL) save_state();
    ipc_cleanup();
    exit(0);
}

void start_coordinator_process(int server_fd) {
    g_server_fd = server_fd;
    signal(SIGINT, handle_sigint);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            log_error("Fork failed"); exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child Process (Worker/Dispatcher)
            signal(SIGINT, SIG_DFL); 
            dispatcher_loop(server_fd); 
            exit(0);
        } else {
            // Parent Process (Master/Coordinator) 記錄 PID
            workers[i] = pid;
        }
    }
    log_info("%d Dispatcher processes started.", WORKER_COUNT);

    pthread_t map_tid;
    if (pthread_create(&map_tid, NULL, map_monitor_thread, NULL) == 0) {
        pthread_detach(map_tid); 
        log_info("Map Monitor thread started.");
    }

    while (g_running) {
        int status;
        if (wait(&status) <= 0 && (errno == ECHILD || !g_running)) break;
    }
}

void start_coordinator_process_insecure(int server_fd) {
    g_server_fd = server_fd;
    signal(SIGINT, handle_sigint);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            log_error("Fork failed"); exit(EXIT_FAILURE);
        } else if (pid == 0) {
            signal(SIGINT, SIG_DFL); 
            dispatcher_loop_insecure(server_fd); 
            exit(0);
        } else {
            workers[i] = pid;
        }
    }
    log_warn("%d INSECURE Dispatchers started.", WORKER_COUNT);

    pthread_t map_tid;
    if (pthread_create(&map_tid, NULL, map_monitor_thread, NULL) == 0) {
        pthread_detach(map_tid); 
    }

    while (g_running) {
        int status;
        if (wait(&status) <= 0 && (errno == ECHILD || !g_running)) break;
    }
}

void process_driver_join(int client_fd, ProtocolHeader *in_header, uint8_t *body) {
    (void)in_header;
    uint32_t driver_id = *((uint32_t*)body);

    pthread_mutex_lock(&g_shared_state->mutex);
    if (g_shared_state->driver_count < MAX_DRIVERS) {
        int idx = g_shared_state->driver_count++;
        g_shared_state->drivers[idx].driver_id = driver_id;
        g_shared_state->drivers[idx].is_available = 1; 
        g_shared_state->drivers[idx].fuel = 10;
    }
    pthread_mutex_unlock(&g_shared_state->mutex);

    // 2：初始化結構
    ProtocolHeader resp_header = {
        .length = 0,
        .type = MSG_TYPE_RIDE_RESP,  // 使用正確的 Message Type
        .opcode = OP_RESPONSE,       // 使用 Opcode
        .checksum = 0
    };
    send_n(client_fd, &resp_header, sizeof(ProtocolHeader));
}