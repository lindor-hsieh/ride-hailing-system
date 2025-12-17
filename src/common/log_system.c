/* src/common/log_system.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#include "include/log_system.h"

static FILE *log_fp = NULL;

/**
 * 初始化日誌系統。
 * filename 如果提供，則輸出到檔案；否則輸出到 stderr。
 */
void log_init(const char *filename) {
    if (filename) {
        log_fp = fopen(filename, "a"); // Append 模式
        if (!log_fp) {
            fprintf(stderr, "Failed to open log file %s, using stderr.\n", filename);
            log_fp = stderr;
        }
    } else {
        log_fp = stderr;
    }
}

/**
 * 清理日誌資源，關閉檔案。
 */
void log_cleanup() {
    if (log_fp && log_fp != stderr) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

/**
 * 內部輔助：實際列印函式。
 */
static void log_base(const char *level, const char *fmt, va_list args) {
    if (!log_fp) log_fp = stderr;

    // 1. 獲取時間
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local);

    // 2. 格式化輸出
    fprintf(log_fp, "[%s] [%s] ", time_str, level);
    vfprintf(log_fp, fmt, args);
    fprintf(log_fp, "\n");

    // 3. 確保寫入磁碟
    fflush(log_fp);
}

//  日誌輸出介面
void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_base("INFO", fmt, args);
    va_end(args);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_base("WARN", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_base("ERROR", fmt, args);
    va_end(args);
}

void log_debug(const char *fmt, ...) {
    #ifdef DEBUG 
    va_list args;
    va_start(args, fmt);
    log_base("DEBUG", fmt, args);
    va_end(args);
    #else
    (void)fmt; 
    #endif
}