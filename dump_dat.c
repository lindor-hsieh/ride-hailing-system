/* dump_dat.c */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "src/common/include/shared_data.h" 

#define DATA_FILE "server.dat"

int main() {
    int fd = open(DATA_FILE, O_RDONLY);
    if (fd == -1) {
        printf("Error: Could not open %s. Please ensure the server has run and shut down gracefully.\n", DATA_FILE);
        return 1;
    }

    SharedState state;
    // read
    if (read(fd, &state, sizeof(SharedState)) != sizeof(SharedState)) {
        printf("Error: File size mismatch. The save file might be corrupted or created by an incompatible version.\n");
        close(fd);
        return 1;
    }
    close(fd);

    printf("======================================\n");
    printf("        Server State Dump Report      \n");
    printf("======================================\n");
    printf("Total Requests Handled : %ld\n", state.total_requests_handled);
    printf("Total Success Requests : %ld\n", state.total_success_requests);
    printf("Total Revenue          : \033[1;32m$%ld\033[0m\n", state.total_revenue); 
    printf("Active Driver Count    : %d\n", state.driver_count);
    printf("--------------------------------------\n");
    printf("Driver List (First 5 Details):\n");
    
    int available_drivers = 0;
    int refueling_drivers = 0;
    int busy_drivers = 0;

    for (int i = 0; i < state.driver_count; i++) {
        const char *status_str;
        
        if (state.drivers[i].is_refueling) {
            status_str = "\033[1;34mREFUELING\033[0m"; // 藍色
            refueling_drivers++;
        } else if (state.drivers[i].is_available) {
            status_str = "\033[1;32mAVAILABLE\033[0m"; // 綠色
            available_drivers++;
        } else {
            status_str = "\033[1;31mBUSY\033[0m";      // 紅色
            busy_drivers++;
        }

        if (i < 5) {
            printf("  [%d] ID: %d, Status: %s, Rides: %d, Fuel: %d/10\n", 
                   i, 
                   state.drivers[i].driver_id, 
                   status_str,
                   state.drivers[i].rides_count,      
                   state.drivers[i].fuel
                   );
        }
    }
    if (state.driver_count > 5) printf("  ... (%d more drivers hidden)\n", state.driver_count - 5);
    printf("--------------------------------------\n");
    printf("Summary:\n");
    printf(" - AVAILABLE : %d\n", available_drivers);
    printf(" - BUSY      : %d\n", busy_drivers);
    printf(" - REFUELING : %d\n", refueling_drivers);
    printf("======================================\n");

    return 0;
}