/* src/server/insecure_dispatcher.c */
// (Insecure Version)
#define SECRET_KEY "RIDE_HAILING_2025_SECURE_KEY" 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>             
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>       
#include <pthread.h>    

// 引入共用模組
#include "../../common/include/protocol.h" 
#include "../../common/include/shared_data.h"
#include "../../common/include/log_system.h"
#include "../../common/include/net_wrapper.h"
// 引入業務服務層
#include "../include/ride_service.h" 
#include "../include/resource_service.h" 

extern SharedState *g_shared_state;

// 宣告在 coordinator.c 中定義的共用函式
extern void process_driver_join(int client_fd, ProtocolHeader *header, uint8_t *body);

// 前向宣告
void handle_client_insecure(int client_fd);

/**
 * 封裝回覆邏輯 (漏洞版)：回覆不加密。
 */
void send_response_packet_insecure(int client_fd, char *resp_msg, size_t len, uint16_t opcode) {
    ProtocolHeader resp_header;
    resp_header.opcode = opcode;
    resp_header.length = (uint32_t)len;
    resp_header.checksum = calculate_checksum((uint8_t*)resp_msg, len);

    // 🚨 漏洞點 1：移除 RC4 加密！ (Payload 將以明文發送 - 機密性缺失) 🚨

    send_n(client_fd, &resp_header, sizeof(ProtocolHeader));
    send_n(client_fd, resp_msg, len);
}

/**
 * Dispatcher 進程的主迴圈 (漏洞版)。
 */
void dispatcher_loop_insecure(int server_fd) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            continue;
        }
        handle_client_insecure(client_fd);
        close(client_fd);
    }
}

/**
 * 請求處理Wrapper (漏洞版)。
 */
void process_ride_request_insecure_wrapper(int client_fd, ProtocolHeader *in_header, uint8_t *body) {
    (void)in_header;
    RideRequestData *req = (RideRequestData *)body; 
    char resp_msg[256]; 
    
    // 🚨 漏洞點 2：移除 DoS 檢查！(可用性缺失) 🚨
    // if (check_and_update_rate_limit(req->client_id)) { ... return; }

    // 業務處理 (單一呼叫 Service Layer)
    int result = handle_ride_request_logic(req->client_id, resp_msg, sizeof(resp_msg));
    (void)result;

    // 網路回覆 (使用漏洞版的發送函式)
    send_response_packet_insecure(client_fd, resp_msg, strlen(resp_msg), OP_RESPONSE);
}

/**
 * 處理單一客戶端連線 (漏洞版)。
 */
void handle_client_insecure(int client_fd) {
    ProtocolHeader header;
    ssize_t bytes_read;
    uint8_t body[1024];

    bytes_read = recv_n(client_fd, &header, sizeof(ProtocolHeader));
    if (bytes_read <= 0) return;

    if (header.length > 1024) return;

    if (header.length > 0) {
        bytes_read = recv_n(client_fd, body, header.length);
        if (bytes_read != header.length) return;
        
        // 🚨 漏洞點 3：移除 RC4 解密！(機密性缺失) 🚨
        
        // 🚨 漏洞點 4：移除 Checksum 驗證！(完整性缺失) 🚨
    }

    // F. 分發業務邏輯
    switch (header.opcode) {
        case OP_REQ_RIDE:
            process_ride_request_insecure_wrapper(client_fd, &header, body); 
            break;
        case OP_DRIVER_JOIN:
            // 司機上線邏輯已移至 Coordinator.c
            process_driver_join(client_fd, &header, body);
            break;
        default:
            break;
    }
}