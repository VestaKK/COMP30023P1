#include "process_manager.h"
#include "linked_list.h"

static uint32_t time = 0;

/** Process Manager
 * main should not be aware of what a process is, so declarations for
 * process related operations and data are located here instead of in 
 * process_manager.h. This also means that a lot of state is statically defined 
 * instead of being located in the definition of process_manager_t (lists, current running
 * process etc.)
*/

typedef enum process_state {
    READY,
    RUNNING,
    FINISHED
} PROCESS_STATE;

typedef struct memory_block {
    uint32_t index;
    uint32_t size;
} memory_block;

typedef struct process {
    program* pProgram;
    uint32_t run_time;
    PROCESS_STATE state;
    memory_block* pBlock;
    pid_t child_pid;        
    int P2Cfd[2];   // Parent to Child File Descriptors
    int C2Pfd[2];   // Child to Parent File Descriptors
    char sha_buf[SHA_HASH_SIZE + 1];
} process;

// Static process manager state
static uint8_t initialised = FALSE;
static process_manager_t instance = {};
static list* list_input = NULL; // Programs waiting to be subitted to the ready list
static list* list_active = NULL; // All ready, running and finished processes
static list* list_ready = NULL; // Processes ready to begin or resume execution
static process* pRunningProcess = NULL; // Current running process

// Runtime statistics
static float max_overhead = 0;
static float avg_overhead = 0;
static float turnaround_time = 0;

/**
 * @brief
 * Prints a log message of a process based on the process' state.
 * @param pProcess pointer to an active process
*/
static void process_log(process* pProcess);

/**
 * @brief
 * Attempts to create a process based on the provided program.
 * @param pProgram pointer to Program
 * @return
 * If the process manager can allocate enough memory to run the process
 * the function returns a heap allocated process pointer. Otherwise the
 * function returns NULL.
*/
static process* process_try_create(program* pProgram);

/**
 * @brief
 * Pushes provided process to the end of the ready list.
 * @param pProcess pointer to an ACTIVE process
*/
static void process_submit_ready(process* pProcess);

/**
 * @brief
 * Sets a ready process to run for the first time, or continues a
 * process that was previously suspended.
 * @param pProcess pointer to an ACTIVE process
*/
static void process_run(process* pProcess);

/**
 * @brief
 * Signals to a running process to stop execution.
 * @param pProcess pointer to a running process
*/
static void process_suspend(process* pProcess);

/**
 * @brief
 * Signals to a suspended process to resume execution, or to a
 * running process to continue execution.
 * @param pProcess pointer to a suspended process
*/
static void process_continue(process* pProcess);

/**
 * @brief
 * Signals to a running process to terminate. Also closes pipes between
 * the parent process and its child, not before retrieving a sha hash string
 * from the terminated process.
 * @param pProcess pointer to a running process
*/
static void process_terminate(process* pProcess);


/** Memory Allocator
 * The implementation of allocator is specific to the process manager, which 
 * is why there is no header file called allocator.h or something like that. 
 * If I was to do this again I'd probably make this more generic but for now
 * this will suffice
*/

typedef struct memory_allocator {
    list* free_list;
    MEMORY_STRATEGY strategy;
} memory_allocator;

static memory_allocator allocator = {};

/**
 * @brief
 * Initialises allocator based on the provided memory strategy.
 *  @param strategy desired memory strategy
*/
static void allocator_initialise(MEMORY_STRATEGY strategy);

/**
 * @brief
 * Deinitilises state used by allocator
*/
static void allocator_destroy();

/**
 * @brief
 * Allocator attempts to find a block of memory large enough to fit
 * size MB of memory
 * @param size desired size of memory block in MB
 * @return
 * If the allocator can find a sufficiently sized block of memory, 
 * this function returns a heap allocated pointer to a block of memory. 
 * Otherwise this function returns NULL.
*/
static memory_block* allocator_find_best_fit(uint32_t size);

/**
 * @brief
 * Takes memory block used by a process and inserts it back into the
 * free list. Also merges any adjacent memory blocks in the free list
 * @param pProcess pointer to a FINISHED process
*/
static void allocater_free_memory(process* pProcess);

// Task 1 and 2

static node* shortest_job_first(list* pList);
static node* round_robin(list* pList);

// Miscellaneous

static uint32_t big_endian(uint32_t integer);
static int32_t mem_block_cmp(void* pData1, void* pData2);
static void print_final_stats();

// Wrapper functions

static void fd_write(int fd, void* pBuf, size_t nbytes);
static void fd_read(int fd, void* pBuf, size_t nbytes);

void process_manager_initialise(
    process_manager* pManager, 
    SCHEDULER_TYPE type, 
    MEMORY_STRATEGY strategy) {

    // Make sure manager is not already initialised
    assert(!initialised);

    debug_log("\nINITIALISING PROCESS MANAGER\n\n");

    // Initialise variables needed by the process manager
    instance.pPrograms = NULL;
    instance.program_count = 0;
    instance.type = type;
    instance.pending_count = 0;
    initialised = TRUE;

    list_active = list_create(TRUE); // List nodes will carry heap allocated memory addresses to processes
    list_input = list_create(FALSE); // References programs in instance.pPrograms[]
    list_ready = list_create(FALSE); // References processes in list_active
    allocator_initialise(strategy);

    *pManager = &instance; // Pass instance handle over to user
}

void process_manager_destroy(process_manager* pManager) {
    assert(initialised);
    assert(*pManager == &instance);

    print_final_stats();
    debug_log("\nDESTROYING PROCESS MANAGER\n\n");

    allocator_destroy();
    list_destroy(&list_ready);
    list_destroy(&list_input);
    list_destroy(&list_active); // All process handles are freed after this point
    memset(instance.pPrograms, 0, sizeof(program)*instance.program_count);
    FREE(instance.pPrograms);
    memset(&instance, 0, sizeof(process_manager_t));

    *pManager = NULL; // User should not be able to use destroyed process manager
}

void program_add(process_manager manager, program* pProgram) {
    assert(initialised);
    assert(manager == &instance);

    // Add process to manager
    if (instance.program_count == 0) {
        instance.program_count++;
        instance.pPrograms = malloc(sizeof(program));
        instance.pPrograms[0] = *pProgram;
    } else {
        instance.program_count++;
        instance.pPrograms = realloc(instance.pPrograms, sizeof(program)*instance.program_count);
        instance.pPrograms[instance.program_count - 1] = *pProgram;
    }
   debug_log("Process %s added to process manager\n", pProgram->name);
}

void update(process_manager manager, uint32_t delta_time) {
    assert(initialised);
    assert(manager == &instance);

    // Update time
    time += delta_time;

    // If a running process exists, run it for one quantum
    if (pRunningProcess == NULL) 
        return;
    pRunningProcess->run_time += delta_time;

    if (pRunningProcess != NULL) {
        if (pRunningProcess->run_time >= pRunningProcess->pProgram->service_time) {
            instance.pending_count--;
            process_terminate(pRunningProcess);
            if (allocator.strategy == BEST_FIT) {
                allocater_free_memory(pRunningProcess);
            }
            process_log(pRunningProcess);
            pRunningProcess = NULL;
        }
    }
}

void check_pending(process_manager manager) {
    assert(initialised);
    assert(manager == &instance);

    debug_log("Checking pending processes\n");

    static uint32_t index = 0;

    // Check if the next program can be inserted into the input list
    while(index < instance.program_count) {
        program* pProgram = &instance.pPrograms[index];

        if (pProgram->time_arrived > time) 
            break;
        list_insert_tail(list_input, pProgram);
        instance.pending_count++;
        index++;
    }

    // return if there are no programs in the input list
    if(list_input->head == NULL) 
        return;

    node* pNode = list_input->head;

    // Iterate through input list and check if any program can
    // be submitted to the ready list
    while (pNode != NULL) {
        program* pProgram = pNode->data;
        process* pProcess = process_try_create(pProgram);

        // If an active process could be generated, submit this
        // to the active and ready lists
        if (pProcess != NULL) {
            list_insert_tail(list_active, pProcess);
            process_submit_ready(pProcess);
            if (pProcess->pBlock != NULL) {
                process_log(pProcess);
            }
            pNode = list_pop_node(list_input, pNode);
            continue;
        }

        pNode = pNode->next;
    }
}

bool keep_process_running(process_manager manager) {
    assert(initialised);
    assert(manager == &instance);

    if (pRunningProcess == NULL) 
        return FALSE;
    
    switch(instance.type) 
    {
    case(SJF):
        process_continue(pRunningProcess);
        break;
    case(RR):
        if (list_ready->head != NULL) {
            process_suspend(pRunningProcess);
            process_submit_ready(pRunningProcess);
            pRunningProcess = NULL;
            return FALSE;
        } else {
            process_continue(pRunningProcess);
        }
        break;
    }

    return TRUE;
}

void switch_process(process_manager manager) {
    assert(initialised);
    assert(manager == &instance);

    node* pReady = NULL;

    // Find valid ready process to run
    switch(instance.type)
    {
        case(SJF):
            pReady = shortest_job_first(list_ready);
            break;
        case(RR):
            pReady = round_robin(list_ready);
            break;
    }

    // Try to switch the ready and running processes
    if (pReady == NULL) {
        return;
    } else {
        pRunningProcess = pReady->data;
        process_run(pRunningProcess);
        list_pop_node(list_ready, pReady);
    }
}

static void allocator_initialise(MEMORY_STRATEGY strategy) {
    switch(strategy) 
    {
        case(INFINITE):
            allocator.strategy = strategy;
            allocator.free_list = NULL;
            break;
        case(BEST_FIT):
            allocator.strategy = strategy;
            allocator.free_list = list_create(TRUE);

            // Allocator begins with one 2048MB size block of memory
            memory_block* pBlock = malloc(sizeof(memory_block));
            pBlock->index = 0;
            pBlock->size = BUFFER_SIZE;
            list_insert_head(allocator.free_list, pBlock);
            break;
    }
}

static void allocator_destroy() {
    switch(allocator.strategy) 
    {
    case(INFINITE):
        allocator.free_list = NULL;
        allocator.strategy = 0;
        break;
    case(BEST_FIT):
        list_destroy(&allocator.free_list); 
        allocator.strategy = 0;
        break;
    }
}

static void allocater_free_memory(process* pProcess) {
    assert(pProcess != NULL);

    // Insert memory block back into the free list
    list_insert_sorted(allocator.free_list, pProcess->pBlock, mem_block_cmp);
    pProcess->pBlock = NULL;

    // Iterate through list and try to merge adjacent blocks of memory
    node* pNode = allocator.free_list->head;
    while(pNode != NULL ) {
        if (pNode->next == NULL) break;
        memory_block* pBlock1 = pNode->data;
        memory_block* pBlock2 = pNode->next->data;

        // If blocks of memory are adjacent, they should be merged
        if (pBlock1->index + pBlock1->size == pBlock2->index) {
            pBlock2->index -= pBlock1->size;
            pBlock2->size += pBlock1->size;
            pNode = list_pop_node(allocator.free_list, pNode);
            continue;
        }

        pNode = pNode->next;
    }
}

static memory_block* allocator_find_best_fit(uint32_t size) {
    if (allocator.strategy == INFINITE) 
        return NULL;
    assert(allocator.free_list->head != NULL); // free list should NEVER be empty
    
    node* pNode = allocator.free_list->head;
    memory_block* pChosenBlock = NULL;
    uint32_t min_gap = 0;

    // Iterate through list and try to find sufficiently
    // size block of memory
    while(pNode != NULL) {
        memory_block* pBlock = pNode->data;
        if (pBlock->size < size) {
            pNode = pNode->next;
            continue;
        }
        uint32_t gap = pBlock->size - size;
        if (pChosenBlock == NULL || 
            min_gap > gap) {
            pChosenBlock = pBlock;
            min_gap = gap;
        }
        pNode = pNode->next;
    }

    // No block was found, so we return NULL
    if (pChosenBlock == NULL) 
        return NULL;

    // Create a memory block to hand over to a process
    memory_block* pAllocation = malloc(sizeof(memory_block));
    pAllocation->index = pChosenBlock->index;
    pAllocation->size = size;

    // Adjust size of existing block of memory
    pChosenBlock->index += size;
    pChosenBlock->size -= size;

    return pAllocation;
}

static void process_terminate(process* pProcess) {
    assert(pProcess != NULL);

    // Send current time to child process
    uint32_t be_time = big_endian(time);
    fd_write(pProcess->P2Cfd[PIPE_WRITE], &be_time, sizeof(uint32_t));
    kill(pProcess->child_pid, SIGTERM);

    // Read sha hash value from process
    fd_read(pProcess->C2Pfd[PIPE_READ], pProcess->sha_buf, SHA_HASH_SIZE);
    pProcess->sha_buf[SHA_HASH_SIZE] = 0;

    // Close remaining pipes
    close(pProcess->P2Cfd[PIPE_WRITE]);
    close(pProcess->C2Pfd[PIPE_READ]);
    pProcess->state = FINISHED;

    // Do some stat stuff
    uint32_t process_turnaround_time = time - pProcess->pProgram->time_arrived;
    float process_time_overhead = process_turnaround_time / (float)pProcess->pProgram->service_time;
    turnaround_time += process_turnaround_time;
    avg_overhead += process_time_overhead;
    if (max_overhead == 0 || max_overhead < process_time_overhead) {
        max_overhead = process_time_overhead;
    }
} 

static void fd_write(int fd, void* pBuf, size_t nbytes) {
    uint8_t* pBuffer = pBuf;
    size_t bytes_remaining = nbytes;

    // Make sure we write N bytes to the given file descriptor
    while(bytes_remaining > 0) {
        ssize_t bytes_written = write(fd, pBuffer, bytes_remaining);
        
        if(bytes_written == -1) {
            err(EXIT_FAILURE, "write");
        }

        bytes_remaining -= bytes_written;
        pBuffer += bytes_written;
    } 
}

static void fd_read(int fd, void* pBuf, size_t nbytes) {
    uint8_t* pBuffer = pBuf;
    size_t bytes_remaining = nbytes;

    // Make sure we read N bytes from the given file descriptor
    while(bytes_remaining > 0) {
        ssize_t bytes_read = read(fd, pBuffer, bytes_remaining);
        if (bytes_read == -1) {
            err(EXIT_FAILURE, "read");
        }
        bytes_remaining -= bytes_read;
        pBuffer += bytes_read;
    }
}


static node* shortest_job_first(list* pList) {
    assert(pList != NULL);

    node* pNode = pList->head;
    node* pChosen = NULL;
    uint32_t min_time = 0;
    
    // Iterate through list and find process that has
    // the shortest service time
    while(pNode != NULL) {
        process* pProcess = pNode->data;
        uint32_t service_time = pProcess->pProgram->service_time;

        // Choose first process that appears
        if (pChosen == NULL) {
            min_time = service_time;
            pChosen = pNode;
            pNode = pNode->next;
            continue;
        }

        process* pChosenProcess = pChosen->data;

        // Otherwise compare and assign accordingly
        if (min_time == service_time) {
            if (pChosenProcess->pProgram->time_arrived == pProcess->pProgram->time_arrived) {
                if(strcmp(pChosenProcess->pProgram->name, pProcess->pProgram->name) > 0) {
                    pChosen = pNode;
                    min_time = service_time;
                }
            } else if (pChosenProcess->pProgram->time_arrived > pProcess->pProgram->time_arrived) {
                pChosen = pNode;
                min_time = service_time;    
            }
        }
        else if (min_time > service_time) {
            pChosen = pNode;
            min_time = service_time;
        }

        pNode = pNode->next;
    }

    return pChosen;
}

static node* round_robin(list* pList) {
    assert(pList != NULL);
    return pList->head;
}

uint32_t big_endian(uint32_t integer) {

    // Check if system is big endian
    // Ref: https://www.youtube.com/watch?v=RvFRCDoj6JI
    uint32_t x = 1;
    char* c = (char*)&x;
    if (!*c) 
        return integer;

    // Otherwise we convert the integer
    // Ref: https://man7.org/linux/man-pages/man3/bswap.3.html
    return bswap_32(integer);
}

static void process_run(process* pProcess) {
    assert(pProcess != NULL);

    pProcess->state = RUNNING;
    process_log(pProcess);

    // Processes that are suspended should resume
    if (pProcess->child_pid != PID_NULL_HANDLE) {
        process_continue(pProcess);
        return;
    } 

    // Set up pipes between parent and child process 
    pipe(pProcess->P2Cfd);
    pipe(pProcess->C2Pfd);

    if ((pProcess->child_pid = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } 
    
    // Child Process
    else if (pProcess->child_pid == 0) {
        
        // Close unused ends of pipes
        close(pProcess->P2Cfd[PIPE_WRITE]);
        close(pProcess->C2Pfd[PIPE_READ]);

        // Set stdin and stdout of child process to the parent process'
        // file descriptors
        dup2(pProcess->P2Cfd[PIPE_READ], STDIN_FILENO);
        dup2(pProcess->C2Pfd[PIPE_WRITE], STDOUT_FILENO);

        // Start execution of child process
        execl("./process", "process", pProcess->pProgram->name, (char*)NULL);

        // execl() should not return anything, so we print an error message
        // and exit the program
        printf("Something went wrong with execl()!: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } 
    
    // Parent Process
    else { 

        // Close unused ends of pipes
        close(pProcess->P2Cfd[PIPE_READ]);
        close(pProcess->C2Pfd[PIPE_WRITE]);

        // Send current time to child process
        uint32_t be_time = big_endian(time);
        uint8_t last_byte_sent = ((uint8_t*)&be_time)[3];
        fd_write(pProcess->P2Cfd[PIPE_WRITE], &be_time, sizeof(uint32_t));

        // Verify information was sent properly
        uint8_t verify_byte = 0;
        fd_read(pProcess->C2Pfd[PIPE_READ], &verify_byte, sizeof(uint8_t));
        if (verify_byte != last_byte_sent) {
            exit(EXIT_FAILURE);
        }
    }   
}

static void process_suspend(process* pProcess) {
    assert(pProcess != NULL);

    int wstatus;
    pid_t w;

    debug_log("Suspending execution of %s\n", pProcess->pProgram->name);

    // Send current time to child process
    uint32_t be_time = big_endian(time);
    fd_write(pProcess->P2Cfd[PIPE_WRITE], &be_time, sizeof(uint32_t));

    // Signal child process to suspend execution
    kill(pProcess->child_pid, SIGTSTP);

    // Wait for process to stop execution
    // Ref: https://man7.org/linux/man-pages/man2/wait.2.html
    do {
        w = waitpid(pProcess->child_pid, &wstatus, WUNTRACED);

        if (w == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFSTOPPED(wstatus)) {
           debug_log("stopped by signal %d\n", WSTOPSIG(wstatus));
            break;
        }
    }   while(!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
}

static void process_continue(process* pProcess) {
    assert(pProcess != NULL);

    debug_log("Continuing execution of %s\n", pProcess->pProgram->name);

    // Send current time to child process
    uint32_t be_time = big_endian(time);
    uint8_t last_byte_sent = ((uint8_t*)&be_time)[3];
    fd_write(pProcess->P2Cfd[PIPE_WRITE], &be_time, sizeof(uint32_t));

    // Signal child process to continue execution
    kill(pProcess->child_pid, SIGCONT);

    // Verify information was properly sent
    uint8_t verify_byte = 0;
    fd_read(pProcess->C2Pfd[PIPE_READ], &verify_byte, sizeof(uint8_t));
    if (verify_byte != last_byte_sent) {
        exit(EXIT_FAILURE);
    }
}

bool should_terminate(process_manager manager) {
    assert(initialised);
    assert(manager == &instance);

    debug_log("%d, LOOP START\n", time);

    // If the process manager has 0 added programs we stop 
    // program execution
    if (list_active->head == NULL && initialised) {
        return instance.program_count > 0 ? FALSE : TRUE;
    }

    uint32_t count = 0;
    bool finished = TRUE;

    // Check all programs added to the process manager have executed 
    // and are finished
    node* pNode = list_active->head;
    while(pNode != NULL) {
        process* pProcess = pNode->data;
        if (pProcess->state != FINISHED) {
            finished = FALSE;
        }
        pNode = pNode->next;
        count++;
    }
    
    return finished && count == instance.program_count ? TRUE : FALSE;
}

static process* process_try_create(program* pProgram) {
    assert(pProgram != NULL);

    debug_log("Attempting to create %s \n", pProgram->name);

    // Try to allocate a block of memory for the program
    memory_block* pBlock = NULL;
    if (allocator.strategy == BEST_FIT) {
        if ((pBlock = allocator_find_best_fit(pProgram->memory_required)) == NULL) {
            debug_log("Allocation for %s unsuccessful\n", pProgram->name);
            return NULL;
        }
    }

    debug_log("Allocation for %s successful\n", pProgram->name);

    // Create process and give it to the process manager
    process* pProcess = malloc(sizeof(process));
    pProcess->pProgram = pProgram;
    pProcess->child_pid = PID_NULL_HANDLE;
    pProcess->pBlock = pBlock;
    pProcess->run_time = 0;
    return pProcess;
}

static void process_submit_ready(process* pProcess) {
    pProcess->state = READY;
    list_insert_tail(list_ready, pProcess);
}

static void process_log(process* pProcess) {
    switch(pProcess->state) 
    {
        case(READY):
            printf("%d,READY,process_name=%s,assigned_at=%d\n",
            time,
            pProcess->pProgram->name,
            pProcess->pBlock->index);
            break;
        case(RUNNING):
            printf("%d,RUNNING,process_name=%s,remaining_time=%d\n",
            time,
            pProcess->pProgram->name,
            pProcess->pProgram->service_time - pProcess->run_time);
            break;
        case(FINISHED):
            printf("%d,FINISHED,process_name=%s,proc_remaining=%d\n",
            time,
            pProcess->pProgram->name,
            instance.pending_count);
            printf("%d,FINISHED-PROCESS,process_name=%s,sha=%s\n",
            time,
            pProcess->pProgram->name,
            pProcess->sha_buf);
            break;
    }
}

static int32_t mem_block_cmp(void* pData1, void* pData2) {
    memory_block* pBlock1 = pData1;
    memory_block* pBlock2 = pData2;
    if (pBlock1 == NULL) return -1;
    if (pBlock2 == NULL) return 1;
    if (pBlock1->index > pBlock2->index) return 1;
    if (pBlock1->index <= pBlock2->index) return -1;
    return -1;
}

static void print_final_stats() {
    turnaround_time /= (float)instance.program_count;
    turnaround_time = ceilf(turnaround_time);
    avg_overhead /= (float)instance.program_count;
    
    printf("Turnaround time %u\n", (uint32_t)turnaround_time);
    printf("Time overhead %.2f %.2f\n", max_overhead, avg_overhead);
    printf("Makespan %u\n", time);
}

void debug_print_program(program* pProgram) {
    assert(pProgram != NULL);

    debug_log(LINE);
    debug_log(" [PROCESS]\n");
    debug_log(" NAME: %s\n", pProgram->name);
    debug_log(" HANDLE: %p\n", pProgram);
    debug_log(" TIME ARRIVED: %d\n", pProgram->time_arrived);
    debug_log(" SERVICE TIME: %d\n", pProgram->service_time);
    debug_log(" MEMORY REQUIRED: %d\n", pProgram->memory_required);
    debug_log(LINE);
}

void debug_print_programs(process_manager manager) {
    assert(initialised);
    assert(manager == &instance);

    debug_log("\n");
    debug_log(LINE);
    debug_log(" [PROCESS_MANAGER]\n");
    debug_log(" HANDLE: %p\n", &instance);
    debug_log(" PROCESS COUNT: %d\n", instance.program_count);
    for (uint32_t i=0; i<instance.program_count; i++) {
        debug_print_program(&instance.pPrograms[i]);
    }
    debug_log(LINE);
}