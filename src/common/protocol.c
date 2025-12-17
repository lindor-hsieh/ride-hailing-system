/* src/common/protocol.c */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "include/protocol.h" 

//  Checksum 演算法 (完整性)
/**
 * 計算 16-bit 校驗和 (使用類似 IP/TCP 的累加和演算法)。
 * data 待校驗的數據
 * len 數據長度
 * return 16-bit 校驗和
 */
uint16_t calculate_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    const uint8_t *ptr = data;

    // 逐字節累加
    for (size_t i = 0; i < len; i++) {
        sum += *ptr++;
    }

    // 將高 16 位加到低 16 位 (Fold)
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum; // 取反
}