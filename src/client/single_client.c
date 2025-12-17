/* src/client/single_client.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <signal.h>

// 引入共用模組
#include "../../common/include/net_wrapper.h"
#include "../../common/include/protocol.h"
#include "../../common/include/log_system.h"

// 宣告在 client_core.c 中定義的核心邏輯
// 這是所有 Client 程式（單人/壓力/惡意）共用的網路通信核心
extern int perform_ride_request(int sock_fd, int client_id, char *msg_buffer);

int main(int argc, char *argv[]) {
    // 忽略 SIGPIPE 訊號，防止 Server 斷線時 Client 崩潰
    signal(SIGPIPE, SIG_IGN); 

    // 1. 檢查參數
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <Server IP> <Port> <Client ID>\n", argv[0]);
        return -1;
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int client_id = atoi(argv[3]);

    log_init(NULL); 
    log_info("Client %d connecting to %s:%d...", client_id, server_ip, server_port);

    // 2. 連線
    int sock_fd = connect_to_server(server_ip, server_port);
    if (sock_fd < 0) {
        log_error("Connection failed.");
        log_cleanup();
        return -1;
    }

    // 3. 設定 Timeout (接收超時 2 秒)
    struct timeval tv = {2, 0};
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    if (client_id <= 10) {
        printf("\n>>> Client ID %d: Requesting as a \033[1;36mVIP Customer\033[0m.\n", client_id);
    } else {
        printf("\n>>> Client ID %d: Requesting as a Standard Customer.\n", client_id);
    }
    // 4. 發送請求
    char msg_buffer[1024]; 
    
    // 呼叫 client_core.c 中的核心網路邏輯
    int result = perform_ride_request(sock_fd, client_id, msg_buffer);

    // 5. 顯示結果
    if (result == 0) {
        printf("✅ Success! %s\n", msg_buffer);
    } else if (result == -2) {
        // DoS 阻擋錯誤碼
        printf("⚠️ Blocked! %s (DoS Limit Reached)\n", msg_buffer);
    } else {
        printf("❌ Request failed or rejected. Response: %s\n", msg_buffer);
    }

    close(sock_fd);
    log_cleanup();
    return 0;
}