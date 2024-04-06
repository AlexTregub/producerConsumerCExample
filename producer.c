///// Alex Tregub
///// CS33211-001
///// Producer-Consumer Implementation Example
///// ===========
///// Producer v1.1.2.1
///// Producer-side implementation of Producer-Consumer problem. Debug done via
/////     printf. Sample data is in the form of char arrays of fixed length
/////     with random data generated each time. Fixed amount of attempts to
/////     'output' to shared data array before exit.
///// Designed to be run with ONLY 1 producer and ONLY 1 consumer. Running more
/////     may cause undesired behavior / processes hanging.
///// ===========
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>



/// Program configuration
#define MEM_BRIDGE_NAME "PROD_CONS_EG_BRIDGE" // Name of shared memory file (in /dev/shm on linux)
#define SYNC_VAL 10 // Integer value used to validate state of shared memory
#define SHARED_DATA_ARRAY_LENGTH 2 // As defined by assignment parameters
#define SHARED_MEM_ACCESSES 20 // Max amount of times producer will attempt to add data to shared data array
#define RANDOM_STRING_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyz1234567890" // Allowed characters for random sample strings - alpha-numeric here
#define RANDOM_STRING_LENGTH 10 // Number of allowed characters in each sample string (actual char array length is n+1, as \0 terminating char needed)
#define RNG_SEED time(NULL) // Seed used for random sample strings and wait times
#define MAX_WAIT_TIME 3 // Maximum number of seconds producer will wait before attempting to put more data in the shared data array (will be randomly determined each time)
#define EXTERNAL_EXEC_STATE_CHECKS 3 // Number of MAX_WAIT_TIME's program will wait before assuming other program has failed
#define TARGET_DATA_OUTPUTS 10 // Maximum amount of objects producer will place into shared meemory (producer may place less if it is unable to place more)

#define TRUE 1 // Int constant used insead of a true bool (must be 1)
#define FALSE 0 // Int constant used instead of a false bool (must be 0)
struct SHARED_MEM_STRUCT {  // Structure to be used in shared memory
    int syncVal; // Used to validate state of shared memory
    sem_t syncAccess; // Used to force atomic access to shared memory after initialization

    char sharedData[SHARED_DATA_ARRAY_LENGTH][RANDOM_STRING_LENGTH+1]; // 2-d char array to store two 'objects' with each object being a fixed length null-terminated string 

    int inPos; // Index of object producer accessing 
    int outPos; // Index of object consumer acceessing
    int isFull; // Allows overlap of indexes for full array usage
};



/// Main runtime
int main() {
    printf("Producer (v1.1.2.1) - Current configuration: MEM_BRIDGE_NAME = '%s', SYNC_VAL = '%i', SHARED_DATA_ARRAY_LENGTH = '%i', SHARED_MEM_ACCESSES = '%i', RANDOM_STRING_ALLOWED_CHARS = '%s', RANDOM_STRING_LENGTH = '%i', RNG_SEED = '%li', MAX_WAIT_TIME = '%i', EXTERNAL_EXEC_STATE_CHECKS = '%i', TARGET_DATA_OUTPUTS = '%i'. \n",MEM_BRIDGE_NAME,SYNC_VAL,SHARED_DATA_ARRAY_LENGTH,SHARED_MEM_ACCESSES,RANDOM_STRING_ALLOWED_CHARS,RANDOM_STRING_LENGTH,RNG_SEED,MAX_WAIT_TIME,EXTERNAL_EXEC_STATE_CHECKS,TARGET_DATA_OUTPUTS);
    printf("Producer (v1.1.2.1) - initializing memory...\n");
    srand(RNG_SEED); // Sets seed for random sequence generation

    // Open, resize, and map shared memory (cleanup memory if exists and not in use, else exit)
    int sharedMemFd = shm_open(MEM_BRIDGE_NAME, O_RDWR, S_IRUSR | S_IWUSR); // Attempts to ONLY open 
    if (sharedMemFd != -1) { // Checks if memory already exists (will attempt to cleanup shared region, and recreate from scratch)
        struct SHARED_MEM_STRUCT *tempMem = mmap(NULL, sizeof(struct SHARED_MEM_STRUCT), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemFd, 0); // Maps ptr to existing shared region
        tempMem->syncVal = SYNC_VAL + 1; // Signal kill whatever processes are connected (SYNC_VAL+2 will be set if other program responds)
        
        for (int i = 0; i < EXTERNAL_EXEC_STATE_CHECKS; ++i) {
            if (tempMem->syncVal == SYNC_VAL + 2) { // Another program has responded, and thus is still using shared memory. This program will exit to prevent issues
                tempMem->syncVal = SYNC_VAL + 3; // Acknowledge reciept of response
                munmap(tempMem,sizeof(struct SHARED_MEM_STRUCT)); // Unmap pointer from shared memory
                close(sharedMemFd); // Close file descriptor
                shm_unlink(MEM_BRIDGE_NAME); // Disconnect from shared memory
                return 1; // Exit
            }
            //tempMem->syncVal = SYNC_VAL + 1; // Reset check flag incase it has been cleared
            printf("Producer - Shared memory exists, checking for other running program... (every %i secs, %i times)\n",MAX_WAIT_TIME,EXTERNAL_EXEC_STATE_CHECKS);
            sleep(MAX_WAIT_TIME); 
        }

        munmap(tempMem,sizeof(struct SHARED_MEM_STRUCT)); // Unmap ptr to testing memory, will proceed with memory allocation as normal.
    } else { // If shared memory does not exist, proceed with normal initialization
        sharedMemFd = shm_open(MEM_BRIDGE_NAME, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR); // Only CREATES shared memory. Returns file descriptor if allowed
        if (sharedMemFd == -1) { // If unable to create memory region
            printf("Producer - Unable to create shared memory region. \n");
            return 1;
        }
    }
    if (ftruncate(sharedMemFd, sizeof(struct SHARED_MEM_STRUCT)) == -1) { // Attempts to resize shared memory region to size of common struct. Exits if fails.
        printf("Producer - Unable to resize shared memory region to needed size.\n");
        shm_unlink(MEM_BRIDGE_NAME); // Closes shared memory region previously created
        return 1;
    }
    struct SHARED_MEM_STRUCT *sharedMem = mmap(NULL, sizeof(struct SHARED_MEM_STRUCT), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemFd, 0); // Maps shared memory region to local pointer

    // Initialize shared memory
    sem_init(&sharedMem->syncAccess,1,0); // Initialize semaphore as shared between processes, and in the locked state
    sharedMem->inPos = 0; // Initialize pointers + data array state 
    sharedMem->outPos = 0;
    sharedMem->isFull = FALSE;
    for (int i = 0; i < SHARED_DATA_ARRAY_LENGTH; ++i) { // Initialize data array 
        for (int j = 0; j < RANDOM_STRING_LENGTH; ++j) {
            sharedMem->sharedData[i][j] = ' '; // Initialize strings with spaces to be empty
        }
        sharedMem->sharedData[i][RANDOM_STRING_LENGTH] = '\0'; 
    }
    sharedMem->syncVal = SYNC_VAL; // Signal to consumer (if/when run, that memory is now setup)

    printf("Producer - Data ["); // Output current state of sharedData
    for (int i = 0; i < SHARED_DATA_ARRAY_LENGTH; ++i) { // Initialize data array 
        printf("\"");
        for (int j = 0; j < RANDOM_STRING_LENGTH; ++j) { printf("%c",sharedMem->sharedData[i][j]); } // Output each valid char
        printf("\"");
        if (i != SHARED_DATA_ARRAY_LENGTH - 1) { printf(","); } // Output seperator when needed
    }
    printf("]\n");
    sem_post(&sharedMem->syncAccess); // Release shared memory as initialization complete

    // Attempt to add random data to array SHARED_MEM_ACCESSES amount of times
    int dataOutputCount = 0; // Counter to be used for limiting outputs to memory
    for (int i = 0; i < SHARED_MEM_ACCESSES; ++i) { 
        if (sharedMem->syncVal == SYNC_VAL + 1) { // Checks if any other programs are requesting current execution state
            sharedMem->syncVal = SYNC_VAL + 2; // Signal existance
            while (sharedMem->syncVal != SYNC_VAL + 3) { // Await until other program has acknowledged existence
                printf("Producer - Another process is checking for other running programs. Waiting until it exits... (%i secs)\n",MAX_WAIT_TIME);
                sleep(MAX_WAIT_TIME);
            }
        }

        sem_wait(&sharedMem->syncAccess); // Await exclusive access to shared memory

        int nextWaitTime = rand() % (MAX_WAIT_TIME+1); // Produce values between 0 and MAX_WAIT_TIME
        if ((sharedMem->inPos == sharedMem->outPos) & sharedMem->isFull) { // Check if shared array full
            printf("Producer - Shared array full. Sleeping %i secs...\n",nextWaitTime);
        }
        else { // Array is not full, generate random string and increment inPos
            for (int i = 0; i < RANDOM_STRING_LENGTH; ++i) {
                int c = rand() % (sizeof(RANDOM_STRING_ALLOWED_CHARS)-1); // Generate random position in allowed char list
                sharedMem->sharedData[sharedMem->inPos][i] = RANDOM_STRING_ALLOWED_CHARS[c]; // Set each character in position 
            }
            sharedMem->sharedData[sharedMem->inPos][RANDOM_STRING_LENGTH] = '\0'; // Ensure null end char ALWAYS set

            sharedMem->inPos = (sharedMem->inPos + 1) % SHARED_DATA_ARRAY_LENGTH; // Ensure inPos wraps around at end of array
            if (sharedMem->inPos == sharedMem->outPos) { sharedMem->isFull = TRUE; } // If inPos and outPos collide, array must be full, thus set 'flag'
            dataOutputCount = dataOutputCount + 1; // Increments outputs counter

            printf("Producer - Data ["); // Output current state of sharedData
            for (int j = 0; j < SHARED_DATA_ARRAY_LENGTH; ++j) { // Initialize data array 
                printf("\"");
                for (int k = 0; k < RANDOM_STRING_LENGTH; ++k) { printf("%c",sharedMem->sharedData[j][k]); } // Output each valid char
                printf("\"");
                if (j != SHARED_DATA_ARRAY_LENGTH - 1) { printf(","); } // Output seperator when needed
            }
            printf("] - Producer will sleep for %i secs...\n",nextWaitTime);
        }

        sem_post(&sharedMem->syncAccess); // Release access to other processes
        if (dataOutputCount == TARGET_DATA_OUTPUTS) { break; } // Exit at target output count
        sleep(nextWaitTime); // Delay next access attempt
    }
    printf("Producer - Produced %i objects to shared data array.\n",dataOutputCount);

    // Wait for consumer to signal completion/close
    int timesSyncChecked = 0;
    while (sharedMem->syncVal != SYNC_VAL - 1) {
        printf("Producer - waiting for signal to cleanup shared memory... (sleeping for %i secs)\n",MAX_WAIT_TIME);
        
        if (timesSyncChecked == EXTERNAL_EXEC_STATE_CHECKS) { // Reached limit of retrys for exiting. Checking if consumer is awake or not.
            timesSyncChecked = 0; // Reset check counter if this state reached

            sharedMem->syncVal = SYNC_VAL + 1; // Request check for alive state
            int brokenState = TRUE; // Var to check for no-response
            for (int i = 0; i < EXTERNAL_EXEC_STATE_CHECKS; ++i) { // Wait for response
                printf("Producer - checking other process 'alive' state... (sleeping for %i secs)\n",MAX_WAIT_TIME);
                if (sharedMem->syncVal == SYNC_VAL + 2) {
                    brokenState = FALSE; // Set that consumer is still alive
                    sharedMem->syncVal = SYNC_VAL + 3; // Confirm 
                    break;
                }
                sleep(MAX_WAIT_TIME);
            }

            if (brokenState) { break; } // Consumer has likely died without signaling exit
            else {
                sharedMem->syncVal = SYNC_VAL + 3; // Signal acknowledged, continue looping
            }
        }
        sleep(MAX_WAIT_TIME); // Sleeps for MAX_WAIT_TIME (to avoid spamming debug output with garbage)
        timesSyncChecked = timesSyncChecked + 1; // Increments check
    }

    // Cleanup shared memory
    sem_wait(&sharedMem->syncAccess); // (After sync val gone to SYNC_VAL-1, wait for consumer to stop using semaphore before proceeding)
    sem_destroy(&sharedMem->syncAccess); // Destroys semaphore. Calls to wait will hang. (Thus we ensured that consumer has shutdown before doing this)
    munmap(sharedMem,sizeof(struct SHARED_MEM_STRUCT)); // Unmap pointer from shared memory
    close(sharedMemFd); // Closes file descriptor
    shm_unlink(MEM_BRIDGE_NAME); // Sets that bridge is no longer in use by this process (should trigger the shared memory file to be deleted - as this should be the last process to quit)
    
    printf("Producer - runtime ended.\n");
    return 0;
}