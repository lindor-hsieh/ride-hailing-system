/* src/common/dh_crypto.c */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// 使用較小的質數與生成元，方便演示且不需大數運算庫
// P = 2147483647 (Mersenne Prime 2^31 - 1)
// G = 16807 (7^5, Primitive root modulo P)
#define DH_PRIME 2147483647L
#define DH_GENERATOR 16807L

// 模數冪運算: (base^exp) % mod
// 使用 long long 防止計算過程溢位
long long power_mod(long long base, long long exp, long long mod) {
    long long res = 1;
    base = base % mod;
    while (exp > 0) {
        if (exp % 2 == 1) res = (res * base) % mod;
        exp = exp >> 1;
        base = (base * base) % mod;
    }
    return res;
}

// 生成私鑰 (隨機數)
long long generate_private_key() {
    return (rand() % 100000) + 1; // 簡單隨機
}

// 計算公鑰: G^Private % P
long long calculate_public_key(long long private_key) {
    return power_mod(DH_GENERATOR, private_key, DH_PRIME);
}

// 計算共享密鑰: OtherPublic^MyPrivate % P
long long calculate_shared_secret(long long other_public_key, long long my_private_key) {
    return power_mod(other_public_key, my_private_key, DH_PRIME);
}

// 將共享密鑰整數轉換為 RC4 可用的字串 Key
void derive_session_key(long long shared_secret, char *output_buffer, size_t len) {
    snprintf(output_buffer, len, "KEY_%lld_SECURE", shared_secret);
}