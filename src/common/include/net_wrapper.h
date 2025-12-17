/* src/common/include/net_wrapper.h */
#ifndef NET_WRAPPER_H
#define NET_WRAPPER_H

#include <stddef.h> 
#include <sys/types.h> // 包含 ssize_t

// Socket 建立與連線

// 建立伺服器 Socket (socket -> bind -> listen)
int create_server_socket(int port);

// 連線到伺服器 (socket -> connect)
int connect_to_server(const char *ip, int port);

// 資料傳輸 (確保 N bytes 讀寫)

// 確保發送 n 個位元組 
ssize_t send_n(int fd, const void *buf, size_t n);

// 確保接收 n 個位元組
ssize_t recv_n(int fd, void *buf, size_t n);

// 加密/解密 (機密性)

// RC4 加密/解密函式 (在 net_wrapper.c 實作)
void rc4_crypt(unsigned char *data, size_t len, const char *key);

#endif // NET_WRAPPER_H