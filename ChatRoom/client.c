#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MESSAGE_LENGTH 2048 // Maximum length of a message

// Global variables
volatile sig_atomic_t exit_flag = 0; // Flag to signal exit
int client_socket_fd = 0;            // Client's socket file descriptor
char client_name[32];                 // Client's name

// Function to overwrite stdout
void overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

// Function to trim newline character from a string
void trim_newline(char* str, int length) {
  int i;
  for (i = 0; i < length; i++) { // Traverse the string
    if (str[i] == '\n') {        // If newline character is found
      str[i] = '\0';             // Replace it with null terminator
      break;                     // Exit loop
    }
  }
}

// Signal handler for catching Ctrl+C and exit
void handle_ctrl_c_and_exit(int sig) {
    exit_flag = 1;
}

// Function to handle sending messages to the server
void *send_message_handler() {
  char message[MESSAGE_LENGTH] = {};     // Buffer to hold user's message
        char buffer[MESSAGE_LENGTH + 32] = {}; // Buffer to hold formatted message

  while(1) {
        overwrite_stdout();                 // Show '>' prompt
    fgets(message, MESSAGE_LENGTH, stdin); // Get user input
    trim_newline(message, MESSAGE_LENGTH); // Trim newline character

    if (message[0] == '\\') { // Check if it's a command
      if (message[1] == 'q') { // If user types '\q'
        handle_ctrl_c_and_exit(2); // Exit the program gracefully
        break; // Exit the loop
      } else { // If it's an unrecognized command
        printf("Unrecognized command: %s\n", message);
      }
    } else { // If it's a regular message
      sprintf(buffer, "%s: %s\n", client_name, message); // Format the message
      send(client_socket_fd, buffer, strlen(buffer), 0); // Send the message to the server
    }

                memset(message, 0, sizeof(message)); // Clear the message buffer
    memset(buffer, 0, sizeof(buffer));     // Clear the buffer
  }

  return NULL;
}

// Function to handle receiving messages from the server
void *receive_message_handler() {
        char message[MESSAGE_LENGTH] = {}; // Buffer to hold received message
  while (1) {
                int receive = recv(client_socket_fd, message, MESSAGE_LENGTH, 0); // Receive message from server
  }
}