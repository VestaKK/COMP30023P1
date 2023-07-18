#define IMPLEMENTS_REAL_PROCESS

#include "defines.h"
#include "process_manager.h"

int main(int argc, char* argv[]) {
    uint32_t quantum = 0;
    char filename[256] = {};
    SCHEDULER_TYPE scheduler_type = 0;
    MEMORY_STRATEGY memory_strategy = 0;
    process_manager manager = NULL;
    
    // Process option flags
    int32_t flag;
    while( (flag = getopt(argc, argv, "f:s:m:q:")) != -1) {
        switch(flag) {
            case('f'):
                strcpy(filename, optarg);
                break;
            case('s'):
                scheduler_type = strcmp("SJF", optarg) == 0 ? SJF : RR;
                break;
            case('m'):
                memory_strategy = strcmp("infinite", optarg) == 0 ? INFINITE : BEST_FIT;
                break;
            case('q'):
                char* tmp_string;
                quantum = strtol(optarg, &tmp_string, 10);
                break;
            case('?'):
                fprintf(stderr, "unknown option: %c\n", optopt);
                break;
            default:
                abort();
                break;
        }
    }

    // Print out optional arguments, if there are any
    for(; optind < argc; optind++){     
        printf("extra arguments: %s\n", argv[optind]); 
    }

    // Initialise Process Manager
    process_manager_initialise(&manager, scheduler_type, memory_strategy);

    // Extract data about each program from file
    // and add them to the process manager
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Could not open file %s\n", filename);
        return 0;
    }
    while(TRUE) {
        program new_program = {};
        if (fscanf(fp, "%u %s %u %hu\n", 
                    &new_program.time_arrived, 
                    new_program.name, 
                    &new_program.service_time, 
                    &new_program.memory_required) == EOF) 
        break;
        
        program_add(manager, &new_program);
    }
    fclose(fp);
    fp = NULL;

    // Print some debug message if we're debugging
    debug_print_programs(manager);
    
    // Execution loop
    while(!should_terminate(manager)) {
        check_pending(manager);
        if (!keep_process_running(manager)) {
            switch_process(manager);
        }
        update(manager, quantum);
    }

    // Destroy and free used state
    process_manager_destroy(&manager);
    return 0;
}
