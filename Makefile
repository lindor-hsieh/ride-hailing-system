# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -Isrc/common/include -Isrc/server/include -Isrc/client/include -lm
LDFLAGS = -Llib -lcommon -pthread -lrt -lm 

# Target Executables
SERVER_APP = server_app
INSECURE_APP = insecure_server
CLIENT_APP = client_app
STRESS_APP = stress_client
MALICIOUS_APP = malicious_client
DUMP_APP = dump_dat
LIB_COMMON = lib/libcommon.a

# Source Files Definitions
# Common Lib
COMMON_SRCS = src/common/net_wrapper.c src/common/log_system.c src/common/protocol.c src/common/dh_crypto.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

# Server Core 
SERVER_CORE_SRCS = src/server/coordinator.c src/server/dispatcher.c src/server/insecure_dispatcher.c src/server/ride_service.c src/server/pricing_service.c src/server/resource_service.c src/server/map_monitor.c src/server/dispatch_algorithms.c src/server/pathfinding.c
SERVER_CORE_OBJS = $(SERVER_CORE_SRCS:.c=.o)

# Main Entries
SERVER_MAIN_SRCS = src/server/server_main.c
INSECURE_MAIN_SRCS = src/server/insecure_server.c
SERVER_MAIN_OBJS = $(SERVER_MAIN_SRCS:.c=.o)
INSECURE_MAIN_OBJS = $(INSECURE_MAIN_SRCS:.c=.o)

# Client Sources
CLIENT_CORE_SRCS = src/client/client_core.c 
CLIENT_CORE_OBJS = $(CLIENT_CORE_SRCS:.c=.o)
CLIENT_MAIN_SRCS = src/client/single_client.c src/client/stress_client.c src/client/malicious_client.c
CLIENT_MAIN_OBJS = $(CLIENT_MAIN_SRCS:.c=.o)

# Main Rules
.PHONY: all clean dump

all: directories $(LIB_COMMON) $(CLIENT_CORE_OBJS) $(SERVER_MAIN_OBJS) $(INSECURE_MAIN_OBJS) $(SERVER_CORE_OBJS) $(CLIENT_MAIN_OBJS) $(SERVER_APP) $(INSECURE_APP) $(CLIENT_APP) $(STRESS_APP) $(MALICIOUS_APP) $(DUMP_APP)

directories:
	@mkdir -p lib

$(LIB_COMMON): $(COMMON_OBJS)
	ar rcs $@ $^
	@echo "Library $@ created."

# 1. Secure Server
$(SERVER_APP): $(SERVER_MAIN_OBJS) $(SERVER_CORE_OBJS) $(LIB_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SERVER_MAIN_OBJS) $(SERVER_CORE_OBJS) $(LDFLAGS)
	@echo "Secure Server $@ build success."

# 2. Insecure Server
$(INSECURE_APP): $(INSECURE_MAIN_OBJS) $(SERVER_CORE_OBJS) $(LIB_COMMON)
	$(CC) $(CFLAGS) -o $@ $(INSECURE_MAIN_OBJS) $(SERVER_CORE_OBJS) $(LDFLAGS)
	@echo "Insecure Server $@ build success."

# 3. Client Apps
$(CLIENT_APP): src/client/single_client.o $(CLIENT_CORE_OBJS) $(LIB_COMMON)
	$(CC) $(CFLAGS) -o $@ src/client/single_client.o $(CLIENT_CORE_OBJS) $(LDFLAGS)

$(STRESS_APP): src/client/stress_client.o $(CLIENT_CORE_OBJS) $(LIB_COMMON)
	$(CC) $(CFLAGS) -o $@ src/client/stress_client.o $(CLIENT_CORE_OBJS) $(LDFLAGS)

$(MALICIOUS_APP): src/client/malicious_client.o $(CLIENT_CORE_OBJS) $(LIB_COMMON)
	$(CC) $(CFLAGS) -o $@ src/client/malicious_client.o $(CLIENT_CORE_OBJS) $(LDFLAGS)

# 4. Dump Utility
$(DUMP_APP): dump_dat.c $(LIB_COMMON)
	$(CC) $(CFLAGS) -o $@ dump_dat.c $(LDFLAGS)

# Compile Rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_APP) $(INSECURE_APP) $(CLIENT_APP) $(STRESS_APP) $(MALICIOUS_APP) $(DUMP_APP)
	rm -f src/common/*.o src/server/*.o src/client/*.o
	rm -rf lib
	rm -f server.dat 
	@echo "Cleaned up build artifacts."