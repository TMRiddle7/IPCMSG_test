#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <ctype.h>
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


// Thread function for receiving heartbeat messages
void *heartbeat_thread(void *arg) {
    while (1) {
        // Receive heartbeat message
        struct heartbeat_msg hb;
        if (msgrcv(heartbeat_queue, &hb, sizeof(hb), 0, 0) == -1) {
            perror("Error receiving heartbeat message");
            exit(EXIT_FAILURE);
        }
        //printf("Received heartbeat from AppID %d at %ld\n", hb.AppID, hb.sys_time);
    }
}

char* math_op(char* data) {
    int len = strlen(data);
    char* result = (char*)malloc((len + 1) * sizeof(char)); // Allocate memory for the result
    if (result == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }

    int num1 = 0; // To store the current number being parsed
    int num2 = 0;
    int op_found = 0;
    int result_index = 0; // Index to track the position in the result string
    char op = '+'; // To store the current operation

    for (int i = 0; i <= len; i++) {
        if (isdigit(data[i])) {
            if(op_found == 0){
            num1 = num1 * 10 + (data[i] - '0'); // Form the multi-digit number
            }
            else if(op_found == 1){
                num2 = num2 * 10 + (data[i] - '0'); // Form the multi-digit number
            }
        } else if (data[i] == '+' || data[i] == '-') {
            // Perform the operation based on the previous operator
            op = data[i];      
            op_found = 1;
        }
    }
    switch(op){
        case '+': sprintf(result,"%d",num1+num2); break;
        case '-': sprintf(result,"%d",num1-num2); break;
    }
    //printf("Result = %s",result);
    return result;
}
char* string_op(char* data)
{
    return data;
}

char* array_op(char* data)
{
    return data;
}

// Thread function for receiving requests
void *request_thread(void *arg) {
    while (1) {
        // Receive request message
        struct packet req;
        if (msgrcv(request_queue, &req, sizeof(req), 0, 0) == -1) {
            perror("Error receiving request message");
            exit(EXIT_FAILURE);
        }

        decode(req.data,req.length);
       
        // Verify CRC of the packet
        unsigned int crc = calculate_crc(req.data, req.length);
        if (crc != req.crc) {
            printf("CRC verification failed for request from client %d\n", req.id);
            // You may handle CRC failure differently, like sending an error response or logging
            continue;
        }

        // Process request
        // Here, we just print the received request data
        //printf("Received request from client %d: %s\n", req.id, req.data);
        // char result[MAX_DATA_SIZE];
        char *result;
        switch(req.type){
            case 1: result = math_op(req.data);
            printf("Performing mathematical operation...\n");
            break;
            case 2: result = string_op(req.data);
            printf("Performing string operation...\n");
            break;
            case 3: result = array_op(req.data);
            printf("Performing array operation...\n");
            break;
        }

        // Dummy response (echo the request back)
        // You can replace this with actual response logic
        //printf("result = %s",result);
        strcpy(req.data, result);
        //strcat(req.data, req.data);
        req.length = strlen(req.data);
        req.crc = calculate_crc(req.data, req.length);
        encode(req.data,req.length);

        // Send response message
        if (msgsnd(response_queue, &req, sizeof(req), 0) == -1) {
            perror("Error sending response message");
            exit(EXIT_FAILURE);
        }
    }
}

// Thread function for sending responses
// void *response_thread(void *arg) {
//     while (1) {
//         // Receive response message
//         struct packet resp;
//         if (msgrcv(response_queue, &resp, sizeof(resp), 0, 0) == -1) {
//             perror("Error receiving response message");
//             exit(EXIT_FAILURE);
//         }

//         // Verify CRC of the packet
//         unsigned int crc = calculate_crc(resp.data, resp.length);
//         if (crc != resp.crc) {
//             printf("CRC verification failed for response to client %d\n", resp.id);
//             // You may handle CRC failure differently, like sending an error to the client or logging
//             continue;
//         }

//         // Process response
//         // Here, we just print the received response data
//         printf("Received response for client %d: %s\n", resp.id, resp.data);
//     }
// }

// Signal handler for graceful shutdown
void sigint_handler(int signum) {
    printf("Exiting the server.\n");

    // Cleanup: Remove message queues
    msgctl(request_queue, IPC_RMID, NULL);
    msgctl(response_queue, IPC_RMID, NULL);
    msgctl(heartbeat_queue, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}

int main() {
    printf("Starting Server ...\n");
    // Create message queues
    request_queue = msgget((key_t)100, 0666 | IPC_CREAT);
    response_queue = msgget((key_t)200, 0666 | IPC_CREAT);
    heartbeat_queue = msgget((key_t)300, 0666 | IPC_CREAT);

    // Set up signal handler for graceful shutdown
    signal(SIGINT, sigint_handler);

    // Create thread for receiving heartbeat messages
    pthread_t heartbeat_tid;
    pthread_create(&heartbeat_tid, NULL, heartbeat_thread, NULL);

    // Create thread for receiving requests
    printf("Server Message queue created with ID:%d\n",request_queue);
    pthread_t request_tid;
    pthread_create(&request_tid, NULL, request_thread, NULL);
    
    printf("Server is running...\n");
    // Create thread for receiving responses
    // pthread_t response_tid;
    // pthread_create(&response_tid, NULL, response_thread, NULL);

    // Wait for threads to finish (shouldn't happen)
    pthread_join(heartbeat_tid, NULL);
    pthread_join(request_tid, NULL);
    // pthread_join(response_tid, NULL);

    return 0;
}
