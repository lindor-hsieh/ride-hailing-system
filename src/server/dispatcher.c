/* src/server/dispatcher.c */
// #define SECRET_KEY ... (移除或註解掉，因為現在要用動態 Session Key)

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

// 引入 DH 數學函式 (來自 src/common/dh_crypto.c)
extern long long generate_private_key();
extern long long calculate_public_key(long long private_key);
extern long long calculate_shared_secret(long long other_public, long long my_private);
extern void derive_session_key(long long secret, char *buf, size_t len);

extern void process_driver_join(int client_fd, ProtocolHeader *header, uint8_t *body);

// 前向宣告
void handle_client(int client_fd);

/**
 * 封裝回覆邏輯：使用動態 Session Key 加密、計算 Checksum 並發送。
 */
void send_response_packet(int client_fd, char *resp_msg, size_t len, uint16_t opcode, const char *session_key) {
    ProtocolHeader resp_header;
    resp_header.type = MSG_TYPE_RIDE_RESP; // 設定類型
    resp_header.opcode = opcode;
    resp_header.length = (uint32_t)len;
    resp_header.checksum = calculate_checksum((uint8_t*)resp_msg, len);

    // ★ 使用協商好的 Session Key 加密回覆 (機密性)
    if (session_key != NULL) {
        rc4_crypt((uint8_t*)resp_msg, len, session_key);
    }

    send_n(client_fd, &resp_header, sizeof(ProtocolHeader));
    send_n(client_fd, resp_msg, len);
}

/**
 * Dispatcher 進程的主迴圈：持續接受連線。
 */
void dispatcher_loop(int server_fd) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue; // 忽略被訊號中斷
            continue;
        }
        handle_client(client_fd); // 處理單一連線
        close(client_fd);         // 處理完畢後關閉 (短連線模型)
    }
}

/**
 * 請求處理Wrapper：處理叫車業務請求
 * 增加 session_key 參數，以便加密回覆
 */
void process_ride_request_wrapper(int client_fd, ProtocolHeader *in_header, uint8_t *body, const char *session_key) {
    (void)in_header;
    RideRequestData *req = (RideRequestData *)body; 
    char resp_msg[256]; 
    
    // 1. 安全檢查 (Rate Limit)
    if (check_and_update_rate_limit(req->client_id)) { 
        char *err_msg = "Error: Blocked.";
        printf("\033[1;31m[SECURITY] Blocked DoS attack from Client %d!\033[0m\n", req->client_id);
        send_response_packet(client_fd, err_msg, strlen(err_msg), OP_RESPONSE, session_key);
        return; 
    }

    // 2. 業務處理 (單一呼叫 Service Layer)
    // 注意：handle_ride_request_logic 現在需要處理 VIP 邏輯 (req->type)
    // 為了相容，我們先假設 logic 函式只看 ID，或者您可以修改 logic 函式傳入 req->type
    // 這裡演示直接呼叫:
    int result = handle_ride_request_logic(req->client_id, resp_msg, sizeof(resp_msg));
    (void)result; 

    // 3. 網路回覆 (使用 Session Key 加密)
    send_response_packet(client_fd, resp_msg, strlen(resp_msg), OP_RESPONSE, session_key);
}

/**
 * 處理單一客戶端連線，負責 DH 握手、I/O 和安全過濾。
 */
void handle_client(int client_fd) {
    ProtocolHeader header;
    uint8_t body[1024];
    char session_key[64] = {0}; // 存放這個連線專屬的 Key
    int is_key_established = 0; // 標記握手是否完成

    // 迴圈接收：因為可能會先收到握手包，再收到資料包
    while (1) {
        ssize_t bytes_read = recv_n(client_fd, &header, sizeof(ProtocolHeader));
        if (bytes_read <= 0) break; // 連線斷開

        if (header.length > 1024) break; // 防止 Buffer Overflow

        // 讀取 Body
        if (header.length > 0) {
            bytes_read = recv_n(client_fd, body, header.length);
            if (bytes_read != header.length) break;
        }

        // 處理握手請求 (MSG_TYPE_HANDSHAKE)
        if (header.type == MSG_TYPE_HANDSHAKE) {
            HandshakeData *client_dh = (HandshakeData *)body;
            
            // 1. Server 生成密鑰對
            long long srv_priv = generate_private_key();
            long long srv_pub  = calculate_public_key(srv_priv);
            
            // 2. 計算 Shared Secret (利用 Client 公鑰 + Server 私鑰)
            long long shared = calculate_shared_secret((long long)client_dh->public_key, srv_priv);
            
            // 3. 衍生 Session Key
            derive_session_key(shared, session_key, 64);
            is_key_established = 1;
            
            log_info("[Security] DH Handshake Success. Session Key Established.");

            // 4. 回覆 Server 公鑰給 Client
            ProtocolHeader resp_h;
            HandshakeData resp_body;
            
            resp_body.public_key = (int64_t)srv_pub;

            resp_h.type = MSG_TYPE_HANDSHAKE_ACK;
            resp_h.opcode = OP_HANDSHAKE;
            resp_h.length = sizeof(HandshakeData);
            resp_h.checksum = 0; // 握手不校驗

            send_n(client_fd, &resp_h, sizeof(ProtocolHeader));
            send_n(client_fd, &resp_body, sizeof(HandshakeData));
            
            continue; // 握手完成，繼續等待下一個封包 (業務請求)
        }

        // 處理叫車請求 (MSG_TYPE_RIDE_REQ) ---
        if (header.type == MSG_TYPE_RIDE_REQ) {
            
            // 安全強制：如果沒握手就傳資料，直接踢掉
            if (!is_key_established) {
                printf("\033[1;31m[SECURITY] Rejected: Request without Handshake!\033[0m\n");
                break;
            }

            // 網路層職責：使用 Session Key 解密 (機密性)
            rc4_crypt(body, header.length, session_key);
            
            // 網路層職責：Checksum 驗證 (完整性)
            uint16_t checksum = calculate_checksum(body, header.length);
            if (checksum != header.checksum) {
                printf("\033[1;31m[SECURITY] Checksum mismatch! Session Key might be wrong.\033[0m\n");
                return; 
            }

            // 分發業務邏輯
            if (header.opcode == OP_REQ_RIDE) {
                process_ride_request_wrapper(client_fd, &header, body, session_key);
                break; // 處理完一個請求後結束 (依您的架構，這裡處理完就斷線)
            }
        }

        // 處理司機加入 (OP_DRIVER_JOIN)
        // 注意：如果要讓司機也支援加密，司機端程式也要加 DH 邏輯
        // 這裡暫時保持原樣 (不加密或假設司機走內部網路)
        if (header.opcode == OP_DRIVER_JOIN) {
             process_driver_join(client_fd, &header, body);
             break;
        }
    }
}