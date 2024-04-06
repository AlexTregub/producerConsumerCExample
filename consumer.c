///// Alex Tregub
///// CS33211-001
///// Producer-Consumer Implementation Example
///// ===========
///// Consumer v1.1.2.1
///// Consumer-side implementation of Producer-Consumer problem. Debug done via
/////     printf. Sample data is in the form of char arrays of fixed length
/////     which are removed and printed by this program. Fixed amount of
/////     attempts to 'remove' shared data from array before exiting.
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
#define SHARED_MEM_ACCESSES 20 // Max amount of times consumer will attempt to remove data from shared data array
#define RANDOM_STRING_LENGTH 10 // Number of characters in each sample string (actual char array length is n+1, as \0 terminating char included)
#define RNG_SEED time(NULL) // Seed used for random wait times
#define MAX_WAIT_TIME 3 // Maximum number of seconds consumer will wait before attempting to remove more data from the shared data array (will be randomly determined each time)
#define EXTERNAL_EXEC_STATE_CHECKS 3 // Number of MAX_WAIT_TIME's program will wait before assuming other program has failed
#define TARGET_DATA_INPUTS 10 // Number of data inputs consumer will take before exitting (may be less if producer does not fill array quickly)

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
    printf("Croducer (v1.1.2.1) - Current configuration: MEM_BRIDGE_NAME = '%s', SYNC_VAL = '%i', SHARED_DATA_ARRAY_LENGTH = '%i', SHARED_MEM_ACCESSES = '%i', RANDOM_STRING_LENGTH = '%i', RNG_SEED = '%li', MAX_WAIT_TIME = '%i', EXTERNAL_EXEC_STATE_CHECKS = '%i', TARGET_DATA_INPUTS = '%i'. \n",MEM_BRIDGE_NAME,SYNC_VAL,SHARED_DATA_ARRAY_LENGTH,SHARED_MEM_ACCESSES,RANDOM_STRING_LENGTH,RNG_SEED,MAX_WAIT_TIME,EXTERNAL_EXEC_STATE_CHECKS,TARGET_DATA_INPUTS);
    printf("Consumer (v1.1.2.1) - connecting to shared memory...\n");
    srand(RNG_SEED); // Sets seed for random wait times

    // Open, map ptr to EXISTING shared memory, and wait until shared memory has been setup by producer
    int memFail = TRUE;
    int sharedMemFd = shm_open(MEM_BRIDGE_NAME, O_RDWR, S_IRUSR | S_IWUSR); // Only OPENS shared memory, if it does not exist, consumer will retry in MAX_WAIT_TIME seconds. Returns file descriptor
    for (int i = 0; i < EXTERNAL_EXEC_STATE_CHECKS; ++i) { // Will re-attempt to open shared memory if failed (incase Producer has not created memory yet)
        if (sharedMemFd != -1) { memFail = FALSE; break; } // If not failed, continue execution
        printf("Consumer - Failed to open shared memory. Retrying in %i secs...\n",MAX_WAIT_TIME);
        sleep(MAX_WAIT_TIME);
        sharedMemFd = shm_open(MEM_BRIDGE_NAME, O_RDWR, S_IRUSR | S_IWUSR); // Retry
    }
    if (memFail) { // Ensures shared memory has been opened
        printf("Consumer - Failed to open shared memory.\n");
        return 1;
    }
    struct SHARED_MEM_STRUCT *sharedMem = mmap(NULL, sizeof(struct SHARED_MEM_STRUCT), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemFd, 0); // Maps shared memory region to local pointer

    int syncFail = TRUE;
    int producerSetupCount = 0; 
    for (int i = 0; i < EXTERNAL_EXEC_STATE_CHECKS; ++i) { // Will check if memory has been initialized, and wait for MAX_WAIT_TIME to recheck
        if (sharedMem->syncVal == SYNC_VAL | sharedMem->syncVal == SYNC_VAL + 3) { syncFail = FALSE; break; } // Exit loop if memory confirmed to be setup
        
        if (sharedMem->syncVal == SYNC_VAL + 1) { 
            i = 0; // Producer checking for existing memory, will wait until producer sets up
            producerSetupCount = producerSetupCount + 1; // Increments variable to check for this value existing MORE than EXTERNAL_EXEC_STATE_CHECKS + 2 times
            if (producerSetupCount == EXTERNAL_EXEC_STATE_CHECKS + 2) { break; }
        } 
        printf("Consumer - Shared memory region has not yet been setup. Retrying in %i secs...\n",MAX_WAIT_TIME);
        sleep(MAX_WAIT_TIME);
    }
    if (syncFail) { // Ensures shared memory has been setup
        printf("Consumer - Shared memory region was not setup correctly.\n");
        return 1;
    }

    // Attempt to remove data from array SHARED_MEM_ACESSES amount of times
    int dataInputCount = 0; // Counter for data inputs
    for (int i = 0; i < SHARED_MEM_ACCESSES; ++i) {
        if (sharedMem->syncVal == SYNC_VAL + 1) { // Checks if other programs are requesting current existence
            sharedMem->syncVal = SYNC_VAL + 2; // Signal existance
            while (sharedMem->syncVal != SYNC_VAL + 3) { // Await until other program has acknowledged existence 
                printf("Consumer - Another process is checking for other running programs. Waiting until it confirms... (%i secs)\n",MAX_WAIT_TIME);
                sleep(MAX_WAIT_TIME);
            }
        }
        
        sem_wait(&sharedMem->syncAccess); // Await exclusive access to shared memory

        int nextWaitTime = rand() % (MAX_WAIT_TIME+1); // Produce values between 0 and MAX_WAIT_TIME
        if ((sharedMem->inPos == sharedMem->outPos) & !(sharedMem->isFull)) { // Check if shared array is empty
            printf("Consumer - Shared array empty. Sleeping %i secs...\n",nextWaitTime);
        }
        else { // Array is non-empty, thus can proceed
            char extractedStringData[RANDOM_STRING_LENGTH+1]; // Temporary array to store extracted data
            
            for (int i = 0; i < RANDOM_STRING_LENGTH; ++i) { // Extract data from array and store in local array. Wipe sharedData portion too.
                extractedStringData[i] = sharedMem->sharedData[sharedMem->outPos][i]; // Copy char out of sharedData
                sharedMem->sharedData[sharedMem->outPos][i] = ' ';// Produce values between 0 and MAX_WAIT_TIME // Clears data in shared array
            }
            sharedMem->sharedData[sharedMem->outPos][RANDOM_STRING_LENGTH] = '\0'; // Force null-terminated string
            extractedStringData[RANDOM_STRING_LENGTH] = '\0'; // Not necessarily needed, however, best practice

            sharedMem->outPos = (sharedMem->outPos + 1) % SHARED_DATA_ARRAY_LENGTH; // Ensure outPos wraps around at end of the array
            sharedMem->isFull = FALSE; // As consumer has JUST taken data out of the array, there is 1 less object in the shared array. Thus can always clear this 'flag'
            dataInputCount = dataInputCount + 1; // Increment input counter

            printf("Consumer - Data ["); // Output current state of sharedData
            for (int j = 0; j < SHARED_DATA_ARRAY_LENGTH; ++j) { // Initialize data array 
                printf("\"");
                for (int k = 0; k < RANDOM_STRING_LENGTH; ++k) { printf("%c",sharedMem->sharedData[j][k]); } // Output each valid char
                printf("\"");
                if (j != SHARED_DATA_ARRAY_LENGTH - 1) { printf(","); } // Output seperator when needed
            }
            //printf("] - Consumer will sleep for %i secs...\n",nextWaitTime);
            printf("] - Consumer removed \"%s\". Consumer will sleep for %i secs...\n",extractedStringData,nextWaitTime);
        }

        sem_post(&sharedMem->syncAccess); // Release access to other processes
        if (dataInputCount == TARGET_DATA_INPUTS) { break; } // Exit if target amount of data inputs reached
        sleep(nextWaitTime); // Delay next access attempt
    }
    printf("Consumer - Took %i objects from shared data array.\n",dataInputCount);

    // Declare exit to producer
    sem_wait(&sharedMem->syncAccess); // Request exclusive access for updating syncVal
    sharedMem->syncVal = SYNC_VAL - 1; // Signals that consumer is exiting
    sem_post(&sharedMem->syncAccess); 

    // Disconnect from shared memory
    munmap(sharedMem,sizeof(struct SHARED_MEM_STRUCT));
    close(sharedMemFd);
    shm_unlink(MEM_BRIDGE_NAME);

    printf("Consumer - runtime ended.\n");
    return 0;
}