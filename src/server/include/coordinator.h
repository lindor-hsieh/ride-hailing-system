#ifndef COORDINATOR_H
#define COORDINATOR_H

// 啟動 Coordinator 主流程 (安全版)
void start_coordinator_process(int server_fd);

// 啟動 Coordinator 主流程 (漏洞版)
void start_coordinator_process_insecure(int server_fd);

// 處理司機加入
void process_driver_join(int client_fd, void *header, void *body);

#endif