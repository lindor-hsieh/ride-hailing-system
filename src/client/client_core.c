/* src/client/client_core.c */
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

// 用動態協商的 Key
// #define SECRET_KEY "..." (已移除)

// 引入 DH 數學函式 (from src/common/dh_crypto.c)
extern long long generate_private_key();
extern long long calculate_public_key(long long private_key);
extern long long calculate_shared_secret(long long other_public, long long my_private);
extern void derive_session_key(long long secret, char *buf, size_t len);

// 取得當前時間 (ms)
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// 取得時間字串
void get_time_str(char *buffer, size_t size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    strftime(buffer, size, "%H:%M:%S", tm_info);
    char ms[10];
    snprintf(ms, sizeof(ms), ".%03d", (int)(tv.tv_usec / 1000));
    strcat(buffer, ms);
}

// 印出 Hex (for debug)
void print_hex(const char *label, uint8_t *data, size_t len) {
    printf("[DEBUG] %s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// DH HandShake 邏輯
// 成功返回 0，失敗返回 -1，並將生成的 Key 填入 session_key_out
int perform_dh_handshake(int sock_fd, char *session_key_out) {
    // 1. 生成 Client 自己的密鑰對
    long long my_priv = generate_private_key();
    long long my_pub  = calculate_public_key(my_priv);

    // 2. 準備發送 Client 公鑰
    ProtocolHeader header;
    HandshakeData body;

    body.public_key = (int64_t)my_pub; // 轉型為協議定義的 int64_t

    header.length = sizeof(HandshakeData);
    header.type = MSG_TYPE_HANDSHAKE;     // 設定類型為握手
    header.opcode = OP_HANDSHAKE;         // 設定操作碼
    header.checksum = 0;                  // 握手階段通常不作 checksum 或簡單處理

    // 3. 發送
    if (send_n(sock_fd, &header, sizeof(ProtocolHeader)) <= 0) return -1;
    if (send_n(sock_fd, &body, sizeof(HandshakeData)) <= 0) return -1;

    // 4. 接收 Server 回應 (Server 公鑰)
    ProtocolHeader resp_header;
    HandshakeData resp_body;

    if (recv_n(sock_fd, &resp_header, sizeof(ProtocolHeader)) <= 0) return -1;
    
    // 檢查回應類型
    if (resp_header.type != MSG_TYPE_HANDSHAKE_ACK) return -1;

    if (recv_n(sock_fd, &resp_body, sizeof(HandshakeData)) <= 0) return -1;

    // 5. 計算共享密鑰 (Shared Secret)
    long long server_pub = (long long)resp_body.public_key;
    long long shared_secret = calculate_shared_secret(server_pub, my_priv);

    // 6. 衍生 Session Key (轉成字串)
    derive_session_key(shared_secret, session_key_out, 64);

    printf("[Security] DH Handshake Success. Key Established.\n");
    return 0;
}

// 核心連線邏輯

/**
 * 發送叫車請求的核心邏輯 (包含握手)。
 * return 0 = 成功, -1 = 失敗, -2 = 被 DoS 阻擋
 */
int perform_ride_request(int sock_fd, int client_id, char *msg_buffer) {
    char session_key[64]; // 用來存放動態協商的 Key
    int assigned_driver_id; 

    // 0. 先執行 DH 握手
    if (perform_dh_handshake(sock_fd, session_key) < 0) {
        snprintf(msg_buffer, 1024, "Handshake Failed");
        return -1;
    }

    // 握手成功，以下通訊都使用 session_key 加密

    ProtocolHeader req_header;
    RideRequestData req_body;

    // 1. 準備 Body
    req_body.client_id = client_id;
    // 設定 VIP 邏輯：ID <= 10 為 VIP
    req_body.type = (client_id <= 10) ? 1 : 0; 
    req_body.lat = 25.0330;
    req_body.lon = 121.5654;

    // 2. 準備 Header
    req_header.type = MSG_TYPE_RIDE_REQ; // 設定訊息類型
    req_header.opcode = OP_REQ_RIDE;     // 設定操作碼
    req_header.length = sizeof(RideRequestData);
    
    // 計算 Checksum (加密前計算)
    req_header.checksum = calculate_checksum((uint8_t*)&req_body, req_header.length);

    // 輸出 DEBUG Log (只針對 Client 1)
    if (client_id == 1) print_hex("Before Encrypt", (uint8_t*)&req_body, req_header.length);

    // 使用動態 Session Key 加密
    rc4_crypt((uint8_t*)&req_body, req_header.length, session_key);

    if (client_id == 1) print_hex("After  Encrypt", (uint8_t*)&req_body, req_header.length);

    // 3. 發送
    if (send_n(sock_fd, &req_header, sizeof(ProtocolHeader)) <= 0) return -1;
    if (send_n(sock_fd, &req_body, req_header.length) <= 0) return -1;

    // 4. 接收 Header
    ProtocolHeader resp_header;
    if (recv_n(sock_fd, &resp_header, sizeof(ProtocolHeader)) <= 0) return -1;

    // 5. 接收 Body
    if (resp_header.length > 0 && resp_header.length < 1024) {
        if (recv_n(sock_fd, msg_buffer, resp_header.length) <= 0) return -1;
        msg_buffer[resp_header.length] = '\0'; 
        
        // 使用動態 Session Key 解密
        rc4_crypt((uint8_t*)msg_buffer, resp_header.length, session_key);

        // Checksum 驗證
        uint16_t local_checksum = calculate_checksum((uint8_t*)msg_buffer, resp_header.length);
        if (local_checksum != resp_header.checksum) {
             snprintf(msg_buffer, 1024, "Checksum Mismatch");
             return -1; 
        }

        // 解析回覆
        if (strstr(msg_buffer, "Ride Confirmed!")) {
            sscanf(msg_buffer, "%*[^:]: %d", &assigned_driver_id);
            return 0; // 成功
        } else if (strstr(msg_buffer, "Blocked.")) {
            return -2; // 被 DoS 阻擋
        }
    }
    return -1; // 失敗
}