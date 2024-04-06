# producerConsumerCExample
Spring 2024, CS 33211-001, Project 1 - Sample implementation of the Producer-Consumer problem in C using shared memory and a semaphore. 

# Producer-Consumer Problem
The Producer-Consumer problem is generally implemented using a shared memory region and a semaphore to ensure access to the shared memory happens atomicly, where the producer and consumer can only access the shared memory region if the other is not accessing the shared memory region. For this implementation, the shared array will contain only 2 objects, where each object is a null-terminated character array of fixed character count 10. The 'producer' process will generate 10 random characters from an allowed list of characters, and place the new string of characters in an empty spot in the shared array. If no such empty spot exists at the time, the producer will release access to the shared memory region and await exclusive access again at a later time. The 'consumer' process will read the data from the shared array, output the string, and clear the data in the array. Each process keeps track of how many times they have output and input data to and from the shared array, for our problem, we have a 'target' amount of accesses for the producer and consumer processes, for our problem, we define this as 10 accesses. We also define an upper limit on the amount of accesses of 2 * target_accesses_count, or 20. This upper limit is used to prevent an infinite loop if the producer or consumer process is terminated without the other. We also define, by our problem specifications, that if the 'target' access counts are equal, then the shared data array will initially be empty and will be empty when both processes exit. Additionally, a integer 'synchronization' variable is used to validate the state of the producer and consumer, namely that if one program exits early, then eventually, the other will also exit causing the shared memory to be cleaned up. If both processes are terminated before completion, then upon the next start of the 'producer' process, the initalization will cleanup the old memory space. As defined by the project instructions, only 1 producer and 1 consumer process will be run at one time, and attempting to run more than 1 of each may cause undefined behavior such as some processes hanging or process crashes. 

The shared memory region is created using shm_open(...) and closed using shm_unlink(...), whilst the access to the shared memory is done using mmap(...) and munmap(...), and ftruncate(...) is used to re-size the shared memory region. Each of the processes' states are output to the terminal where the processes are launched, where, for convenience, the producer and consumer processes identify their respective outputs by starting each line with 'Producer - ' and 'Consumer - '. The random function rand() used to generate the sample strings is initialized with a seed of the current time, in seconds, at the start of the processes' executions. Within the shared memory region, alongside the synchronization integer, the semaphore, and the shared data array, the memory region also contains the position in the array where both the producer and consumer would be accessing, and a 'isFull' variable used to track the state of the shared array to maximize the utilization (eg. to not leave an empty object in the shared array). Additionally, alongside the upper limit on memory access attempts, we also define a maximum wait time of 3 seconds between accesses - determined randomly each time exclusive access is gained to the shared memory, and a maximum amount of times either program will attempt to wait before continuing as if the programs are in a 'failure' state. The shared memory bridge is named 'PROD_CONS_EG_BRIDGE', and the synchronization variable is defined as 10 - an arbitrary decision, the value itself does not necessarily matter. 

All variables discussed above are defined at compile time, and cannot be changed without re-compiling the programs. All 'structural' contants MUST be the same in the producer and the consumer programs. (Namely 'MEM_BRIDGE_NAME','SYNC_VAL','SHARED_DATA_ARRAY_LENGTH','RANDOM_STRING_LENGTH'), however, other constants, such as ('SHARED_MEM_ACCESSES','RANDOM_STRING_ALLOWED_CHARS','MAX_WAIT_TIME','EXTERNAL_STATE_CHECKS','TARGET_DATA_OUTPUTS'/'TARGET_DATA_INPUTS') may be changed on a per-program basis. 

# Implementation Details
## Variable Definitions
```
MEM_BRIDGE_NAME "PROD_CONS_EG_BRIDGE" // Name of shared memory file (in /dev/shm on linux)

SYNC_VAL 10 // Integer value used to validate state of shared memory

SHARED_DATA_ARRAY_LENGTH 2 // As defined by assignment parameters

SHARED_MEM_ACCESSES 20 // Max amount of times producer will attempt to add data to shared data array

RANDOM_STRING_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyz1234567890" // Allowed characters for random sample strings - alpha-numeric here

RANDOM_STRING_LENGTH 10 // Number of allowed characters in each sample string (actual char array length is n+1, as \0 terminating char needed)

RNG_SEED time(NULL) // Seed used for random sample strings and wait times

MAX_WAIT_TIME 3 // Maximum number of seconds producer will wait before attempting to put more data in the shared data array (will be randomly determined each time)

EXTERNAL_EXEC_STATE_CHECKS 3 // Number of MAX_WAIT_TIME's program will wait before assuming other program has failed

TARGET_DATA_OUTPUTS 10 // Maximum amount of objects producer will place into shared meemory (producer may place less if it is unable to place more)

TARGET_DATA_INPUTS 10 // Number of data inputs consumer will take before exitting (may be less if producer does not fill array quickly)
```

## Algorithm used (Producer process)
* Producer initializes memory (or re-initializes if it already exists and no other process is using it)
* Attempt to add data to array SHARED_MEM_ACCESSES times:
    * Ensure no other processes are requesting an 'alive' check - Resume when check completed
    * Wait for exclusive access to shared memory by waiting for semaphore
    * Check if array is full (will skip current access attempt) or if it isn't (will proceed with adding data to array)
    * Write random character string to array and output current state
    * Release semaphore for other process
    * Sleep for some random time <= MAX_WAIT_TIME
* Wait for consumer to signal completion (while also checking if consumer process is still 'alive')
* Cleanup shared memory and exit

## Algorithm used (Consumer process)
* Consumer await memory initialization (ensuring producer is making progress on initalization)
* Attempt to remove data from array SHARED_MEM_ACCESSES times:
    * Ensure no other processes are requesting an 'alive' check - Resume when check completed
    * Wait for exclusive access to shared memory by waiting for semaphore
    * Check if array is empty (will skip current access attempt) or if it isn't (will proceed with removing data from array)
    * Copies data out of shared array
    * Clears array using 'space' chars
    * Release semaphore for other processes
    * Sleep for some random time <= MAX_WAIT_TIME
* Signal completion for producer
* Unlink from shared memory and exit

## Sample runs (from local machine)
Sample runs were performed 1 after the other, allowing time for both processes to finish normally. The logs of each run are stored in sampleRun1.log , sampleRun2.log , sampleRun3.log in the root directory of this github repository.

# Execution Details
As defined by the problem specifications, we expect the programs to be built via the following commands:
```
gcc producer.c -pthread -lrt -o producer
gcc consumer.c -pthread -lrt -o consumer
./producer & ./consumer &
```
Thus, for convinence, two scripts have been included : './buildScript.sh' and './runScript.sh', used to compile and execute the Producer-Consumer problem. Unfortunately, only scripts for linux are included, and to execute the scripts, you may need to run 'chmod +x ./*.sh' in the root directory of this github repo. (WARNING : DO NOT RUN COMMANDS YOU DO NOT UNDERSTAND.)

Producer-Consumer implementation tested locally with gcc v13.2.0 // gcc (Ubuntu 13.2.0-4ubuntu3) 13.2.0. 

# Real-world usage
As implemented here, the program simulates a message-passing situation, where the producer sends messages into a buffer and the consumer 'recieves' messages from the buffer and removes the recieved message from the buffer. With some modification, this code can be used to implement this message passing between 2 processes, however, this is a very simple one-way bridge (only one process can send messages and only one message can recieve messages, and the implementation is not bidirectional). 
