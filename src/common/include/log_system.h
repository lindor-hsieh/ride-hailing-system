/* src/common/include/log_system.h */
#ifndef LOG_SYSTEM_H
#define LOG_SYSTEM_H

// 初始化日誌系統 
void log_init(const char *filename);

// 清理日誌資源
void log_cleanup();

// 日誌函式
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_debug(const char *fmt, ...);

#endif // LOG_SYSTEM_H