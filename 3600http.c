/*
 *   CS3600 Project 4: A HTTP Web Server
 */

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include "3600http.h"
#include "glib.h"

#define TCP_PORT 8080
#define MAX_PENDING_CONNECTIONS 10
#define MAX_REQUEST_SIZE 1024
#define MAX_FILENAME_SIZE 30
#define MAX_PATH_SIZE 90
#define FLAGS 0
#define BASE 10
#define ASCII_SLASH 47

//Maybe do something with multiple connections
char* global_path;
GHashTable* hash;
char* hello = "hello";

int not_strcmp(char* a, char* b){
  return !strcmp(a, b);
}

int parse_mimes(){
  FILE* fp = fopen("./mime.types", "r");
  if(fp == NULL){
    printf("ERROR: Can't open /etc/mime.types\n");
    return -1; 
  }

  hash = g_hash_table_new_full(g_str_hash, not_strcmp, free, free);
  g_hash_table_insert(hash, "hi", hello);

  char* s = (char*) malloc(100);
  char c;
  unsigned int x;
  while (fgets(s, 99, fp) != NULL){
    char* mime = strtok(s, " \t\n");
    char* tok = strtok(NULL, " \t\n");
    char* type;
    char* ext;
    while(tok != NULL){
      type = (char*) g_malloc(strlen(mime) + 1);
      strcpy(type, mime);
      ext = (char*) g_malloc(strlen(tok) + 1);
      strcpy(ext, tok);
      g_hash_table_insert(hash, ext, type);
      tok = strtok(NULL, " \t\n");
    }
  }
  
  free(s);
  return 0;
}

int parse_args(char** path, int argc, char *argv[]){
  int port = TCP_PORT;
  int path_num = 1;
  printf("Arg count: %d\n", argc);
  if(argc == 4){
    if(!strcmp(argv[1], "-p")){
      port = atoi(argv[2]);
      if(port == 0){
	port = TCP_PORT;
      } 
    } else {
      printf("ERROR: Bad flag argument.  Expected -p.\n");
      return -1;
    }
    path_num = 3;
  } else if(argc != 2){
    printf("ERROR: Wrong number of arguments. Usage:\n./3600http [-p <port>] <directory> \n   port:\t (Optional) The TCP port number of the HTTP server. Default value: 8080. \n   directory:\t (Required) The directory that your server should treat as the root Web directory.\n");
    return -1;
  }

  *path = (char*) malloc(strlen(argv[path_num]) + 1);
  strcpy(*path, argv[path_num]);

  return port;
}

/**
 * get_filename
 * takes a buffer that begins with an HTTP request header
 * extracts the requested filename and stores in &filename

 * throws an error if the request header 
 * doesn't start with "GET" (error code -1)
 * or if the filename doesn't begin with "/" (error code -2)
 * GET /afile.txt HTTP/1.0
 * aasdfjkla;sdfjkla;sdfjkl;
 */

//TODO: Why does this return an int*??
//was -> int *get_filename(char *filename, char *buffer){
int get_filename(char *filename, char *buffer){
  // buffer should read "GET <path>"
  char * buf = (char *) malloc(strlen(buffer) + 1);
  strcpy(buf, buffer);
  char * get = "GET";
  char *str;
  str = strtok(buf, " "); 
  // str should now be "GET"
  if (strcmp(str, get) != 0)
    return -1;
  str = strtok(NULL, " ");
  // str should now have filename
  if (str[0] != ASCII_SLASH) // 
    return -2;
  strcpy(filename, str);
  free(buf);
  return 0;
}

//Returns a pointer to the file extension
char* get_file_ext(char* filename){
  char* ext = strrchr(filename, '.');
  return ++ext;
}


void format_header(response_header *header, int file_size, char* file_ext, int found){
  
  file_size = file_size; 
  char * code;
  // header.code (char *)
  if (found){
    code = "HTTP/1.1 200 OK\r\n";
  }
  else {
    code = "HTTP/1.1 404 Not Found\r\n";
  }
  header->code = (char *) malloc(strlen(code) + 1);
  strncpy(header->code, code, strlen(code) + 1);
  
  //  struct time_t time;
  char * date_string = "Date: ";
  header->date = (char *) malloc(50);
  strncpy(header->date, date_string, strlen(date_string) + 1);
  time_t clock = time (NULL);
  strcat(header->date, ctime(&clock));

  // header.server (char *)
  char * server = "Server: HtiTTyP\r\n";
  header->server = (char *) malloc(strlen(server) + 1);
  strncpy(header->server, server, strlen(server) + 1);

  // header.connection (char *)
  char * connection = "Connection: close\r\n";
  header->connection = (char *) malloc(strlen(connection) + 1);
  strncpy(header->connection, connection, strlen(connection) + 1);
  
  if (found){
    // header.content_length (char *)
    char * content_length = (char *) malloc(15); //file_size);
    snprintf(content_length, 10, "%d", file_size);

    char * prefix = "Content-Length: ";
    char * suffix = "\r\n";
    int length = strlen(prefix) + strlen(content_length) + strlen(suffix) + 1;
    header->content_length = (char *) malloc(length + 1);
    strncpy(header->content_length, prefix, length + 1);
    strcat(header->content_length, content_length);
    strcat(header->content_length, suffix);  
    free(content_length);
  
    // header.content_type (char *)
    char* mime_type = g_hash_table_lookup(hash, file_ext);
    char* content_prefix = "Content-Type: ";
    char * content_type = (char*) malloc(strlen(content_prefix) + strlen(mime_type) + 2 * strlen(suffix) + 1);
    strcpy(content_type, content_prefix);
    strcat(content_type, mime_type);
    strcat(content_type, suffix);
    strcat(content_type, suffix);
    header->content_type = (char *) malloc(strlen(content_type) + 1);
    strncpy(header->content_type, content_type, strlen(content_type) + 1);
    //free(content_type);
  }
  else { // found = 0, i.e. 404 error
    char * content_length = "Content Length: 0\r\n";
    char * content_type = "Content Type: \r\n\r\n";
    header->content_length = (char *) malloc(strlen(content_length) + 1);
    header->content_type = (char *) malloc(strlen(content_type) + 1);
    strncpy(header->content_length, content_length, strlen(content_length) + 1);
    strncpy(header->content_type, content_type, strlen(content_type) + 1);
  }
  return;
}

/**
 * This function will serve the request of one client on the socket that's
 * passed in.  The socket is already contructed, you just need to read/write
 * from it to serve the request, close it, and then return from the 
 * function.
 * 
 * This function should take an int - the int of the new socket for 
 */


void serve_client(int cl_sock) {

  // read in the socket
  char * buffer = (char *) malloc(MAX_REQUEST_SIZE);
  memset(buffer, 0, MAX_REQUEST_SIZE);
  int received = recv(cl_sock, buffer, MAX_REQUEST_SIZE-1, FLAGS);
  received = received; //TODO: Need to keep track of how much data we get from the request

  char filename[MAX_FILENAME_SIZE];
  int errorcode;
  errorcode = (int) get_filename(filename, buffer);
  if (errorcode == -1){
    printf("ERROR: My web server only serves GET requests");
    return;
  }
  if (errorcode == -2){
    printf("ERROR: Filename formatted incorrectly");
    return;
  }
  // TODO:  extract below code to file_to_buffer which takes a path 
  //        and changes a buffer and returns a file_size

  char* full_path = (char*) malloc(strlen(global_path) + strlen(filename) + 1);
  strcpy(full_path, global_path);
  strcat(full_path, filename);
  printf("path is: %s\n", full_path);

  response_header header;

  FILE *file_descriptor;
  int filedes;
  int found = 0;
  int file_size = 0;
  char *file_contents = (char *) malloc (file_size + 1);
  memset(file_contents, 0, file_size + 1);
  file_descriptor = fopen(full_path, "rb");

  if (file_descriptor) {
    found = 1;
    filedes = fileno(file_descriptor);
    struct stat file_info;
    int file_size;
    stat(full_path, &file_info);
    file_size = file_info.st_size;
    printf("file size is %d\n", file_size);
    file_contents = (char *) realloc(file_contents, file_size + 1);
    memset(file_contents, 0, file_size + 1);
    read(filedes, file_contents, file_size);
    printf("file_contents are : %s\n", file_contents);
 
    format_header(&header, file_size, get_file_ext(filename), found);
  }
  else {
    found = 0;
    
    printf("404 NOT FOUND");
    format_header(&header, file_size, get_file_ext(filename), found);
  }


  int header_length = (strlen(header.code) +
		       strlen(header.date) +
		       strlen(header.server) +
		       strlen(header.connection) +
    		       strlen(header.content_length) +
		       strlen(header.content_type));
  
  char * full_response = (char *) malloc(header_length + file_size + 1000);
  strcpy(full_response, header.code);
  strcat(full_response, header.date);
  strcat(full_response, header.server);
  strcat(full_response, header.connection);
  strcat(full_response, header.content_length);
  strcat(full_response, header.content_type);
  strcat(full_response, file_contents);

  // clean up memory
  free(full_path);
  free(buffer);
  free(header.code); 
  free(header.server);
  free(header.date);
  free(header.connection);
  if (found) {
  // only free on a 200 OK
    free(header.content_length);
    free(header.content_type);
  }
  free(file_contents);
  // serve request

  send(cl_sock, full_response, strlen(full_response), FLAGS);

  // and close the socket
  free(full_response);
  close(cl_sock);
  
  return;

}

int main(int argc, char *argv[]) {
  /**
   * I've included some basic code for opening a server socket in C.  You
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */
  //Parse the mime-types
  parse_mimes(); 

  printf("Key:%s, Value:%s, Size:%d\n", "text", g_hash_table_lookup(hash, "text"), g_hash_table_size(hash));  

  //Parse the command line arguments
  int port = parse_args(&global_path, argc, argv);


  if(port < 0)
    return -1;

  printf("port : %d\n", port);
  printf("path : %s\n", global_path);

  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    printf("reached error creating socket");
    // handle error
  }

  // next, construct the destination address
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    printf("reached error binding sock to sockaddr");
    // handle error
  }

  if (listen(sock, MAX_PENDING_CONNECTIONS) < 0) {
    printf("reached error - too many sockets");
    // handle error
  }

  // now loop, accepting incoming connections and serving them on other threads
  while (1) {
    struct sockaddr_in cl_addr;
    unsigned int cl_addr_len = sizeof(cl_addr);
    int cl_sock;
    if ((cl_sock = accept(sock, (struct sockaddr *) &cl_addr, &cl_addr_len)) > 0) {
      
      pthread_t thread;
      if (pthread_create(&thread, NULL, &serve_client, cl_sock) != 0) {
        printf("reached error after pthread_create");
	// handle error
      } 
    } else {
      printf("reached error accepting socket");
      // handle error
    }
  }
  free(global_path);
  return 0;
}
