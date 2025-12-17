/* src/common/net_wrapper.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "include/net_wrapper.h"
#include "include/log_system.h" 

//  Socket 建立與連線
/**
 * 建立伺服器監聽 Socket (socket -> setsockopt -> bind -> listen)。
 * port 監聽的端口號
 * return file descriptor 或 -1 (失敗)
 */
int create_server_socket(int port) {
    int fd;
    struct sockaddr_in server_addr;

    // 1. 建立 TCP Socket
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    // 2. 設定 SO_REUSEADDR
    // 這允許伺服器在關閉後 (或崩潰後) 立即重新綁定到同一個 Port，
    // 解決 "bind: Address already in use" 的問題。
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    // 3. 綁定地址 (Bind)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 監聽所有網卡
    server_addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    // 4. 監聽 (Listen)
    if (listen(fd, 1024) < 0) { // Backlog 設為 1024 以應對高併發
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * 連線到伺服器 (socket -> connect)。
 * ip 伺服器 IP 地址
 * port 伺服器端口號
 * return file descriptor 或 -1 (失敗)
 */
int connect_to_server(const char *ip, int port) {
    int fd;
    struct sockaddr_in server_addr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // 將 IP 字串轉換為二進位
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        close(fd);
        return -1;
    }

    // 發起連線
    if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

//  資料傳輸的穩定化 (N-byte 讀寫)
/**
 * 確保發送 n 個位元組 (處理 partial write)。
 * fd socket file descriptor
 * buf 數據緩衝區
 * n 欲發送的位元組數
 * return 實際發送位元組數 (n) 或 -1 (錯誤)
 */
ssize_t send_n(int fd, const void *buf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *ptr = buf;

    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0; // 被訊號中斷，重試
            else
                return -1;    // 錯誤
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

/**
 * 確保接收 n 個位元組 (處理 partial read)。
 * fd socket file descriptor
 * buf 數據緩衝區
 * n 欲接收的位元組數
 * return 實際讀到的位元組數 (n) 或 <=0 (錯誤或連線關閉)
 */
ssize_t recv_n(int fd, void *buf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *ptr = buf;

    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      // 被訊號中斷，重試
            else
                return -1;      // 錯誤
        } else if (nread == 0) {
            break;              // EOF (對方關閉連線)
        }
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); // 回傳實際讀到的位元組數
}

//  RC4 Stream Cipher 實作 (機密性)
/**
 * RC4 加密/解密函式。
 * data 待處理數據
 * len 數據長度
 * key RC4 金鑰
 */
void rc4_crypt(unsigned char *data, size_t len, const char *key) {
    unsigned char S[256];
    unsigned char temp;
    int i, j = 0;
    size_t key_len = strlen(key);

    // 1. KSA (Key-Scheduling Algorithm) - 初始化狀態陣列
    for (i = 0; i < 256; i++) {
        S[i] = i;
    }
    
    for (i = 0; i < 256; i++) {
        j = (j + S[i] + key[i % key_len]) % 256;
        // Swap S[i] and S[j]
        temp = S[i];
        S[i] = S[j];
        S[j] = temp;
    }

    // 2. PRGA (Pseudo-Random Generation Algorithm) - 生成密鑰流並 XOR
    i = 0;
    j = 0;
    for (size_t n = 0; n < len; n++) {
        i = (i + 1) % 256;
        j = (j + S[i]) % 256;
        
        // Swap S[i] and S[j]
        temp = S[i];
        S[i] = S[j];
        S[j] = temp;

        // 生成密鑰流 byte
        unsigned char k = S[(S[i] + S[j]) % 256];

        // XOR 運算
        data[n] ^= k;
    }
}