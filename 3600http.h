/*
 * CS3600 Project 4: A HTTP Web Server
 */

#ifndef __3600HTTP_H__
#define __3600HTTP_H__

//typedef struct response_header response_header;

typedef struct response_header_t {
  char * code;
  char * date;
  char * server;
  char * connection;
  char * content_length;
  char * content_type;
} response_header;


#endif

