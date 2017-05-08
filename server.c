#include "basic.h"
#include "utils.h"

void listen_connections(void) {
    struct sockaddr_in server_addr;

    if ((listensd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error_found("Error in socket\n");

    memset((void *) &server_addr, 0, sizeof(server_addr));
    (server_addr).sin_family = AF_INET;
    // All available interface
    (server_addr).sin_addr.s_addr = htonl(INADDR_ANY);
    (server_addr).sin_port = htons((signed short) port);

    // To reuse a socket
    int flag = 1;
    if (setsockopt(listensd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) != 0)
        error_found("Error in setsockopt\n");

    errno = 0;
    if (bind(listensd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        switch (errno) {
            case EACCES:
                error_found("Choose another socket\n");

            case EADDRINUSE:
                error_found("Address in use\n");

            case EINVAL:
                error_found("The socket is already bod to an address\n");

            default:
                error_found("Error in bind\n");
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


    fprintf(stdout, "-Server's socket correctly created with number: %d\n", port);

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
        rset = allset;  /* Setta il set di descrittori per la lettura */
        /* ready è il numero di descrittori pronti */
        if ((ready = select(maxd+1, &rset, NULL, NULL, NULL)) < 0) {
            perror("errore in select");
            exit(1);
        }

        if(FD_ISSET(fileno(stdin), &rset)) {
            check_stdin();

            if (fileno(stdin) > maxd) maxd = fileno(stdin);
            if (--ready <= 0) //Cicla finchè ci sono ancora descrittori leggibili da controllare
                continue;

        }


        /* Se è arrivata a richiesta di connessione, il socket di ascolto
           è leggibile: viene invocata accept() e creato  socket di connessione */
        if (FD_ISSET(listensd, &rset)) {
            len = sizeof(cliaddr);
            connsd = accept(listensd, (struct sockaddr *)&cliaddr, &len);

            if (connsd == -1) {
                switch (errno) {
                    case ECONNABORTED:
                        error_found("The connection has been aborted\n");
                        continue;

                    case ENOBUFS:
                        error_found("Not enough free memory\n");

                    case ENOMEM:
                        error_found("Not enough free memory\n");

                    case EMFILE:
                        error_found("Too many open files!\n");
                        continue;

                    case EPROTO:
                        error_found("Protocol error\n");
                        continue;

                    case EPERM:
                        error_found("Firewall rules forbid connection\n");
                        continue;

                    case ETIMEDOUT:
                        error_found("Timeout occured\n");
                        continue;

                    case EBADF:
                        error_found("Bad file number\n");
                        continue;

                    default:
                        error_found("Error in accept\n");
                }
            }

            /* Inserisce il descrittore del nuovo socket nel primo posto
               libero di client */
            for (i=0; i<FD_SETSIZE; i++)
                if (client[i] < 0) {
                    client[i] = connsd;
                    break;
                }
            /* Se non ci sono posti liberi in client, errore */
            if (i == FD_SETSIZE) {
                fprintf(stderr, "errore in accept, non ci sono posti liberi\n");
                error_found("exceeded maximum number of supported connections");
            }
            /* Altrimenti inserisce connsd tra i descrittori da controllare
               ed aggiorna maxd */
            FD_SET(connsd, &allset);
            ++active;
            if (connsd > maxd) maxd = connsd;
            if (i > maxi) maxi = i;
            if (--ready <= 0) /* Cicla finchè ci sono ancora descrittori
                           leggibili da controllare */
                continue;
        }

        /* Controlla i socket attivi per controllare se sono leggibili */
        for (i = 0; i <= maxi; i++) {
            if ((socksd = client[i]) < 0 )
                /* se il descrittore non è stato selezionato viene saltato */
                continue;

            if (FD_ISSET(socksd, &rset)) {
                // Se socksd è leggibile, invoca la recv

                memset(http_req, (int) '\0', 5 * DIM);
                for (x = 0; x < 7; ++x)
                    line_req[x] = NULL;

                errno=0;

                n = recv(socksd, http_req, 5*DIM,0);


                if (n <0) {
                    switch (errno) {
                        case EACCES:
                            fprintf(stderr, "EACCES");
                            break;
                        case ECONNRESET:
                            fprintf(stderr, "ECONNRESET");
                            break;
                        case EDESTADDRREQ:
                            fprintf(stderr, "EDESTADRREQ");
                            break;
                        case EISCONN:
                            fprintf(stderr, "EISCONN");
                            break;
                        case EMSGSIZE:
                            fprintf(stderr, "EMGSIZE");
                            break;
                        case ENOBUFS:
                            fprintf(stderr, "ENOBUFS");
                            break;
                        case ENOMEM:
                            fprintf(stderr, "ENOMEM");
                            break;
                        case EOPNOTSUPP:
                            fprintf(stderr, "EOPTNOTSUPP");
                            break;
                        case EPIPE:
                            fprintf(stderr, "EPIPE");
                            break;
                        case EFAULT:
                            fprintf(stderr, "The receive  buffer  pointer(s)  point  outside  the  process's address space");
                            break;

                        case EBADF:
                                 fprintf(stderr, "The argument of recv() is an invalid descriptor: %d\n", socksd);
                                 break;

                        case ECONNREFUSED:
                            fprintf(stderr, "Remote host refused to allow the network connection\n");
                            break;

                        case ENOTSOCK:
                            fprintf(stderr, "The argument of recv() does not refer to a socket\n");
                            break;

                        case EINVAL:
                            fprintf(stderr, "Invalid argument passed\n");
                            break;

                        case EINTR:
                            fprintf(stderr, "Timeout receiving from socket\n");
                            break;

                        case EWOULDBLOCK:
                            fprintf(stderr, "Timeout receiving from socket\n");
                            break;

                        default:
                            fprintf(stderr, "Error in recv: error while receiving data from client\n");
                    }

                }

                if ((n == 0 )) {

                    // Se legge EOF, chiude il descrittore di connessione
                    if (close(socksd) == -1) {
                        perror("error in close");
                        error_found("error in close");

                    }
                    // Rimuove socksd dalla lista dei socket da controllare
                    FD_CLR(socksd, &allset);
                    --active;
                    // Cancella socksd da client
                    client[i] = -1;

                }else {
                    parse_http(http_req, line_req);

                    char log_string[DIM ];
                    memset(log_string, (int) '\0', DIM);
                    sprintf(log_string, "\t%s [%s] '%s %s %s' ",
                            inet_ntoa(cliaddr.sin_addr),get_time(), line_req[0], line_req[1], line_req[2]);
                    if(data_to_send(socksd, line_req, log_string) == 0){
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
    //to manage SIGPIPE signal
    catch_signal();

    //set server in listening state
    listen_connections();
    //the core of server based on events
    start_multiplexing_io();

}