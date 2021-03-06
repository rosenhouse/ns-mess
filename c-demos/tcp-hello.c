/*
 tcp-hello.c

 expects one or more network namespace paths as command line arguments
 starts a TCP listener in each one, each listener on port 5000.

 modified from
 http://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/tcpserver.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

const int listenPort = 5000;

// listenAndServe waits for connections in a loop
// when a client connects, it sends the message and disconnects
// listenAndServe returns only if there is an error
void listenAndServe(char* message) {
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    perror("error: listenAndServe: opening socket");
    return;
  }

  int optval = 1; // allow port to be immediately re-used after process is killed
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  struct sockaddr_in serverAddr;
  bzero((char *) &serverAddr, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons((unsigned short)listenPort);

  if (bind(listener, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
    perror("error: listenAndServe: binding");
    return;
  }

  const int queueLength = 5;
  if (listen(listener, queueLength) < 0) {
    perror("error: listenAndServe: listen");
    return;
  }

  while (1) {
    struct sockaddr_in clientAddr;
    int clientlen = sizeof(clientAddr);
    int connection = accept(listener, (struct sockaddr *) &clientAddr, &clientlen);
    if (connection < 0) {
      perror("error: accept");
      return;
    }

    if (write(connection, message, strlen(message)) < 0) {
      perror("error: writing to socket");
      return;
    }

    close(connection);
  }
}

#define BUFFER_SIZE 1024

void* threadWorker(char* namespacePath) {
  char message[BUFFER_SIZE];

  // acquire a handle to the namespace
  int namespaceFd = open(namespacePath, O_RDONLY);
  if (namespaceFd == -1) {
    perror("error: open namespace");
    exit(1);
  }

  // switch this thread to the namespace
  if (setns(namespaceFd, 0) == -1) {
    perror("error: setns");
    exit(1);
  }

  fprintf(stderr, "\nstarting a server in namespace %s\n", namespacePath);
  snprintf(message, BUFFER_SIZE, "hello from namespace %s\n", namespacePath);

  listenAndServe(message); // should block forever
  exit(1); // if we get here, there's been an error
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    fprintf(stderr, "usage: %s nspath1 nspath2 ...\n", argv[0]);
    exit(1);
  }

  int numNamespaces = argc - 1;
  char** namespacePaths = &argv[1];

  for (int i = 0; i < numNamespaces; i++) {
    pthread_t thread;
    pthread_create(&thread, NULL,
        (void *(*) (void *)) threadWorker,
        (void *) namespacePaths[i]
    );
  }

  pause();
}
