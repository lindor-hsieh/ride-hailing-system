🚖 Smart City Ride-Hailing System (智慧城市叫車系統)
Course: Network Systems Programming and Security Final Project

Language: C (Linux System Programming)

Architecture: Multi-Process Server (Preforking) + Multi-Threaded Client

📖 Introduction
本專案實作了一個高併發、高可靠性的模擬叫車系統伺服器。系統採用 Linux 多行程架構 (Multi-Process Architecture) 設計，利用 IPC (Shared Memory & Mutex) 解決競爭問題，並實作了自定義的二進位通訊協定與安全加密層。

系統能模擬真實世界的商業邏輯，包含司機的 A 路徑規劃*、動態定價 (Surge Pricing) 以及 VIP 優先媒合 機制，並具備防禦 DoS 攻擊與資料竄改的能力。


✨ 核心功能 (Key Features)
1. 系統架構 (System Architecture)
Preforking Model: 採用 Master-Worker 模式，主行程預先 Fork 多個 Dispatcher 行程建立 Process Pool，避免連線時的 Fork 開銷。

High Concurrency: 支援 100+ 並發連線的壓力測試，確保在高負載下的穩定性。

IPC Mechanism: 使用 mmap (POSIX Shared Memory) 共享全域司機狀態與統計數據，並透過 pthread_mutex (Process-Shared) 實現跨行程的互斥鎖定。

2. 商業邏輯 (Business Logic)
Matchmaking Algorithms:

Basic Mode: 基於歐幾里得距離的最近司機搜尋。

Smart Mode: 針對 VIP 客戶的加權演算法（考量司機評分、距離與車況）。

Pathfinding: 實作 *A (A-Star)** 演算法，模擬司機在 40x20 的城市網格中避開障礙物導航。

Dynamic Pricing: 根據供需比率自動觸發 Surge Pricing (加價機制)。

3. 通訊協定與安全性 (Protocol & Security)
Custom Application Protocol: 定義嚴格的封包結構 (Length + OpCode + Checksum + Payload)，解決 TCP 黏包問題。

Security Layer:

Confidentiality: 實作 RC4 Stream Cipher 加密所有 Payload，防止明文傳輸。

Authentication: 實作 Diffie-Hellman Key Exchange 握手協定，動態協商 Session Key。

Integrity: 封包表頭包含 Checksum 校驗，防止傳輸竄改。

Reliability: 具備 DoS Rate Limiting (流量清洗) 與 Graceful Shutdown (資源釋放) 機制


📂 檔案結構 (Project Structure)
.
├── src/
│   ├── client/          # 客戶端程式碼
│   │   ├── stress_client.c  (多執行緒壓力測試工具)
│   │   ├── client_core.c    (客戶端核心邏輯)
│   │   └── ...
│   ├── server/          # 伺服器端程式碼
│   │   ├── server_main.c    (主程式進入點 & IPC 初始化)
│   │   ├── coordinator.c    (Master Process 管理)
│   │   ├── dispatcher.c     (Worker Process 請求處理)
│   │   ├── ride_service.c   (媒合與計價邏輯)
│   │   ├── map_monitor.c    (地圖狀態同步與 A* 導航)
│   │   └── ...
│   └── common/          # 共用模組 (封裝為 libcommon.a)
│       ├── include/         (標頭檔: protocol.h, shared_data.h)
│       ├── net_wrapper.c    (Socket 封裝)
│       ├── dh_crypto.c      (Diffie-Hellman & RC4 加密)
│       └── protocol.c       (封包解析與 Checksum 計算)
├── lib/                 # 編譯出的靜態函式庫
├── Makefile             # 自動化編譯腳本
└── README.md

🚀 編譯與執行 (Build & Run)
1. 環境需求
GCC Compiler

Linux Environment (Ubuntu/Debian recommended)

Libraries: pthread, rt (Real-time extensions)

2. 編譯專案
使用 Makefile 進行一鍵編譯：

Bash

# 清除舊檔並重新編譯
make clean && make
3. 啟動伺服器 (Server)
Bash

# Usage: ./server_app <port> <worker_count> <dispatch_mode>
# mode: 0 = Basic, 1 = Smart
./server_app 8888 8 1
4. 啟動客戶端 (Client)
選項 A: 壓力測試 (Stress Test) 模擬 100 個並發連線進行壓力測試：

Bash

# Usage: ./stress_client <ip> <port> <total_requests> <concurrency>
./stress_client 127.0.0.1 8888 1000 100
選項 B: 單一客戶端 (Interactive/Debug)

Bash

./single_client 127.0.0.1 8888
🛡️ 安全性驗證展示 (Security Proof)
本系統在 Runtime 提供了完整的安全性驗證 Log。

1. 完整性檢查 (Integrity Check)
伺服器即時計算 Checksum，確保封包未被竄改。

[SECURITY] Integrity Check PASSED! (Calc: A1B2 == Header: A1B2)

2. 加密傳輸驗證 (Encryption Proof)
展示 Payload 在網路傳輸層 (Ciphertext) 與應用層 (Plaintext) 的差異，證明 RC4 加密生效。

[Proof] Encrypted Data: A9 F3 11 B2 ... (RC4 Encrypted)

[Proof] Decrypted Data: ClientID=1, Type=VIP

3. 攻擊防禦 (DoS & Auth)
當惡意客戶端試圖跳過握手或發送錯誤 Checksum 時，伺服器將拒絕服務。

[SECURITY] Checksum mismatch! Session Key might be wrong.

📊 效能指標 (Performance)
在標準 Linux 環境下 (4 vCPU, 8GB RAM) 進行測試：

Throughput: ~1200 Requests/sec

Average Latency: < 5ms

IPC Overhead: 極低 (使用 Shared Memory 避免了大量 System Call)

📝 License
This project is developed for academic purposes. Copyright © 2025 [Your Name/Student ID]. All Rights Reserved.