CS 3600 Project 4: An HTTP Web Server

When creating this web server, we started by getting the serve client function to parse just the first line of an HTTP get request, determine the filename, and serve the file back to the client.

We then added the HTTP response header.  We format it in a struct composed of several char *s.  Once formatted, we compose all the header fields and the file contents into a char * full_response, and call send using that buffer.  I think designing the code this way makes it more readable and obvious what is happening, instead of doing multiple send calls or just going through the header fields sequentially.  

The biggest challenge was a heap-smashing memory bug that apparently was caused by initializing the header structure as a pointer:

struct *header = malloc(...);
format_header(header, ...);

Switching this to 

struct header = malloc(...);
format_header(&header, ...);

seemed to solve the whole issue. 

Our code satisfies all general requirements except 404 errors.
