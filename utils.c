
#include "basic.h"



struct cache_hit *cache_hit_tail, *cache_hit_head;

//------------------
//indexes to manage select() file descriptors
int maxi, maxd;

//select() file descriptors sets
fd_set		rset, allset;

//array that keep trace of the active connections
int client[FD_SETSIZE];

//string that reports the result of the operations
char esito[MAXLINE];
//-------------------
// Log's file pointer
FILE *LOG = NULL;
// Pointer to html files; 1st: root, 2nd: 404, 3rd 400.
char *HTML[3];
// Port's number
int PORT = 11502;
// Maximum number of active connection
int MAXCONN = 1000;
// Listening socket
int LISTENsd;
// Number of cached images
volatile int CACHE_N = -1;
// Path of images to share
char IMG_PATH[DIM / 2];
// tmp files
char tmp_resized[DIM2] = "/tmp/RESIZED.XXXXXX";
// tmp files cached
char tmp_cache[DIM2] = "/tmp/CACHE.XXXXXX";
// Usage string
char *usage_str = "Usage: %s\n"
        "\t\t\t[-p port]\n"
        "\t\t\t[-i image's path]\n"
        "\t\t\t[-r percentage to resize HTML images]\n"
        "\t\t\t[-n cache size (number of images)]\n"
        "\t\t\t[-h help]\n";
// User's command
char *user_command = "-Enter 'q'/'Q' to close the server, "
        "'s'/'S' to know server's state or ";


// Struct which contains cache's state
struct cache {
    // Quality factor
    int q;
    // type string: "%s_%d";
    //     %s is the name of the image; %d is the factor quality (above int q)
    char img_q[DIM / 2];
    size_t size_q;
    struct cache *next_img_c;
};

// Struct to manage cache hit
struct cache_hit {
    char cache_name[DIM / 2];
    struct cache_hit *next_hit;
};

// Struct which contains all image's references
struct image {
    // Name of current image
    char name[DIM2 * 2];
    // Memory mapped of resized image
    size_t size_r;
    struct cache *img_c;
    struct image *next_img;
};


struct image *img;

/*prints on stderr and updates LOG file when an error occours */
void error_found(char *s) {
    fprintf(stderr, "%s", s);
    strcpy(s,"failure");

    //free_mem();

    exit(EXIT_FAILURE);
}

void usage(const char *p) {
    fprintf(stderr, usage_str, p);
    exit(EXIT_SUCCESS);
}
//Adds date and time to a string
void add_time(char *text){
    time_t	ticks;
    char buff[MAXLINE];

    ticks = time(NULL); /* legge l'orario usando la chiamata di sistema time */
    /* scrive in buff l'orario nel formato ottenuto da ctime
    snprintf impedisce l'overflow del buffer */
    snprintf(buff, sizeof(buff), "\n%s", ctime(&ticks));
    strcat(text, buff);
    return;
}

void write_fstream(char *s, FILE *file) {
    add_time(s);
    fprintf(file,"%s\n",s);
    fseek(file, sizeof(s),SEEK_CUR);
    strcpy(s,"NN");
}

//open file and check permissions
FILE *open_file(const char *path) {
    errno = 0;
    FILE *f = fopen(path, "a");
    if (!f) {
        if (errno == EACCES)
            error_found("Missing permission\n");
        error_found("Error in fopen\n");
    }

    fprintf(stdout, "file created: %s\n", path);

    return f;
}

// Used to block SIGPIPE sent from send fction
void catch_signal(void) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        error_found("Error in sigaction\n");
}
// Thread which control stdin to recognize user's input
void check_stdin(void) {

    printf("\n%s\n", user_command);
    char cmd[2];
    memset(cmd, (int) '\0', 2);
    if (fscanf(stdin, "%s", cmd) != 1)
        error_found("Error in fscanf\n");

    if (strlen(cmd) != 1) {
        printf("%s\n", user_command);
    } else {
        if (cmd[0] == 's' || cmd[0] == 'S') {
            int i,active=0;
            for(i=0; i<FD_SETSIZE;++i){
                if(client[i]>0)
                    ++active;
            }
            fprintf(stdout, "\nMaximum of simultaneous connections supported: %d\n"
                    "Number of active connections: %d\n\n", FD_SETSIZE, active);
        }/* else if (cmd[0] == 'f' || cmd[0] == 'F') {
                errno = 0;
                if (fflush(LOG)) {
                    if (errno == EBADF)
                        fprintf(stderr, "Error in fflush: Stream is not an open stream, or is not open for writing.\n");
                    fprintf(stderr, "check_stdin: Unexpected error in fflush\n");
                }
                fprintf(stdout, "Log file updated\n");
                continue;
            }*/ else if (cmd[0] == 'q' || cmd[0] == 'Q') {
            fprintf(stdout, "Closing server\n");

            errno = 0;
            // Kernel may still hold some resources for a period (TIME_WAIT)
            if (close(LISTENsd) != 0) {
                if (errno == EIO)
                    error_found("I/O error occurred\n");
                error_found("Error in close\n");
            }

            strcpy(esito,"\t\tServer closed.\n\n\n");

            errno = 0;

            if (fclose(LOG) != 0)
                error_found("Error in fclose\n");

            //free_mem();

            exit(EXIT_SUCCESS);
        }
    }
}

// Start server
void listen_connections(void) {
    struct sockaddr_in server_addr;

    if ((LISTENsd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error_found("Error in socket\n");

    memset((void *) &server_addr, 0, sizeof(server_addr));
    (server_addr).sin_family = AF_INET;
    // All available interface
    (server_addr).sin_addr.s_addr = htonl(INADDR_ANY);
    (server_addr).sin_port = htons((signed short) PORT);

    // To reuse a socket
    int flag = 1;
    if (setsockopt(LISTENsd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) != 0)
        error_found("Error in setsockopt\n");

    errno = 0;
    if (bind(LISTENsd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
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
    if (listen(LISTENsd, MAXCONN) <0)
        error_found("Error in listen\n");

    int i;
    /* Initialize fds numbers */
    maxd = LISTENsd;
    maxi = -1;
    /* Initialize array  client containing used fds*/
    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;

    FD_ZERO(&allset); /* Initialize to zero allset */


    fprintf(stdout, "-Server's socket correctly created with number: %d\n", PORT);

}

int data_to_send(int sock, char **line);
void parse_http(char *s, char **d);

void start_multiplexing_io(void){

    int			connsd, socksd;
    int			i,x;
    int			ready;
    ssize_t		n;
    struct sockaddr_in	 cliaddr;
    socklen_t		len;
    char http_req[DIM * DIM];
    char *line_req[7];

    FD_SET(LISTENsd, &allset); /* Insert the listening socket into the set */

    FD_SET(fileno(stdin), &allset); /* Insert the listening socket into the set */


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
        if (FD_ISSET(LISTENsd, &rset)) {
            len = sizeof(cliaddr);
            connsd = accept(LISTENsd, (struct sockaddr *)&cliaddr, &len);

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
//-----INVECE DI BREAK METTEREI UNA SLEEP COSI CHE SE HAI PIù di 1024 conn contemporanemente il server non si chiude

                    break;
                }
            /* Se non ci sono posti liberi in client, errore */
            if (i == FD_SETSIZE) {
                fprintf(stderr, "errore in accept, non ci sono posti liberi\n");
                strcpy(esito,"failure, exceeded maximum number of supported connections");
                exit(1);
            }
            /* Altrimenti inserisce connsd tra i descrittori da controllare
               ed aggiorna maxd */
            FD_SET(connsd, &allset);
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

                       /* case EBADF:
                            fprintf(stderr, "The argument of recv() is an invalid descriptor: %d\n", socksd);
                            break;*/

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
                        perror("errore in close");
                        strcpy(esito,"failure");
                        //exit(1);
                    }
                    // Rimuove socksd dalla lista dei socket da controllare
                    FD_CLR(socksd, &allset);
                    // Cancella socksd da client
                    client[i] = -1;
                    memset((void *)&esito, 0,sizeof(esito));

                }else {

                        //printf("\n\n%s\n\n", http_req);
                        parse_http(http_req, line_req);

                        //printf("\n\n%s\n\n", line_req[3]);

                        char log_string[DIM / 2];
                        memset(log_string, (int) '\0', DIM / 2);
                        sprintf(log_string, "\tClient:\t%s\tRequest: '%s %s %s'\n",
                                inet_ntoa(cliaddr.sin_addr), line_req[0], line_req[1], line_req[2]);
                        //write_fstream(log_string, LOG);
                        if(data_to_send(socksd, line_req)){
                            strcpy(esito,log_string);
                            break;
                        }

                        if (--ready <= 0) break;
                }
            }
        }


//----------------SCRITTURA LOG--------------------------------------
        if(strcmp(esito, "NN")==0 ){
            continue;

        }else{
            write_fstream(esito,LOG);

        }
//---------------------------------------------------------


    }


}


// Used to map in memory HTML files which respond with
//  error 400 or error 404
void map_html_error(char *HTML[3]) {
    char *s = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head><title>%s</title></head><body><h1>%s</h1><p>%s</p></body></html>\0";
    size_t len = strlen(s) + 2 * DIM2 * sizeof(char);

    char *mm1 = malloc(len);
    char *mm2 = malloc(len);
    if (!mm1 || !mm2)
        error_found("Error in malloc\n");
    memset(mm1, (int) '\0', len); memset(mm2, (int) '\0', len);
    sprintf(mm1, s, "404 Not Found", "404 Not Found", "The requested URL was not found on this server.");
    sprintf(mm2, s, "400 Bad Request", "Bad Request", "Your browser sent a request that this server could not derstand.");
    HTML[1] = mm1;
    HTML[2] = mm2;
}

// Used to get information from a file on the file system
//  check values: 1 for check directory
//                0 for check regular files
void get_info(struct stat *buf, char *path, int check) {
    memset(buf, (int) '\0', sizeof(struct stat));
    errno = 0;
    if (stat(path, buf) != 0) {
        if (errno == ENAMETOOLONG)
            error_found("Path too long\n");
        error_found("alloc_r_img: Invalid path\n");
    }
    if (check) {
        if (!S_ISDIR((*buf).st_mode)) {
            error_found("Argument -l: The path is not a directory!\n");
        }
    } else {
        if (!S_ISREG((*buf).st_mode)) {
            error_found("Non-regular files can not be analysed!\n");
        }
    }
}

// Used to analyze user's options
void get_opt(int argc, char **argv, char *path, int *perc) {
    int i;
    for (i = 1; argv[i] != NULL; ++i)
        if (strcmp(argv[i], "-h") == 0)
            usage(argv[0]);

    int c; char *e;
    struct stat statbuf;
    // Parsing the command line arguments
    // -p := port number;
    // -i := directory of files to send;
    // -r := percentage of resized images which belong to HTML file;
    // -n := size of cache;
    while ((c = getopt(argc, argv, "p:i:c:r:n:")) != -1) {
        switch (c) {
            case 'p':
                if (strlen(optarg) > 5)
                    error_found("Argument -p: Port's number too high\n");

                errno = 0;
                int p_arg = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -p: Error in strtol: Invalid number port\n");
                if (p_arg > 65535)
                    error_found("Argument -p: Port's number too high\n");
                PORT = p_arg;
                break;


            case 'i':
                get_info(&statbuf, optarg, 1);

                if (optarg[strlen(optarg) - 1] != '/') {
                    strncpy(path, optarg, strlen(optarg));
                } else {
                    strncpy(path, optarg, strlen(optarg) - 1);
                }

                if (path[strlen(path)] != '\0')
                    path[strlen(path)] = '\0';
                break;

            case 'r':
                errno = 0;
                *perc = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -r: Error in strtol: Invalid number\n");
                if (*perc < 1 || *perc > 100)
                    error_found("Argument -r: The number must be >=1 and <= 100\n");
                break;

            case 'n':
                errno = 0;
                int cache_size = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -n: Error in strtol: Invalid number\n");
                if (cache_size)
                    CACHE_N = cache_size;
                break;

            case '?':
                error_found("Invalid argument\n");

            default:
                error_found("Unknown error in getopt\n");
        }
    }
}

// Used to dynamically fill the HTML file just created
void check_and_build(char *s, char **html, size_t *dim) {
    char *k = "<b>%s</b><br><br><a href=\"%s\"><img src=\"%s/%s\" height=\"130\" weight=\"100\"></a><br><br><br><br>";

    size_t len = strlen(*html);
    if (len + DIM >= *dim * DIM) {
        ++*dim;
        *html = realloc(*html, *dim * DIM);
        if (!*html)
            error_found("Check and build: Error in realloc\n");
        memset(*html + len, (int) '\0', *dim * DIM - len);
    }

    char *w;
    if (!(w = strrchr(tmp_resized, '/')))
        error_found("Unexpected error creating HTML root file\n");
    ++w;

    char *q = *html + len;
    sprintf(q, k, s, s, w, s);
}



// Used to fill img dynamic structure
void alloc_r_img(struct image **h, char *path) {
    char new_path[DIM];
    memset(new_path, (int) '\0', DIM);
    struct image *k = malloc(sizeof(struct image));
    if (!k)
        error_found("Error in malloc\n");
    memset(k, (int) '\0', sizeof(struct image));

    char *name = strrchr(path, '/');
    if (!name) {
        if (!strncmp(path, "favicon.ico", 11)) {
            sprintf(new_path, "%s/%s", IMG_PATH, path);
            strcpy(k->name, path);
            path = new_path;
        } else {
            error_found("alloc_r_img: Error analyzing file");
        }
    } else {
        strcpy(k->name, ++name);
    }

    struct stat statbuf;
    get_info(&statbuf, path, 0);

    k->size_r = (size_t) statbuf.st_size;
    k->img_c = NULL;

    if (!*h) {
        k->next_img = *h;
        *h = k;
    } else {
        k->next_img = (*h)->next_img;
        (*h)->next_img = k;
    }
}

/*
 * NOTE: "imagemagick" package required
 * */
// Used to build the dynamic structure and fill it with the images
//  in the current folder less otherwise specified by the user.
void check_images(int perc) {

    DIR *dir;
    struct dirent *ent;
    char *k;

    errno = 0;
    dir = opendir(IMG_PATH);
    if (!dir) {
        if (errno == EACCES)
            error_found("Permission denied\n");
        error_found("check_images: Error in opendir\n");
    }

    size_t dim = 4;
    char *html = malloc((size_t) dim * DIM * sizeof(char));
    if (!html)
        error_found("Error in malloc\n");
    memset(html, (int) '\0', (size_t) dim * DIM * sizeof(char));

    // %s page's title; %s header; %s text.
    char *h = "<!DOCTYPE html><html><head><meta charset=\"utf-8\" /><title>%s</title><style type=\"text/css\"></style><script type=\"text/javascript\"></script></head><body backgrod=\"\"><h1>%s</h1><br><br><h3>%s</h3><hr><br>";
    sprintf(html, h, "ProjectIIW", "Welcome", "");
    // %s image's path; %d resizing percentage
    char *convert = "convert %s -resize %d%% %s;exit";
    size_t len_h = strlen(html), new_len_h;

    struct image **i = &img;
    char input[DIM], output[DIM];
    memset(input, (int) '\0', DIM); memset(output, (int) '\0', DIM);

    fprintf(stdout, "-Please wait while resizing images...\n");
    while ((ent = readdir(dir)) != NULL) {
        if (ent -> d_type == DT_REG) {
            if (strrchr(ent -> d_name, '~')) {
                fprintf(stderr, "File '%s' was skipped\n", ent -> d_name);
                continue;
            }

            if (!strcmp(ent -> d_name, "favicon.ico")) {
                fprintf(stdout, "-favicon.ico was setted\n");
                alloc_r_img(i, ent -> d_name);
                i = &(*i) -> next_img;
                continue;
            } else {
                if ((k = strrchr(ent -> d_name, '.'))) {
                    if (strcmp(k, ".db") == 0) {
                        fprintf(stderr, "File '%s' was skipped\n", ent -> d_name);
                        continue;
                    }
                    if (strcmp(k, ".gif") != 0 && strcmp(k, ".GIF") != 0 &&
                        strcmp(k, ".jpg") != 0 && strcmp(k, ".JPG") != 0 &&
                        strcmp(k, ".png") != 0 && strcmp(k, ".PNG") != 0)
                        fprintf(stderr, "Warning: file '%s' may have an supported format\n", ent -> d_name);
                } else {
                    fprintf(stderr, "Warning: file '%s' may have an supported format\n", ent -> d_name);
                }
            }

            char command[DIM * 2];
            memset(command, (int) '\0', DIM * 2);
            sprintf(input, "%s/%s", IMG_PATH, ent -> d_name);
            sprintf(output, "%s/%s", tmp_resized, ent -> d_name);
            sprintf(command, convert, input, perc, output);

            /**
             * NOTE: "imagemagick" package required
            **/
            if (system(command))
                error_found("check_image: Error resizing images\n");

            alloc_r_img(i, output);
            i = &(*i) -> next_img;
            check_and_build(ent -> d_name, &html, &dim);
        }
    }

    new_len_h = strlen(html);
    if (len_h == new_len_h)
        error_found("There are no images in the specified directory\n");

    h = "</body></html>";
    if (new_len_h + DIM2 / 4 > dim * DIM) {
        ++dim;
        html = realloc(html, (size_t) dim * DIM);
        if (!html)
            error_found("Checking images: Error in realloc\n");
        memset(html + new_len_h, (int) '\0', (size_t) dim * DIM - new_len_h);
    }
    k = html;
    k += strlen(html);
    strcpy(k, h);

    HTML[0] = html;

    if (closedir(dir))
        error_found("Error in closedir\n");

    fprintf(stdout, "-Images correctly resized in: '%s' with percentage: %d%%\n", tmp_resized, perc);
}

void init(int argc, char **argv){

    LOG= open_file("LOG");
    strcpy(esito,"NN");

    char IMAGES_PATH[DIM];
    memset(IMAGES_PATH, (int) '\0', DIM);
    strcpy(IMAGES_PATH, ".");

    int perc = 20;

    get_opt(argc, argv, IMAGES_PATH,&perc);

    // Create tmp folder for resized and cached images
    if (!mkdtemp(tmp_resized) || !mkdtemp(tmp_cache))
        error_found("Error in mkdtmp\n");
    strcpy(IMG_PATH, IMAGES_PATH);

    check_images(perc);

    map_html_error(HTML);


}



// Used to remove file from file system
void remove_file(char *path) {
    if (unlink(path)) {
        errno = 0;
        switch (errno) {
            case EBUSY:
                error_found("File can not be linked: It is being use by the system\n");

            case EIO:
                error_found("File can not be linked: An I/O error occurred\n");

            case ENAMETOOLONG:
                error_found("File can not be linked: Pathname was too long\n");

            case ENOMEM:
                error_found("File can not be linked: Insufficient kernel memory was available\n");

            case EPERM:
                error_found("File can not be linked: The file system does not allow linking of files\n");

            case EROFS:
                error_found("File can not be linked: Pathname refers to a file on a read-only file system\n");

            default:
                error_found("File can not be linked: Error in link\n");
        }
    }
}

int quality(char *h_accept) {
    double images, others, q;
    images = others = q = -2.0;
    char *chr, *t1 = strtok(h_accept, ",");
    if (!h_accept || !t1)
        return (int) (q *= 100);

    do {
        while (*t1 == ' ')
            ++t1;

        if (!strncmp(t1, "image", strlen("image"))) {
            chr = strrchr(t1, '=');
            // If not specified the 'q' value or if there was
            //  an error in transmission, the default
            //  value of 'q' is 1.0
            if (!chr) {
                images = 1.0;
                break;
            } else {
                errno = 0;
                double tmp = strtod(++chr, NULL);
                if (tmp > images)
                    images = tmp;
                if (errno != 0)
                    return -1;
            }
        } else if (!strncmp(t1, "*", strlen("*"))) {
            chr = strrchr(t1, '=');
            if (!chr) {
                others = 1.0;
            } else {
                errno = 0;
                others = strtod(++chr, NULL);
                if (errno != 0)
                    return -1;
            }
        }
    } while ((t1 = strtok(NULL, ",")));

    if (images > others || (others > images && images != -2.0))
        q = images;
    else if (others > images && images == -2.0)
        q = others;
    else
        fprintf(stderr, "string: %s\t\tquality: Unexpected error\n", h_accept);

    return (int) (q *= 100);
}

// Used to get image from file system
char *get_img(char *name, size_t img_dim, char *directory) {
    ssize_t left = 0;
    int fd;
    char *buf;
    char path[strlen(name) + strlen(directory) + 1];
    memset(path, (int) '\0', strlen(name) + strlen(directory) + 1);
    sprintf(path, "%s/%s", directory, name);
    if (path[strlen(path)] != '\0')
        path[strlen(path)] = '\0';

    errno = 0;
    if ((fd = open(path, O_RDONLY)) == -1) {
        switch (errno) {
            case EACCES:
                fprintf(stderr, "get_img: Permission denied\n");
                break;

            case EISDIR:
                fprintf(stderr, "get_img: '%s' is a directory\n", name);
                break;

            case ENFILE:
                fprintf(stderr, "get_img: The maximum allowable number of files is currently open in the system\n");
                break;

            case EMFILE:
                fprintf(stderr, "get_img: File descriptors are currently open in the calling process\n");
                break;

            default:
                fprintf(stderr, "Error in get_img\n");
        }
        return NULL;
    }

    errno = 0;
    if (!(buf = malloc(img_dim))) {
        fprintf(stderr, "errno: %d\t\timg_dim: %d\tget_img: Error in malloc\n", errno, (int) img_dim);
        return buf;
    } else {
        memset(buf, (int) '\0', img_dim);
    }

    while ((left = read(fd, buf + left, img_dim)))
        img_dim -= left;

    if (close(fd)) {
        fprintf(stderr, "get_img: Error closing file\t\tFile Descriptor: %d\n", fd);
    }

    return buf;
}



// Used to send HTTP messages to clients
ssize_t send_http_msg(int sd, char *s, ssize_t dim) {
    ssize_t sent = 0;
    char *msg = s;
    while (sent < dim) {
        errno=0;
        sent = send(sd, msg, (size_t) dim, MSG_NOSIGNAL);

        if (sent == -1) {
            switch (errno) {
                case EINTR:
                    perror("errore send(EINTR)");
                    break;
                case EAGAIN:
                    perror("errore send(EAGAIN)");
                    break;
                case EBADF:
                    perror("errore send(EBADF)");
                    break;
                case ECONNREFUSED:
                    perror("errore send(ECONNREFUSED)");
                    break;
                case EFAULT:
                    perror("errore send(EFAULT)");
                    break;
                case EINVAL:
                    perror("errore send(EINVAL)");
                    break;
                case ENOMEM:
                    perror("errore send(ENOMEM)");
                    break;
                case ENOTCONN:
                    perror("errore send(ENOTCONN)");
                    break;
                case ENOTSOCK:
                    perror("errore send(ENOTSOCK)");
                    break;

                default:
                    perror("errore send");
            }
        }



        if (sent <= 0)
            break;

        msg += sent;
        dim -= sent;
    }

    return sent;
}

void free_time_http(char *time, char *http) {
    free(time);
    free(http);
}
char *get_time(void) {
    time_t now = time(NULL);
    char *k = malloc(sizeof(char) * DIM2);
    if (!k)
        error_found("Error in malloc\n");
    memset(k, (int) '\0', sizeof(char) * DIM2);
    strcpy(k, ctime(&now));
    if (!k)
        error_found("Error in ctime\n");
    if (k[strlen(k) - 1] == '\n')
        k[strlen(k) - 1] = '\0';
    return k;
}


// Find and send resource for client
int data_to_send(int sock, char **line) {
    char *http_rep = malloc(DIM * DIM * 2 * sizeof(char));
    if (!http_rep)
        error_found("Error in malloc\n");
    memset(http_rep, (int) '\0',DIM * DIM * 2);

    // %d status code; %s status code; %s date; %s server; %s content type; %d content's length; %s connection type
    char *header = "HTTP/1.1 %d %s\r\nDate: %s\r\nServer: %s\r\nAccept-Ranges: bytes\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: %s\r\n\r\n";
    char *t = get_time();
    char *server = "ProjectIIW";
    char *h;

    //build the reply in case of bad request with the 400 message error

    if (!line[0] || !line[1] || !line[2] ||
        ((strncmp(line[0], "GET", 3) && strncmp(line[0], "HEAD", 4)) ||
         (strncmp(line[2], "HTTP/1.1", 8) && strncmp(line[2], "HTTP/1.0", 8)))) {
        sprintf(http_rep, header, 400, "Bad Request", t, server, "text/html", strlen(HTML[2]), "close");
        h = http_rep;
        h += strlen(http_rep);
        memcpy(h, HTML[2], strlen(HTML[2]));

        if (send_http_msg(sock, http_rep, strlen(http_rep)) == -1) {
            fprintf(stderr, "Error while sending data to client(400 bad request)\n");
            free_time_http(t, http_rep);
            return -1;
        }
        return 0;
    }
    // build the reply in case of method HEAD request
    if (strncmp(line[1], "/", strlen(line[1])) == 0) {
        sprintf(http_rep, header, 200, "OK", t, server, "text/html", strlen(HTML[0]), "keep-alive");
        if (strncmp(line[0], "HEAD", 4)) {
            h = http_rep;
            h += strlen(http_rep);
            memcpy(h, HTML[0], strlen(HTML[0]));
        }

        if (send_http_msg(sock, http_rep, strlen(http_rep)) == -1) {
            fprintf(stderr, "Error while sending data to client\n");
            free_time_http(t, http_rep);
            return -1;
        }
    }else {
        struct image *i = img;
        char *p_name;
        if (!(p_name = strrchr(line[1], '/')))
            i = NULL;
        ++p_name;
        char *p = tmp_resized + strlen("/tmp");
        // Finding image in the image structure
        while (i) {
            if (!strncmp(p_name, i->name, strlen(i->name))) {
                ssize_t dim = 0;
                char *img_to_send = NULL;
                int favicon = 1;
                if (!strncmp(p, line[1], strlen(p) - strlen(".XXXXXX")) ||
                    !(favicon = strncmp(p_name, "favicon.ico", strlen("favicon.ico")))) {
                    // Looking for resized image or favicon.ico
                    if (strncmp(line[0], "HEAD", 4)) {
                        img_to_send = get_img(p_name, i->size_r, favicon ? tmp_resized : IMG_PATH);
                        if (!img_to_send) {
                            fprintf(stderr, "data_to_send: Error in get_img\n");
                            free_time_http(t, http_rep);
                            return -1;
                        }
                    }
                    dim = i->size_r;
                } else {
                    // Looking for image in memory cache
                    char name_cached_img[DIM / 2];
                    memset(name_cached_img, (int) '\0', sizeof(char) * DIM / 2);
                    struct cache *c;
                    int def_val = 70;
                    int processing_accept = quality(line[5]);
                    if (processing_accept == -1)
                        fprintf(stderr, "data_to_send: Unexpected error in strtod\n");
                    int q = processing_accept < 0 ? def_val : processing_accept;
                    c = i->img_c;
                    while (c) {
                        if (c->q == q) {
                            strcpy(name_cached_img, c->img_q);
                            // If an image has been accessed, move it on top of the list
                            //  in order to keep the image with less hit in the bottom of the list
                            if (CACHE_N >= 0 && strncmp(cache_hit_head->cache_name,
                                                        name_cached_img, strlen(name_cached_img))) {
                                struct cache_hit *prev_node, *node;
                                prev_node = NULL;
                                node = cache_hit_tail;
                                while (node) {
                                    if (!strncmp(node->cache_name, name_cached_img, strlen(name_cached_img))) {
                                        if (prev_node) {
                                            prev_node->next_hit = node->next_hit;
                                        } else {
                                            cache_hit_tail = cache_hit_tail->next_hit;
                                        }
                                        node->next_hit = cache_hit_head->next_hit;
                                        cache_hit_head->next_hit = node;
                                        cache_hit_head = cache_hit_head->next_hit;
                                        break;
                                    }
                                    prev_node = node;
                                    node = node->next_hit;
                                }
                            }
                            break;
                        }
                        c = c->next_img_c;
                    }

                    if (!c) {
                        // %s = image's name; %d = factor quality (between 1 and 99)
                        sprintf(name_cached_img, "%s_%d", p_name, q);
                        char path[DIM / 2];
                        memset(path, (int) '\0', DIM / 2);
                        sprintf(path, "%s/%s", tmp_cache, name_cached_img);

                        if (CACHE_N > 0) {
                            // Cache of limited size
                            // If it has not yet reached
                            //  the maximum cache size
                            // %s/%s = path/name_image; %d = factor quality
                            char *format = "convert %s/%s -quality %d %s/%s;exit";
                            char command[DIM];
                            memset(command, (int) '\0', DIM);
                            sprintf(command, format, IMG_PATH, p_name, q, tmp_cache, name_cached_img);
                            if (system(command)) {
                                fprintf(stderr, "data_to_send: Unexpected error while refactoring image\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            struct stat buf;
                            memset(&buf, (int) '\0', sizeof(struct stat));
                            errno = 0;
                            if (stat(path, &buf) != 0) {
                                if (errno == ENAMETOOLONG) {
                                    fprintf(stderr, "Path too long\n");
                                    free_time_http(t, http_rep);

                                    return -1;
                                }
                                fprintf(stderr, "data_to_send: Invalid path\n");
                                free_time_http(t, http_rep);

                                return -1;
                            } else if (!S_ISREG(buf.st_mode)) {
                                fprintf(stderr, "Non-regular files can not be analysed!\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            struct cache *new_entry = malloc(sizeof(struct cache));
                            struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                            memset(new_entry, (int) '\0', sizeof(struct cache));
                            memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                            if (!new_entry || !new_hit) {
                                fprintf(stderr, "data_to_send: Error in malloc\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }
                            new_entry->q = q;
                            strcpy(new_entry->img_q, name_cached_img);
                            new_entry->size_q = (size_t) buf.st_size;
                            new_entry->next_img_c = i->img_c;
                            i->img_c = new_entry;
                            c = i->img_c;

                            strncpy(new_hit->cache_name, name_cached_img, strlen(name_cached_img));
                            if (!cache_hit_head && !cache_hit_tail) {
                                new_hit->next_hit = cache_hit_head;
                                cache_hit_tail = cache_hit_head = new_hit;
                            } else {
                                new_hit->next_hit = cache_hit_head->next_hit;
                                cache_hit_head->next_hit = new_hit;
                                cache_hit_head = cache_hit_head->next_hit;
                            }
                            --CACHE_N;
                        } else if (!CACHE_N){
                            // Cache full.
                            // You have to delete an item.
                            // You choose to delete the oldest requested element.
                            char name_to_remove[DIM / 2];
                            memset(name_to_remove, (int) '\0', DIM / 2);
                            sprintf(name_to_remove, "%s/%s", tmp_cache, cache_hit_tail->cache_name);

                            DIR *dir;
                            struct dirent *ent;
                            errno = 0;
                            dir = opendir(tmp_cache);
                            if (!dir) {
                                if (errno == EACCES) {
                                    fprintf(stderr, "data_to_send: Error in opendir: Permission denied\n");
                                    free_time_http(t, http_rep);

                                    return -1;
                                }
                                fprintf(stderr, "data_to_send: Error in opendir\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            while ((ent = readdir(dir)) != NULL) {
                                if (ent->d_type == DT_REG) {
                                    if (!strncmp(ent->d_name, cache_hit_tail->cache_name,
                                                 strlen(cache_hit_tail->cache_name))) {
                                        remove_file(name_to_remove);
                                        break;
                                    }
                                }
                            }
                            if (!ent) {
                                fprintf(stderr, "File: '%s' not removed\n", name_to_remove);
                            }

                            if (closedir(dir)) {
                                fprintf(stderr, "data_to_send: Error in closedir\n");
                                free(img_to_send);
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            // %s/%s = path/name_image; %d = factor quality
                            char *format = "convert %s/%s -quality %d %s/%s;exit";
                            char command[DIM];
                            memset(command, (int) '\0', DIM);
                            sprintf(command, format, IMG_PATH, p_name, q, tmp_cache, name_cached_img);
                            if (system(command)) {
                                fprintf(stderr, "data_to_send: Unexpected error while refactoring image\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            struct stat buf;
                            memset(&buf, (int) '\0', sizeof(struct stat));
                            errno = 0;
                            if (stat(path, &buf) != 0) {
                                if (errno == ENAMETOOLONG) {
                                    fprintf(stderr, "Path too long\n");
                                    free_time_http(t, http_rep);

                                    return -1;
                                }
                                fprintf(stderr, "data_to_send: Invalid path\n");
                                free_time_http(t, http_rep);

                                return -1;
                            } else if (!S_ISREG(buf.st_mode)) {
                                fprintf(stderr, "Non-regular files can not be analysed!\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            struct cache *new_entry = malloc(sizeof(struct cache));
                            memset(new_entry, (int) '\0', sizeof(struct cache));
                            if (!new_entry) {
                                fprintf(stderr, "data_to_send: Error in malloc\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }
                            new_entry->q = q;
                            strcpy(new_entry->img_q, name_cached_img);
                            new_entry->size_q = (size_t) buf.st_size;
                            new_entry->next_img_c = i->img_c;
                            i->img_c = new_entry;
                            c = i->img_c;

                            // To find and delete oldest requested
                            //  element from cache structure
                            struct image *img_ptr = img;
                            struct cache *cache_ptr, *cache_prev = NULL;
                            char *ext = strrchr(cache_hit_tail->cache_name, '_');
                            size_t dim_fin = strlen(ext);
                            char name_i[DIM / 2];
                            memset(name_i, (int) '\0', DIM / 2);
                            strncpy(name_i, cache_hit_tail->cache_name,
                                    strlen(cache_hit_tail->cache_name) - dim_fin);
                            while (img_ptr) {
                                if (!strncmp(img_ptr->name, name_i, strlen(name_i))) {
                                    cache_ptr = img_ptr->img_c;
                                    while (cache_ptr) {
                                        if (!strncmp(cache_ptr->img_q, cache_hit_tail->cache_name,
                                                     strlen(cache_hit_tail->cache_name))) {
                                            if (!cache_prev)
                                                img_ptr->img_c = cache_ptr->next_img_c;
                                            else
                                                cache_prev->next_img_c = cache_ptr->next_img_c;

                                            free(cache_ptr);
                                            break;
                                        }
                                        cache_prev = cache_ptr;
                                        cache_ptr = cache_ptr->next_img_c;
                                    }
                                    if (!cache_ptr) {
                                        fprintf(stderr, "data_to_send: Error! struct cache compromised\n"
                                                "-Cache size automatically set to Unlimited\n\t\tfinding: %s\n", name_i);
                                        free_time_http(t, http_rep);
                                        CACHE_N = -1;

                                        return -1;
                                    }
                                    break;
                                }
                                img_ptr = img_ptr->next_img;
                            }
                            if (!img_ptr) {
                                CACHE_N = -1;
                                fprintf(stderr, "data_to_send: Unexpected error while looking for image in struct image\n"
                                        "-Cache size automatically set to Unlimited\n\t\tfinding: %s\n", name_i);
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                            memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                            if (!new_hit) {
                                fprintf(stderr, "data_to_send: Error in malloc\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            strncpy(new_hit->cache_name, name_cached_img, strlen(name_cached_img));
                            struct cache_hit *to_be_removed = cache_hit_tail;
                            new_hit->next_hit = cache_hit_head->next_hit;
                            cache_hit_head->next_hit = new_hit;
                            cache_hit_head = cache_hit_head->next_hit;
                            cache_hit_tail = cache_hit_tail->next_hit;
                            free(to_be_removed);
                        } else {
                            // In the case where it is not place
                            //  a limit on the size of the cache
                            // %s/%s = path/name_image; %d = factor quality
                            char *format = "convert %s/%s -quality %d %s/%s;exit";
                            char command[DIM];
                            memset(command, (int) '\0', DIM);
                            sprintf(command, format, IMG_PATH, p_name, q, tmp_cache, name_cached_img);
                            if (system(command)) {
                                fprintf(stderr, "data_to_send: Unexpected error while refactoring image\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            struct stat buf;
                            memset(&buf, (int) '\0', sizeof(struct stat));
                            errno = 0;
                            if (stat(path, &buf) != 0) {
                                if (errno == ENAMETOOLONG) {
                                    fprintf(stderr, "Path too long\n");
                                    free_time_http(t, http_rep);

                                    return -1;
                                }
                                fprintf(stderr, "data_to_send: Invalid path\n");
                                free_time_http(t, http_rep);

                                return -1;
                            } else if (!S_ISREG(buf.st_mode)) {
                                fprintf(stderr, "Non-regular files can not be analysed!\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }

                            struct cache *new_entry = malloc(sizeof(struct cache));
                            memset(new_entry, (int) '\0', sizeof(struct cache));
                            if (!new_entry) {
                                fprintf(stderr, "data_to_send: Error in malloc\n");
                                free_time_http(t, http_rep);

                                return -1;
                            }
                            new_entry->q = q;
                            strcpy(new_entry->img_q, name_cached_img);
                            new_entry->size_q = (size_t) buf.st_size;
                            new_entry->next_img_c = i->img_c;
                            i->img_c = new_entry;
                            c = i->img_c;
                        }
                    }



                    if (strncmp(line[0], "HEAD", 4)) {
                        DIR *dir;
                        struct dirent *ent;
                        errno = 0;
                        dir = opendir(tmp_cache);
                        if (!dir) {
                            if (errno == EACCES) {
                                fprintf(stderr, "data_to_send: Error in opendir: Permission denied\n");
                                free_time_http(t, http_rep);
                                return -1;
                            }
                            fprintf(stderr, "data_to_send: Error in opendir\n");
                            free_time_http(t, http_rep);
                            return -1;
                        }

                        while ((ent = readdir(dir)) != NULL) {
                            if (ent->d_type == DT_REG) {
                                if (!strncmp(ent->d_name, name_cached_img, strlen(name_cached_img))) {
                                    img_to_send = get_img(name_cached_img, c->size_q, tmp_cache);
                                    if (!img_to_send) {
                                        fprintf(stderr, "data_to_send: Error in get_img\n");
                                        free_time_http(t, http_rep);
                                        return -1;
                                    }
                                    break;
                                }
                            }
                        }

                        if (closedir(dir)) {
                            fprintf(stderr, "data_to_send: Error in closedir\n");
                            free(img_to_send);
                            free_time_http(t, http_rep);
                            return -1;
                        }
                    }
                    dim = c->size_q;
                }

                sprintf(http_rep, header, 200, "OK", t, server, "image/gif", dim, "keep-alive");
                ssize_t dim_tot = (size_t) strlen(http_rep);
                if (strncmp(line[0], "HEAD", 4)) {
                    if (dim_tot + dim > DIM * DIM * 2) {
                        http_rep = realloc(http_rep, (dim_tot + dim) * sizeof(char));
                        if (!http_rep) {
                            fprintf(stderr, "data_to_send: Error in realloc\n");
                            free_time_http(t, http_rep);
                            free(img_to_send);
                            return -1;
                        }
                        memset(http_rep + dim_tot, (int) '\0', (size_t) dim);
                    }
                    h = http_rep;
                    h += dim_tot;
                    memcpy(h, img_to_send, (size_t) dim);
                    dim_tot += dim;
                }
                printf("%zi\n", dim_tot);
                if (send_http_msg(sock, http_rep, dim_tot) == -1) {
                    fprintf(stderr, "data_to_send: Error while sending data to client(good request)\n");
                    free_time_http(t, http_rep);
                    return -1;
                }

                free(img_to_send);
                break;
            }
            i = i->next_img;
        }

        if (!i) {
            sprintf(http_rep, header, 404, "Not Found", t, server, "text/html", strlen(HTML[1]), "close");
            if (strncmp(line[0], "HEAD", 4)) {
                h = http_rep;
                h += strlen(http_rep);
                memcpy(h, HTML[1], strlen(HTML[1]));
            }
            if (send_http_msg(sock, http_rep, strlen(http_rep)) == -1) {
                fprintf(stderr, "Error while sending data to client(404 not found)\n");
                free_time_http(t, http_rep);
                return -1;
            }
        }

    }

    free_time_http(t, http_rep);
    return 0;
}



// Used to split HTTP message
void parse_http(char *s, char **d) {
    char *msg_type[4];
    msg_type[0] = "Connection: ";
    msg_type[1] = "User-Agent: ";
    msg_type[2] = "Accept: ";
    msg_type[3] = "Cache-Control: ";
    // HTTP message type
    d[0] = strtok(s, " ");
    // Requested object
    d[1] = strtok(NULL, " ");
    // HTTP version
    d[2] = strtok(NULL, "\n");
    if (d[2]) {
        if (d[2][strlen(d[2]) - 1] == '\r')
            d[2][strlen(d[2]) - 1] = '\0';
    }
    char *k;
    while ((k = strtok(NULL, "\n"))) {
        // Connection type
        if (!strncmp(k, msg_type[0], strlen(msg_type[0]))) {
            d[3] = k + strlen(msg_type[0]);
            if (d[3][strlen(d[3]) - 1] == '\r')
                d[3][strlen(d[3]) - 1] = '\0';
        }
            // User-Agent type
        else if (!strncmp(k, msg_type[1], strlen(msg_type[1]))) {
            d[4] = k + strlen(msg_type[1]);
            if (d[4][strlen(d[4]) - 1] == '\r')
                d[4][strlen(d[4]) - 1] = '\0';
        }
            // Accept format
        else if (!strncmp(k, msg_type[2], strlen(msg_type[2]))) {
            d[5] = k + strlen(msg_type[2]);
            if (d[5][strlen(d[5]) - 1] == '\r')
                d[5][strlen(d[5]) - 1] = '\0';
        }
            // Cache-Control
        else if (!strncmp(k, msg_type[3], strlen(msg_type[3]))) {
            d[6] = k + strlen(msg_type[3]);
            if (d[6][strlen(d[6]) - 1] == '\r')
                d[6][strlen(d[6]) - 1] = '\0';
        }
    }
}
