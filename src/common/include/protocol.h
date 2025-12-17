/* src/common/include/protocol.h */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// 訊息類型定義
#define MSG_TYPE_RIDE_REQ   1   // 一般叫車請求
#define MSG_TYPE_RIDE_RESP  2   // 伺服器回應
#define MSG_TYPE_HANDSHAKE  3   // Client 發送 DH 公鑰
#define MSG_TYPE_HANDSHAKE_ACK 4 // Server 回覆 DH 公鑰

// 操作碼定義 (Opcodes)
#define OP_REQ_RIDE     0x0001  // 乘客請求叫車
#define OP_DRIVER_JOIN  0x0002  // 司機加入網路
#define OP_UPDATE_LOC   0x0003  // 更新位置
#define OP_RESPONSE     0x8000  // 伺服器回應
#define OP_HANDSHAKE    0x0004  // 握手操作

// 協定頭部 (Header)
typedef struct {
    uint32_t length;    // [Packet Length]: Body 的長度 (4 bytes)
    uint8_t type;       // [Msg Type]: 訊息類型 (1 byte)
    uint16_t opcode;    // [OpCode]: 操作碼 (2 bytes)
    uint16_t checksum;  // [Checksum]: 完整性校驗 (2 bytes)
} __attribute__((packed)) ProtocolHeader;

// (Body Payloads)
// 1. DH 金鑰交換 Payload
// 用於 MSG_TYPE_HANDSHAKE 和 MSG_TYPE_HANDSHAKE_ACK
typedef struct {
    int64_t public_key; // 存放 DH 演算法生成的公鑰 (64-bit)
} __attribute__((packed)) HandshakeData;

// 2. 乘客叫車請求 Payload
// 增加了 type 欄位以支援 VIP 邏輯
typedef struct {
    uint32_t client_id;
    uint32_t type;      // 0=Standard, 1=VIP
    double lat;         // 緯度
    double lon;         // 經度
} __attribute__((packed)) RideRequestData;

// 3. 司機加入請求 Payload (保持不變)
typedef struct {
    uint32_t driver_id;
    // 可以根據需要擴充其他欄位
} __attribute__((packed)) DriverJoinData;

// 安全性與工具函式宣告

// 計算校驗和 (在 protocol.c 實作)
uint16_t calculate_checksum(const uint8_t *data, size_t len);

#endif // PROTOCOL_H