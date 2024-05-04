#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100     // Maximum number of clients the server can handle
#define BUFFER_SIZE 2048    // Buffer size for messages

static _Atomic unsigned int client_count = 0;   // Atomic counter for client count
static int unique_id = 10;                      // Unique ID counter for clients

/* Client structure */
typedef struct{
    struct sockaddr_in address; // Client's address structure
    int socket_fd;              // Client's socket file descriptor
    int uid;                    // Unique ID for the client
    char name[32];              // Client's name
} client_t;

client_t *clients[MAX_CLIENTS]; // Array to hold client structures

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to control access to the clients array

/* Overwrite stdout to maintain a clean output */
void clear_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

/* Trim newline character from the end of a string */
void trim_newline(char* arr, int length) {
    int i;
    for (i = 0; i < length; i++) { // Iterate through the string
        if (arr[i] == '\n') {       // If newline character is found
            arr[i] = '\0';          // Replace it with null terminator
            break;                  // Exit loop
        }
    }
}

/* Print client's IP address and port */
void print_client_address(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to the clients array */
void add_to_queue(client_t *cl){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i < MAX_CLIENTS; ++i){
        if(!clients[i]){
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients from the clients array */
void remove_from_queue(int uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i < MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid == uid){
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Send a message to all clients except the sender */
void send_message_to_all(char *message, int sender_uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid != sender_uid){
                if(write(clients[i]->socket_fd, message, strlen(message)) < 0){
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Handle communication with a client */
void *handle_client_communication(void *arg){
    char buffer_out[BUFFER_SIZE];
    char name[32];
    int leave_flag = 0;

    client_count++;
    client_t *client = (client_t *)arg;

    // Name
    if(recv(client->socket_fd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    } else{
        strcpy(client->name, name);
        sprintf(buffer_out, "%s has joined\n", client->name);
        printf("%s", buffer_out);
        send_message_to_all(buffer_out, client->uid);
    }

    bzero(buffer_out, BUFFER_SIZE);

    while(1){
        if (leave_flag) {
            break;
        }

        int receive = recv(client->socket_fd, buffer_out, BUFFER_SIZE, 0);
        if (receive > 0){
            if(strlen(buffer_out) > 0){
                send_message_to_all(buffer_out, client->uid);

                trim_newline(buffer_out, strlen(buffer_out));
                printf("%s -> %s\n", buffer_out, client->name);
            }
        } else if (receive == 0 || strcmp(buffer_out, "exit") == 0){
            sprintf(buffer_out, "%s has left\n", client->name);
            printf("%s", buffer_out);
            send_message_to_all(buffer_out, client->uid);
            leave_flag = 1;
        } else {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        bzero(buffer_out, BUFFER_SIZE);
    }

    // Delete client from queue and yield thread
    close(client->socket_fd);
    remove_from_queue(client->uid);
    free(client);
    client_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv){
    if(argc != 2){
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);
    int option = 1;
    int listen_socket_fd = 0, client_socket_fd = 0;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    pthread_t thread_id;

    /* Socket settings */
    listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(ip);
    server_address.sin_port = htons(port);

    /* Ignore pipe signals */
    signal(SIGPIPE, SIG_IGN);

    if(setsockopt(listen_socket_fd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

    /* Bind */
    if(bind(listen_socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(listen_socket_fd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("Jamar Whitfield's CHATROOM\n");

    while(1){
        socklen_t client_length = sizeof(client_address);
        client_socket_fd = accept(listen_socket_fd, (struct sockaddr*)&client_address, &client_length);

        /* Check if max clients is reached */
        if((client_count + 1) == MAX_CLIENTS){
            printf("Max clients reached. Rejected: ");
            print_client_address(client_address);
            printf(":%d\n", client_address.sin_port);
            close(client_socket_fd);
            continue;
        }

        /* Client settings */
        client_t *new_client = (client_t *)malloc(sizeof(client_t));
        new_client->address = client_address;
        new_client->socket_fd = client_socket_fd;
        new_client->uid = unique_id++;

        /* Add client to the queue and start a thread */
        add_to_queue(new_client);
        pthread_create(&thread_id, NULL, &handle_client_communication, (void*)new_client);

        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}
