#include "basic.h"
#include "http_image.h"

//array that keep trace of the active connections
int client[FD_SETSIZE];
//indexes to manage select() file descriptors
int maxi, maxd;

int listen_connections(int port) {
    struct sockaddr_in server_addr;
    int listensd, BACKLOG = 1000;

    if ((listensd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error_found("listen_connections: Error listen socket creations\n");

    memset((void *) &server_addr, 0, sizeof(server_addr));
    (server_addr).sin_family = AF_INET;
    // All available interface
    (server_addr).sin_addr.s_addr = htonl(INADDR_ANY);
    (server_addr).sin_port = htons((uint16_t) (signed short) port);

    // To reuse a socket
    int flag = 1;
    if (setsockopt(listensd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) != 0)
        error_found("listen_connections: Error in set option socket\n");

    errno = 0;
    if (bind(listensd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        switch (errno) {
            case EACCES:
                error_found("listen_connections: Choose another socket\n");

            case EADDRINUSE:
                error_found("listen_connections: Address in use\n");

            case EINVAL:
                error_found("listen_connections: The socket is already bod to an address\n");

            default:
                error_found("listen_connections: Error in bind\n");
        }
    }

    // listen for incoming connections
    if (listen(listensd, BACKLOG) <0)
        error_found("Error in listen\n");

    char textStart[MAXLINE];
    char *t= get_time();
    sprintf(textStart,"%s%s%s","-------------SERVER START[",t,"]-----------------");
    write_fstream(textStart,log_file);

    int i;
    /* Initialize fds numbers */
    maxd = listensd;
    maxi = -1;
    /* Initialize array  client containing used fds*/
    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;

    fprintf(stdout, "Listen socket created at port: %d\nServer is ready.\n", port);
    return listensd;
}

void start_multiplexing_io(int listensd){

    int			connsd, socksd;
    int			i,x;
    int			ready;
    struct sockaddr_in	 cliaddr;
    socklen_t	len;
    char http_req[DIM * DIM];
    char *line_req[7], *http_reply=NULL;
    ssize_t dim_reply;
    fd_set	rset, allset;
    char log_string[DIM*2],dim_string[DIM];

    //number of active connections
    int active_conn=0;

    FD_ZERO(&allset); /* Initialize to zero allset */

    FD_SET(listensd, &allset); /* Insert the listening socket into the set */
    FD_SET(fileno(stdin), &allset); /* Insert stdin into the set */

    for ( ; ; ) {
        struct timeval tv = {180, 0};
        rset = allset;  /* Configure read fds set */
        /* select returns the number of ready descriptors */
        if ((ready = select(maxd+1, &rset, NULL, NULL, &tv)) < 0) {
            perror("errore in select");
            exit(1);
        }
        //check whether the user types a character on stdin
        if(FD_ISSET(fileno(stdin), &rset)) {
            check_stdin(listensd, active_conn);

            if (fileno(stdin) > maxd) maxd = fileno(stdin);
            if (--ready <= 0)
                continue;

        }
        /* If a connection request has arrived,
         * the listening socket is readable: accept() is invoked
         * and connection socket is created */
        if (FD_ISSET(listensd, &rset)) {
            len = sizeof(cliaddr);
            connsd = accept(listensd, (struct sockaddr *)&cliaddr, &len);

            if (connsd == -1) {
                switch (errno) {
                    case ECONNABORTED:
                        error_found("start_multiplexing_io: The connection has been aborted\n");
                        continue;
                    case ENOBUFS:
                        error_found("start_multiplexing_io: Not enough free memory\n");
                    case ENOMEM:
                        error_found("start_multiplexing_io: Not enough free memory\n");
                    case EMFILE:
                        error_found("start_multiplexing_io: Too many open files\n");
                        continue;
                    case EPROTO:
                        error_found("start_multiplexing_io: Protocol error\n");
                        continue;
                    case EPERM:
                        error_found("start_multiplexing_io: Firewall rules forbid connection\n");
                        continue;
                    case ETIMEDOUT:
                        error_found("start_multiplexing_io: Timeout occured\n");
                        continue;
                    case EBADF:
                        error_found("start_multiplexing_io: Bad file descriptor\n");
                        continue;
                    default:
                        error_found("start_multiplexing_io: Error in accept\n");
                }
            }

            /* Inserts the fd of newly created socket in the first free client slot */
            for (i=0; i<FD_SETSIZE; i++)
                if (client[i] < 0) {
                    client[i] = connsd;
                    break;
                }
            /* No free slot in client */
            if (i == FD_SETSIZE) {
                error_found("start_multiplexing_io: exceeded maximum number of supported connections");
            }
            /* Otherwise inserts connsd among the descriptors to be checked and update maxd */
            FD_SET(connsd, &allset);
            ++active_conn;
            if (connsd > maxd) maxd = connsd;
            if (i > maxi) maxi = i;
            /*Loop until there are fds to be checked*/
            if (--ready <= 0)
                continue;
        }


        /* Checks whether active socket are readable or not */
        for (i = 0; i <= maxi; i++) {
            if ((socksd = client[i]) < 0 )
                /* if the fd hasn't been selected it is skipped */
                continue;

            if (FD_ISSET(socksd, &rset)) {
                //If the socket is readable, invokes the recv() functions
                memset(http_req, (int) '\0', 5 * DIM);
                for (x = 0; x < 7; ++x)
                    line_req[x] = NULL;



                if (ctrl_recv(socksd, http_req, 5 * DIM, 0)==0) {
                    //If it reads EOF, closes the connection descriptor
                    if (close(socksd) == -1)
                        error_found("error in close");

                    // Removes socksd from the list of sockets to be checked
                    FD_CLR(socksd, &allset);
                    --active_conn;
                    // Delete socksd from client
                    client[i] = -1;

                }else {
                    parse_http(http_req, line_req);

                    memset(log_string, (int) '\0', DIM*2);
                    sprintf(log_string, "\t%s [%s] '%s %s %s' ", inet_ntoa(cliaddr.sin_addr),get_time(), line_req[0], line_req[1], line_req[2]);
                    if(complete_http_reply(line_req, log_string, &http_reply,&dim_reply) == 0){
                        ctrl_send(socksd, http_reply, dim_reply);

                        free(http_reply);
                        memset(dim_string, (int) '\0', DIM);
                        sprintf(dim_string," %zi\r", dim_reply);
                        memset(&dim_reply, (int) '\0', sizeof(ssize_t));

                        strcat(log_string,dim_string);
                        write_fstream(log_string,log_file);
                    }else free(http_reply);
                    if (--ready <= 0) break;
                }
            }
        }

    }


}


int main(int argc, char **argv)
{
    //function used to initialize parameters option, create services file and directories
    int port= init(argc,argv);
    //set server in listening state
    int listensd= listen_connections(port);
    //the core of server based on events
    start_multiplexing_io(listensd);

}