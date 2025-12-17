/* src/client/malicious_client.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h> 

#include "../../common/include/protocol.h"
#include "../../common/include/net_wrapper.h"
#include "../../common/include/log_system.h"

// 資安測試二：故意使用錯誤的金鑰 (MUST BE WRONG!)
// 正確金鑰是: "RIDE_HAILING_2025_SECURE_KEY"
#define WRONG_SECRET_KEY "HACKER_WRONG_2025_KEY!" 

/**
 * 惡意發送請求：使用錯誤金鑰加密，但 Checksum 正確。
 * Server 在解密後會發現 Checksum 錯誤並拒絕請求。
 */
int perform_malicious_request(int sock_fd, int client_id, char *msg_buffer) {
    ProtocolHeader req_header;
    RideRequestData req_body;
    
    // 1. 準備 Body (Payload)
    req_body.client_id = client_id;
    req_body.lat = 25.0330;
    req_body.lon = 121.5654;

    req_header.opcode = OP_REQ_RIDE;
    req_header.length = sizeof(RideRequestData);
    
    // 2. 計算正確的 Checksum (Server 預期的值)
    uint16_t original_checksum = calculate_checksum((uint8_t*)&req_body, req_header.length);
    req_header.checksum = original_checksum; 

    // 3. 使用錯誤的金鑰進行加密 (竄改)
    rc4_crypt((uint8_t*)&req_body, req_header.length, WRONG_SECRET_KEY); 

    // 4. 發送 (送出的是：錯誤加密的密文 + 正確的 Checksum)
    if (send_n(sock_fd, &req_header, sizeof(ProtocolHeader)) <= 0) return -1;
    if (send_n(sock_fd, &req_body, req_header.length) <= 0) return -1;

    // 5. 接收回覆 (Server 預期會直接丟棄連線或不回覆，這裡嘗試讀取)
    ProtocolHeader resp_header;
    if (recv_n(sock_fd, &resp_header, sizeof(ProtocolHeader)) > 0) {
        // 如果 Server 決定回覆，我們嘗試讀取
        if (resp_header.length > 0 && resp_header.length < 1024) {
            if (recv_n(sock_fd, msg_buffer, resp_header.length) > 0) {
                 msg_buffer[resp_header.length] = '\0';
                 // 這裡無需解密，只需看 Server 是否回覆錯誤訊息
                 return 0;
            }
        }
    }
    
    return -1; // 失敗 (可能是 Server 直接斷線)
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN); 

    if (argc < 4) {
        printf("Usage: %s <Server IP> <Port> <Malicious Client ID>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int client_id = atoi(argv[3]);

    printf("[ATTACK] Starting malicious client %d (ID: %d)...\n", client_id, client_id);

    int sock_fd = connect_to_server(server_ip, server_port);
    if (sock_fd < 0) {
        printf("[ERROR] Connection failed.\n");
        return -1;
    }

    char msg_buffer[1024] = {0};
    printf("[ATTACK] Sending forged packet with WRONG KEY and valid Checksum...\n");
    
    int result = perform_malicious_request(sock_fd, client_id, msg_buffer);

    if (result == -1) {
        printf("[RESULT] ✅ SUCCESS: Server dropped connection or sent no reply (Security Refusal).\n");
        printf("         (Server should have logged 'Checksum mismatch').\n");
    } else {
        printf("[RESULT] ⚠️ FAILURE: Server returned a response (%s). Check Server Log.\n", msg_buffer);
    }
    
    close(sock_fd);
    return 0;
}