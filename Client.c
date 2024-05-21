#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define HEARTBEAT_INTERVAL 3
#define MAX_DATA_SIZE 512

// Define custom packet structure
struct packet {
    long mtype;             // Message type
    int id;                 // Packet ID
    int type;               // Packet type
    int length;             // Length of data
    char data[MAX_DATA_SIZE]; // Data
    int crc;                // CRC
};

// Define heartbeat structure
struct heartbeat_msg {
    long mtype;             // Message type
    int type;               // Packet type
    int AppID;              // Application ID
    time_t sys_time;        // System time
};

// Global variables for message queues
int request_queue, response_queue, heartbeat_queue;

// Data to cycle through based on Ctrl+C presses
char *math_data = "10+5"; // Example math data
char *string_data = "Hello World!"; // Example string data
char *array_data = "{1,2,3,4,5}"; // Example array data

// Variable to track Ctrl+C presses
int ctrl_c_count = 0;

// CRC calculation function (CRC-32)
// Function to calculate CRC (example implementation)
int calculate_crc(const char *data, int length) {
    // Example CRC calculation algorithm
    int crc = 0;
    for (int i = 0; i < length; i++) {
        crc += (int)data[i];
    }
    return crc;
}

void encode(char *data, int len){
    printf("Encoding data...\n");
    int shift = 0;
    for(int i=0;i<len;i++)
    {
        //bitwise exor 3 bits starting from right hand side
        data[i] ^= (0b111 << shift);
        //moving to next 8 bits but incrementing th shift.
        shift = (shift+1)%8;
    }

}

void decode(char*data, int len)
{
    printf("Decoding data...\n");
    int shift = 0;
    for(int i=0;i<len;i++)
    {
        //bitwise exor 3 bits starting from right hand side
        data[i] ^= (0b111 << shift);
        //moving to next 8 bits but incrementing th shift.
        shift = (shift+1)%8;
    }

}

// Thread function for sending requests
void *request_thread(void *arg) {
    while (1) {
        // Prepare request message
        struct packet req;
        req.mtype = 1; // Example message type
        req.id = getpid(); // Example client ID
        req.type = (ctrl_c_count%3)+1; // Packet type based on ctrl_c_count
        
        // Determine data based on Ctrl+C count
        char *current_data;
        char *data_type;
        switch (ctrl_c_count%3) {
            case 0:
                current_data = math_data;
                data_type = "Sending Math data 5+10...";
                break;
            case 1:
                current_data = string_data;
                data_type = "Sending String data hello world";
                break;
            case 2:
                current_data = array_data;
                data_type = "Sending Array data {1,2,3,4,5}";
                break;
        }
        // Copy data to request data
        strcpy(req.data, current_data);
        req.length = strlen(req.data);
        req.crc = calculate_crc(req.data, req.length);
        encode(req.data,req.length);
        // Send request message
        if (msgsnd(request_queue, &req, sizeof(req), 0) == -1) {
            perror("Error sending request message");
            exit(EXIT_FAILURE);
        }
        printf("Packet sent to server\n");
        // Sleep before sending the next request
        sleep(3);
    }
}

// Thread function for receiving and processing responses
void *response_thread(void *arg) {
    while (1) {
        // Receive response message
        struct packet resp;
        if (msgrcv(response_queue, &resp, sizeof(resp), 0, 0) == -1) {
            perror("Error receiving response message");
            exit(EXIT_FAILURE);
        }

        decode(resp.data,resp.length);

        // Process response based on type
        printf("Received results from server %s\n", resp.data);
    }
}

// Thread function for sending heartbeat messages
void *heartbeat_thread(void *arg) {
    struct heartbeat_msg hb;
    hb.mtype = 1; // Message type for heartbeat
    hb.type = 0; // Heartbeat type
    hb.AppID = getpid(); // Application ID

    while (1) {
        // Get current system time
        hb.sys_time = time(NULL);

        // Send heartbeat message
        if (msgsnd(heartbeat_queue, &hb, sizeof(hb), 0) == -1) {
            perror("Error sending heartbeat message");
            exit(EXIT_FAILURE);
        }
        //printf("Heart Breat send at - %s",ctime(&hb.sys_time));
        sleep(HEARTBEAT_INTERVAL);
    }
}

// Signal handler for Ctrl+C
void sigint_handler(int signum) {
    printf("Changing Operation...\n");
    ctrl_c_count++;
    
}

// Signal handler for Ctrl+Z
void sigtstp_handler(int signum) {
    printf("Exiting the program.\n");

    // Cleanup: Remove message queues
    msgctl(request_queue, IPC_RMID, NULL);
    msgctl(response_queue, IPC_RMID, NULL);
    msgctl(heartbeat_queue, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}

int main() {
    
    printf("Starting Client...\n");
    // Create message queues
    request_queue = msgget((key_t)100, 0666 | IPC_CREAT);
    response_queue = msgget((key_t)200, 0666 | IPC_CREAT);
    heartbeat_queue = msgget((key_t)300, 0666 | IPC_CREAT);

    // Set up signal handler for Ctrl+C
    signal(SIGINT, sigint_handler);

    // Set up signal handler for Ctrl+Z
    signal(SIGTSTP, sigtstp_handler);
    
    printf("Message queue connected with ID:%d\n",request_queue);
    // Create thread for sending requests
    pthread_t request_tid;
    pthread_create(&request_tid, NULL, request_thread, NULL);

    // Create thread for receiving responses
    pthread_t response_tid;
    pthread_create(&response_tid, NULL, response_thread, NULL);

    // Create thread for sending heartbeats
    pthread_t heartbeat_tid;
    pthread_create(&heartbeat_tid, NULL, heartbeat_thread, NULL);

    // Wait for threads to finish (shouldn't happen)
    pthread_join(request_tid, NULL);
    pthread_join(response_tid, NULL);
    pthread_join(heartbeat_tid, NULL);

    return 0;
}
