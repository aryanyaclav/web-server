#include "proxy_parse.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>



#define MAX_CLIENTS 10
#define MAX_BYTES 10*(1<<10)

typedef struct cache_element cache_element;

struct cache_element{
    char *data;
    int len;
    char* url;
    time_t lru_time_track;
    cache_element *next;
};

cache_element* find(char* url);
int add_cache_element(char* data, int size, char* url);
int remove_cache_element();

int port_number = 8080;
int proxy_socketId;
pthread_t tid[MAX_CLIENTS];
sem_t semaphore;
pthread_mutex_t lock;

cache_element* head;
int cache_size;

int * handle_request(int clientSocketid, ParsedRequest *request, char* tempReq){
    char* buf = (char *)malloc(sizeof(char)*MAX_BYTES);
    strcpy(buf, "GET");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);

    if(ParsedHeader_set(request, "Connection", "Close")<0){
        printf("set header key is not working");
    }

    if(ParsedHeader_get(request, "Host") == NULL){
        if(ParsedHeader_set(request, "Host", request->host)<0){
            printf("Set host header key is not woporkingh");
        }
    }

    
}

void * thread_fn(void * socketNew){
    sem_wait(&semaphore);
    int p;
    sem_getvalue(&semaphore, p);
    printf("Semaphore valuye is %d /n", p);
    int * t = (int*) socketNew;
    int socket = *t;
    int byte_send_clients, len;

    char* buffer = (char*)calloc(MAX_BYTES, sizeof(char));
    bzero(buffer, MAX_BYTES);
    byte_send_clients = recv(socket, buffer, MAX_BYTES, 0);

    while(byte_send_clients > 0){
        len = strlen(buffer);
        if(strstsr(buffer, "/r/n/r/n") == NULL){
            byte_send_clients = recv(socket, buffer+len, MAX_BYTES - len, 0);
        }else{
            break;
        }
    }

    char *tempReq = (char*)malloc(strlen(buffer)*sizeof(char)+1);
    for(int i=0;i<strlen(buffer);i++){
        tempReq[i] = buffer[i];
    }

    struct cache_element* temp = find(tempReq);
    if(temp != NULL){
        int size = temp->len/sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];

        while(pos < size){
            bzero(response, MAX_BYTES);
            for(int i=0;i<MAX_BYTES;i++){
                response[i] = temp->data[i];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("data retreived from cache /n.");
        printf("%s\n\n", response);
    }else if(byte_send_clients > 0){
        len = strlen(buffer);
        ParsedRequest *request = ParsedRequest_create();

        if(ParsedRequest_parse(request, buffer, len) < 0){
            printf("Parsing failed./n");
        }else{
            bzero(buffer, MAX_BYTES);
            if(!strcmp(request->method, "GET")){
                if(request->host && request->PATH && checkHTTPversion(request->version) == 1){
                    byte_send_clients = handle_request(socket, request, tempReq);
                    if(byte_send_clients == -1){
                        sendErrorMessage(socket, 500);
                    }
                }else{
                    sendErrorMessage(socket, 500);
                }
            }else{
                printf("This code doesn't support any method aprt from GET\n.");
            }
        }
        ParsedRequest_destroy(request);
    }else if(byte_send_clients == 0){
        printf("Client is disconnected");
    }
    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);
    sem_getvalue(&semaphore, p);
    printf("Semaphore value is %d \n", p);
    free(tempReq);
    return NULL;

}

int main(int argc, char* argv[]){
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;

    sem_init(&semaphore, 0 , MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if(argv == 2){
        port_number = atoi(argv[1]);
    }else{
        printf("Too few arguments/n");
        exit(1);
    }

    printf('Strating new proxy server at : %d/n', port_number);

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    
    if(proxy_socketId < 0){
        perror("Failed to create socket id/n.");
        exit(1);
    }

    int reuse = 1;

    if(setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0){
        perror("setsockotp failed /n");
    }

    bzero((char*)&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(proxy_socketId, (struct sockaddr*)&server_addr, size_of(server_addr) <0)){
        perror("Port is not available /n");
        exit(1);
    }

    printf('Binding on port %d/n', port_number);

    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if(listen_status < 0){
        perror("Error in listening/n");
        exit(1);
    }
    int i = 0;
    int connected_sockedId[MAX_CLIENTS];

    while(1){
        bzero((char*)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
        if(client_socketId < 0){
            perror("Not able to connect to socket");
            exit(1);
        }else{
            connected_sockedId[i] = client_socketId;
        }

        struct sockaddr_in * client_pt = (struct sockaddr*)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client is connected with port number %d and with ip adress is %s /n.", ntohs(client_addr.sin_port), str);

        pthread_create(&tid[i], NULL, thread_fn, (void *)&connected_sockedId[i]);
        i++;
    }
    close(proxy_socketId);
    return 0;
 } 