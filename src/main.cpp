#include "gui.hpp"
#include "config.hpp"
#include "remote_client.hpp"

#include <libgen.h>

#include <sys/socket.h> 
#include <fcntl.h> 
#include <netinet/in.h> 
#include <unistd.h> 


/* the TCP controller port */
int controller_fd = -1;

void die_with_error_message(char *s)
{
    printf("\n\n\nFATAL ERROR: %s\n\n\n", s);
    exit(1);
}

/* Create a socket, listening to it for a TCP connections, and then 
   __wait for a connection__ to it. 
   Returns the FD of the CLIENT tcp connection! */
int wait_for_client_connection(int port)
{
    /* socket to listen to */
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0) { 
        DIE("Could not create socket");
    } 

    /* set up the sockaddr_in struct properly */
    struct sockaddr_in ipv4_addr; 
    ipv4_addr.sin_family = AF_INET;  // specify the IPV4 protocol family 
    ipv4_addr.sin_addr.s_addr = INADDR_ANY;  // listen to any available interface 


    /* If the bind fails, keep increasing until we find a port we can bind on */

    bool done = false;
    while (! done) {
        ipv4_addr.sin_port = htons(port); 
        if (port > 9000) {
            printf("Couldn't bind any ports up to %d\n", port);
            DIE("failed to bind");
        }

        printf("Trying to bind to port %d\n", port);

        /* bind to the socket, and then start listening for connections */
        if (bind(socket_fd, (struct sockaddr *)&ipv4_addr, sizeof(ipv4_addr)) < 0) { 
            //DIE("Failed to bind()");
            printf("Failed to bind() - trying next port!");
            port ++;
            continue;
        } 
        if (listen(socket_fd, 3) < 0) { 
            //DIE("Failed to listen()");
            printf("Failed to listen() - trying next port!");
            port ++;
            continue;
        } 

        done = true;
    }

    socklen_t addrlen = sizeof(ipv4_addr);
    /* Now, wait for a connection before returning */
    int client_fd;
    printf("Waiting for client to connect on port %d\n", port);
    if ((client_fd = accept(socket_fd, (struct sockaddr *) &ipv4_addr,  
                                 (socklen_t*) &addrlen)) < 0) { 
        DIE("Failed to accept() connection!");
    } 

    /* Make the socket non blocking, so that when we come to read the state
       in the get_joypad_state function, we don't block */
    int cur_flags = fcntl(client_fd, F_GETFL, nullptr); 
    if (fcntl(client_fd, F_SETFL, cur_flags | O_NONBLOCK) < 0) {
        DIE("Failed to make socket non_blocking");
    }
    
    /* Close the socket we're listening to */
    close(socket_fd);
    return client_fd;
}


/* print usage_and_die information and exit the program with exit code 1 */
void usage_and_die(char *progname, char *errmsg)
{
    printf("\n\n\n");
    if (errmsg != nullptr){
        printf("ERROR: %s\n\n\n", errmsg);
    }
    printf("Usage:    %s  [optional flags]  <rom>\n\n", progname);
    printf("Flags:\n");
    printf("          -help | --help           display this help\n");
    printf("          -screensize:<n>          screen size (1 <= n <= 4)");
    printf("          -tcp:<xx>              listen to port xx for controller instructions");

    printf("\n\n\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    char *the_rom = nullptr;
    int screen_size = 1;

    // figure out program name (and take base name of the path passed in)
    char *progname = basename(argv[0]);

    /* check to see if user passed any command line params */
    char **p = argv;
    for (int p = 1; p < argc; p++) {
        char *cur_p = (char*)argv[p];
        if (cur_p[0] == '-') {
            // this is a flag 

            /* if the parameters is of the type -paramname:value, find value */
            char *value = nullptr;
            value = strchr(cur_p, ':');
            if (value)
                value++;

            if (streq(cur_p, "--help") || streq(cur_p, "-help")) {
                usage_and_die(progname, nullptr);
            }
            else if (strstarts("-screensize:", cur_p)) {
                /* code */
                printf("Setting screen size!\n");
                screen_size = atoi((char *)value);
            }
            else if (strstarts("-tcp:", cur_p)) {
                /* Set up a tcp socket for remote control */
                int p = atoi((char *) value);
                printf("Setting up a tcp socket on port %d\n", p);
                controller_fd = wait_for_client_connection(p);
            }
        } else {
            // assume rom path
            the_rom = argv[p];
        }
    }

    if (the_rom == nullptr) {
        usage_and_die(progname, (char*)"specify rom on command line");
    }

    GUI::load_settings(screen_size);
    GUI::init(the_rom);

    GUI::run();

    return 0;
}
