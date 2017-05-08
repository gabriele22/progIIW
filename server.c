#include "basic.h"
#include "utils.h"


void init(int argc, char **argv){

    LOG= open_file("LOG");

    char IMAGES_PATH[DIM];
    memset(IMAGES_PATH, (int) '\0', DIM);
    strcpy(IMAGES_PATH, ".");
    int perc = 50;

    get_opt(argc, argv, IMAGES_PATH,&perc);

    // Create tmp folder for resized and cached images
    if (!mkdtemp(tmp_resized) || !mkdtemp(tmp_cache))
        error_found("Error in mkdtmp\n");
    strcpy(src_path, IMAGES_PATH);

    build_images_list(perc);
    //Size of cache is setted to an appropriate number
    if(number_of_img!=0)
        CACHE_N=((number_of_img-2)*30);
    alloc_reply_error(HTML);
    //to manage SIGPIPE signal
    catch_signal();

}

void listen_connections(void) {
    struct sockaddr_in server_addr;

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
    write_fstream(textStart,LOG);

    int i;
    /* Initialize fds numbers */
    maxd = listensd;
    maxi = -1;
    /* Initialize array  client containing used fds*/
    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;
    FD_ZERO(&allset); /* Initialize to zero allset */

    fprintf(stdout, "Listen socket created at port: %d\n", port);

}

void start_multiplexing_io(void){

    int			connsd, socksd;
    int			i,x;
    int			ready;
    ssize_t		n;
    struct sockaddr_in	 cliaddr;
    socklen_t		len;
    char http_req[DIM * DIM];
    char *line_req[7];

    FD_SET(listensd, &allset); /* Insert the listening socket into the set */
    FD_SET(fileno(stdin), &allset); /* Insert stdin into the set */

    for ( ; ; ) {
        rset = allset;  /* Configure read fds set */
        /* select returns the number of ready descriptors */
        if ((ready = select(maxd+1, &rset, NULL, NULL, NULL)) < 0) {
            perror("errore in select");
            exit(1);
        }
        //check whether the user types a character on stdin
        if(FD_ISSET(fileno(stdin), &rset)) {
            check_stdin();

            if (fileno(stdin) > maxd) maxd = fileno(stdin);
            if (--ready <= 0)
                continue;

        }
        /* If a connection request has arrived,
         * the listening socket is readable: accept() is invoked
         * anc connection socket is created */
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
            ++active;
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

                errno=0;

                n = ctrl_recv(socksd, http_req, 5 * DIM, 0);

                if ((n == 0 )) {
                    //If it reads EOF, closes the connection descriptor
                    if (close(socksd) == -1)
                        error_found("error in close");

                    // Removes socksd from the list of sockets to be checked
                    FD_CLR(socksd, &allset);
                    --active;
                    // Delete socksd from client
                    client[i] = -1;

                }else {
                    parse_http(http_req, line_req);

                    //Prepares log_string
                    char log_string[DIM];
                    memset(log_string, (int) '\0', DIM);
                    sprintf(log_string, "\t%s [%s] '%s %s %s' ",
                            inet_ntoa(cliaddr.sin_addr),get_time(), line_req[0], line_req[1], line_req[2]);
                    if(complete_http_reply(socksd, line_req, log_string) == 0){
                        write_fstream(log_string,LOG);
                        break;
                    }

                    if (--ready <= 0) break;
                }
            }
        }



    }


}


int main(int argc, char **argv)
{
    //function used to initialize parameters option, create services file and directories
    init(argc,argv);
    //set server in listening state
    listen_connections();
    //the core of server based on events
    start_multiplexing_io();

}