#ifndef __PROCESS_MANAGER_H__
#define __PROCESS_MANAGER_H__

#include "defines.h"

typedef enum memory_strategy {
    INFINITE,
    BEST_FIT
} MEMORY_STRATEGY;

typedef enum scheduler_type {
    SJF,
    RR,
} SCHEDULER_TYPE;

/**
 * @param name name of the program { Maximum of 8 characters }
 * @param time_arrived time program is ready to be allocated to the CPU
 * @param service_time total expected run-time of the program
 * @param memory_required  total memory required by the program during its run-time
*/
typedef struct program {
    char name[MAX_NAME_LEN + 1]; // Add one for the null-terminating character
    uint32_t time_arrived; 
    uint32_t service_time;
    uint16_t memory_required;
} program;

/**
 * @param pPrograms dynamically allocated array of programs
 * @param program_count number of programs added to process manager
 * @param pending_count number of programs in the input + active processes
 * @param type process manager's scheduler type
*/
typedef struct process_manager_t {
    program* pPrograms;
    uint32_t program_count;
    uint32_t pending_count;
    SCHEDULER_TYPE type;
} process_manager_t;

typedef process_manager_t* process_manager;

/**
 * @brief
 * Initialises process manager with a memory strategy and scheduler type.
 * Assigns value at pManager to this initialised process manager instance.
 * @param pManager pointer to where process manager handle will be stored
*/
void process_manager_initialise(
    process_manager* pManager, 
    SCHEDULER_TYPE type, 
    MEMORY_STRATEGY strategy);

/**
 * @brief
 * Destroys state used by the process manager during its lifetime, 
 * including zeroing available state and freeing all heap allocated memory.
 * Also sets value at pManager to NULL.
 * @param pManager pointer to a process manager handle
*/
void process_manager_destroy(process_manager* pManager);

/**
 * @brief
 * Adds a program to the the process manager
 * @param manager process manager handle
 * @param pProgram pointer to program
*/
void program_add(process_manager manager, program* pProgram);

/**
 * @param manager process manager handle
 * @return 
 * Whether or not the process manager should terminate.
*/
bool should_terminate(process_manager manager);

/**
 * @brief
 * Checks for incoming programs and pending processes in the process input queue.
 * @param manager process manager handle
*/
void check_pending(process_manager manager);

/**
 * @brief
 * If the process should be suspended, this function suspends the running process 
 * and submits the process to the end of the ready queue. Otherwise we continue
 * running the current running process.
 * @param manager process manager handle
 * @return
 * Whether or not the current running process should continue running
 * 
*/
bool keep_process_running(process_manager manager);

/**
 * @brief
 * Selects a new process to run based on the type of scheduler
 * assigned to the process manager at creation.
 * @param manager process manager handle
*/
void switch_process(process_manager manager);

/**
 * @brief
 * Updates simulation time of the process manager. Also updates run-time
 * of the current running process.
 * @param manager process manager handle
 * @param delta_time delta_time of execution loop
*/
void update(process_manager manager, uint32_t delta_time);

/**
 * @brief
 * Prints out debug information on a program's state. 
 * Need to define DEBUG during compilation of the program
 * for this function to do print anything
 * @param pProgram pointer to program.
*/
void debug_print_program(program* pProgram);

/**
 * @brief
 * Prints out debug information on a program manager's programs. 
 * Need to define DEBUG during compilation of the program
 * for this function to do anything.
 * @param manager process manager handle
*/
void debug_print_programs(process_manager manager);

#endif