
#include "basic.h"


struct cache_hit *cache_hit_tail, *cache_hit_head;
//number of active requests
int active=0;

//indexes to manage select() file descriptors
int maxi, maxd;

int number_of_img=0;

//select() file descriptors sets
fd_set	rset, allset;

//array that keep trace of the active connections
int client[FD_SETSIZE];

// Log's file pointer
FILE *LOG = NULL;
// Pointer to html files; 1st: root, 2nd: 404, 3rd 400.
char *HTML[3];
// Port's number
int port = 11502;
//MAximum number of active connections
int BACKLOG = 1000;
// Listening socket
int listensd;
// Number of cached images
volatile int CACHE_N;
// Path of images to share
char src_path[DIM / 2];
// tmp files
char tmp_resized[DIM2] = "/tmp/RESIZED.XXXXXX";
// tmp files cached
char tmp_cache[DIM2] = "/tmp/CACHE.XXXXXX";
// Usage string
char *usage_str = "USAGE: %s\n"
        "\t\t\t[-p port]\n"
        "\t\t\t[-i image's path]\n"
        "\t\t\t[-r percentage of resized images in  homepage]\n"
        "\t\t\t[-n number of cached images]\n"
        "\t\t\t[-h help]\n";
// User's command
char *user_command = "ENTER 'q'/'Q' to close the server, "
        "'s'/'S' to know server's state ";


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


void clean_resources();


// Used to get current time
char *get_time(void) {
    time_t now = time(NULL);
    char *k = malloc(sizeof(char) * DIM2);
    if (!k) {
        perror("error malloc");
        exit(1);
    }
    memset(k, (int) '\0', sizeof(char) * DIM2);
    strcpy(k, ctime(&now));
    if (k[strlen(k) - 1] == '\n')
        k[strlen(k) - 1] = '\0';
    return k;
}
//writes on stdout and on file
void write_fstream(char *s, FILE *file) {
    fprintf(file,"%s\n\n",s);
    fseek(file, sizeof(s),SEEK_CUR);
    memset(s,(int)'\0',MAXLINE);
}

/*prints on stderr and updates LOG file when an error occours */
void error_found(char *s) {
    fprintf(stderr, "%s", s);
    char *t =get_time();
    strcat(s,t);
    char fail[MAXLINE*2];
    sprintf(fail,"FAILURE--> %s",s);

    write_fstream(fail,LOG);

    clean_resources();

    exit(EXIT_FAILURE);
}
//controls return value of recv()
ssize_t  ctrl_recv(int sockfd, void *buf, size_t len, int flags){
    ssize_t s;
    s= recv(sockfd,buf,len,flags);

    if (s <0) {
        switch (errno) {
            case EFAULT:
                fprintf(stderr, "ctr_rec: The receive  buffer  pointer(s)  point  outside  the  process's address space");
                break;
            case EBADF:
                fprintf(stderr, "ctr_rec: The argument of recv() is an invalid descriptor\n");
                break;
            case ECONNREFUSED:
                fprintf(stderr, "ctr_rec: Remote host refused to allow the network connection\n");
                break;
            case ENOTSOCK:
                fprintf(stderr, "ctr_rec: The argument of recv() does not refer to a socket\n");
                break;
            case EINVAL:
                fprintf(stderr, "ctr_rec: Invalid argument passed\n");
                break;
            case EINTR:
                fprintf(stderr, "ctr_rec:Timeout receiving from socket\n");
                break;
            case EWOULDBLOCK:
                fprintf(stderr, "ctr_rec:Timeout receiving from socket\n");
                break;
            default:
                fprintf(stderr, "ctr_rec: Error in recv: error while receiving data from client\n");
        }

    }
    return s;


}

// Controls the return value of send() and avoids incomplete sending
ssize_t ctrl_send(int sd, char *s, ssize_t dim) {
    ssize_t sent = 0;
    char *msg = s;
    while (sent < dim) {
        errno=0;
        sent = send(sd, msg, (size_t) dim, MSG_NOSIGNAL);

        if (sent == -1) {
            switch (errno) {
                case EINTR:
                    fprintf(stderr,"ctrl_send: A signal occurred before any  data  was  transmitted");
                    break;
                case EAGAIN:
                    fprintf(stderr,"ctrl_send: The  socket  is  marked  nonblocking and the requested operation would block ");
                    break;
                case EBADF:
                    fprintf(stderr,"ctrl_send: An invalid descriptor was specified.");
                    break;
                case EFAULT:
                    fprintf(stderr,"ctrl_send: An invalid user space address was specified for an argument");
                    break;
                case EINVAL:
                    fprintf(stderr,"ctrl_send: Invalid argument passed");
                    break;
                case ENOMEM:
                    fprintf(stderr,"ctrl_send: No memory available");
                    break;
                case ENOTCONN:
                    fprintf(stderr,"ctrl_send: The socket is not connected, and no target has been given.");
                    break;
                case ENOTSOCK:
                    fprintf(stderr,"ctrl_send: The file descriptor sockfd does not refer to a socket");
                    break;
                default:
                    fprintf(stderr,"ctrl_send: error send");
            }
        }

        if (sent <= 0)
            break;

        msg += sent;
        dim -= sent;
    }
    return sent;
}



//removes directory and its content
int remove_directory(const char *path)
{
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;
    if (d)
    {
        struct dirent *p;
        r = 0;
        while (!r && (p=readdir(d)))
        {
            int r2 = -1;
            char *buf;
            size_t len;
            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            {
                continue;
            }
            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);
            if (buf)
            {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path, p->d_name);
                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        r2 = remove_directory(buf);
                    }
                    else
                    {
                        r2 = unlink(buf);
                    }
                }
                free(buf);
            }
            r = r2;
        }
        closedir(d);
    }
    if (!r)
    {
        r = rmdir(path);
    }
    return r;
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
            case EACCES:
                error_found("EACCES");
            case ENOTEMPTY:
                error_found("ENOTEMPY");
            case EEXIST:
                error_found("EEXIST");

            default:
                error_found("File can not be linked: Error in link\n");
        }
    }
}


// Deallocates memory and remove utility folders
void clean_resources() {
    free(HTML[0]);
    free(HTML[1]);
    free(HTML[2]);
    if (CACHE_N >= 0 && cache_hit_head && cache_hit_tail) {
        struct cache_hit *to_be_removed;
        while (cache_hit_tail) {
            to_be_removed = cache_hit_tail;
            cache_hit_tail = cache_hit_tail->next_hit;
            free(to_be_removed);
        }
    }
    remove_directory(tmp_resized);
    remove_directory(tmp_cache);
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

    return f;
}

// Ignore SIGPIPE sent from send function
void catch_signal(void) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        error_found("catch_signal: Error in sigaction\n");
}
//Manages user's input
void check_stdin(void) {
    char cmd[2];
    memset(cmd, (int) '\0', 2);
    if (fscanf(stdin, "%s", cmd) != 1)
        error_found("check_stdin: Error in fscanf\n");

    if (strlen(cmd) != 1 ) {
        printf("%s\n", user_command);
    } else {
        if (cmd[0] == 's' || cmd[0] == 'S') {
            fprintf(stdout, " Number of current requests: %d\n\n", active);
        }else if (cmd[0] == 'q' || cmd[0] == 'Q') {
            fprintf(stdout, "Closing server\n");

            errno = 0;
            // Kernel may still hold some resources for a period (TIME_WAIT)
            if (close(listensd) != 0) {
                if (errno == EIO)
                    error_found("I/O error occurred\n");
                error_found("check_stdin: Error in close\n");
            }

            char textClose[MAXLINE];
            char *t = get_time();

            sprintf(textClose, "%s%s%s", "-------------SERVER CLOSED[", t, "]-----------------");
            write_fstream(textClose, LOG);
            fflush(LOG);

            errno = 0;
            if (fclose(LOG) != 0)
                error_found("check_stdin: Error in fclose\n");

            clean_resources();
            exit(EXIT_SUCCESS);
        }
        else printf("\n%s\n", user_command);
    }
}



// Allocates responses with error 400 or error 404
void alloc_reply_error(char **HTML) {
    char *s = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head>\t<link rel=\"shortcut icon\" href=\"/favicon.ico\">\n"
            " <title>%s</title></head><body><h1>%s</h1><p>%s</p></body></html>\0";
    size_t len = strlen(s) + 2 * DIM2 * sizeof(char);

    char *mm1 = malloc(len);
    char *mm2 = malloc(len);
    if (!mm1 || !mm2)
        error_found(" alloc_reply_error: Error in malloc\n");
    memset(mm1, (int) '\0', len); memset(mm2, (int) '\0', len);
    sprintf(mm1, s, "404 Not Found", "404 Not Found", "The requested URL was not found on this server.");
    sprintf(mm2, s, "400 Bad Request", "Bad Request", "Your browser sent a request that this server could not understand.");
    HTML[1] = mm1;
    HTML[2] = mm2;
}

// Used to fill the structure buf with file or directory information (1 for check directory, 0 for check regular files)
void fill_struct_stat(struct stat *buf, char *path, int check) {
    memset(buf, (int) '\0', sizeof(struct stat));
    errno = 0;
    if (stat(path, buf) != 0) {
        if (errno == ENAMETOOLONG)
            error_found("Path too long\n");
        error_found("insert_img: Invalid path\n");
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

// Analyze arguments passed from command line
void get_opt(int argc, char **argv, char *path, int *perc) {
    int i;
    for (i = 1; argv[i] != NULL; ++i)
        if (strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, usage_str, argv[0]);
            exit(EXIT_SUCCESS);
        }
    int c; char *e;
    struct stat statbuf;
    // -p --> port number;
    // -i --> source path for images to be resized;
    // -r --> percentage of resized images in  homepage;
    // -n --> size of cache;
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
                port = p_arg;
                break;


            case 'i':
                fill_struct_stat(&statbuf, optarg, 1);

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

// Used to build the HTML file with images tags
void add_img_tag(char *s, char **html, size_t *dim) {
    char *k = "<div align=\"center\"><b>IMAGE NAME: </b><p style=\"color:#0066FF; display:inline;\">%s</p><br><br><a href=\"%s\"><img src=\"%s/%s\" height=\"530\" weight=\"530\"></a><br><br><br><br></div>";

    size_t len = strlen(*html);
    if (len + DIM >= *dim * DIM) {
        ++*dim;
        *html = realloc(*html, *dim * DIM);
        if (!*html)
            error_found("add_img_tag: Error in realloc\n");
        memset(*html + len, (int) '\0', *dim * DIM - len);
    }

    char *w;
    if (!(w = strrchr(tmp_resized, '/')))
        error_found("add_img_tag: Error creating HTML file\n");
    ++w;

    char *q = *html + len;
    sprintf(q, k, s, s, w, s);
}

// Insert image into the list
void insert_img(struct image **h, char *path) {
    char new_path[DIM];
    memset(new_path, (int) '\0', DIM);
    struct image *k = malloc(sizeof(struct image));
    if (!k)
        error_found("insert_img: Error in malloc\n");
    memset(k, (int) '\0', sizeof(struct image));

    char *name = strrchr(path, '/');
    if (!name) {
        error_found("insert_img: Error while parsing file path");
    } else {
        strcpy(k->name, ++name);
    }

    struct stat statbuf;
    fill_struct_stat(&statbuf, path, 0);

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

//ATTENTION: "imagemagick" package needed
// Used to build the dynamic list of images searching for them
//  in the current folder (favicon.ico) and in the folder specified by the user.
void build_images_list(int perc) {

    DIR *dirCorr;
    struct dirent *entCurr;
    struct image **i = &img;
    size_t dim = 4;
    char *html = malloc((size_t) dim * DIM * sizeof(char));
    if (!html)
        error_found("Error in malloc\n");
    memset(html, (int) '\0', (size_t) dim * DIM * sizeof(char));
    // %s page's title; %s header; %s text.
    char *h = "<!DOCTYPE html><html><head><meta charset=\"utf-8\" /><title>%s</title><style type=\"text/css\"></style><script type=\"text/javascript\"></script></head><body style=\"background-color:#DDDDDD\" ><h1 align=\"center\">%s</h1><br><br><h3 align=\"center\">%s</h3><br>";
    sprintf(html, h, "ProjectIIW", "ProjectIIW HOMEPAGE", "--->CLICK on a image to resize it<---");
    // %s image's path; %d resizing percentage
    char *convert = "convert %s -resize %d%% %s;exit";
    char input[DIM], output[DIM];
    memset(input, (int) '\0', DIM); memset(output, (int) '\0', DIM);

    errno = 0;
    dirCorr = opendir(".");
    if (!dirCorr) {
        if (errno == EACCES)
            error_found("Permission denied\n");
        error_found("build_images_list: Impossible to open current directory\n");
    }
    while ((entCurr = readdir(dirCorr)) != NULL) {
        if (entCurr -> d_type == DT_REG) {
            if (!strcmp(entCurr -> d_name, "favicon.ico")) {
                char pathFav[DIM],pwd[DIM];
                if (getcwd(pwd, sizeof(pwd)) == NULL)
                    perror("build_images_list: not able to get current directory path");
                sprintf(pathFav, "%s/%s",pwd, entCurr->d_name);
                insert_img(i, pathFav);
                i = &(*i)->next_img;
                break;

            }
        }
    }

    DIR *dir;
    struct dirent *ent;
    char *k;

    errno = 0;
    dir = opendir(src_path);
    if (!dir) {
        if (errno == EACCES)
            error_found("build_images_list: Permission denied\n");
        error_found("build_images_list: Impossible to open images directory\n");
    }

    size_t len_h = strlen(html), new_len_h;

    fprintf(stdout, "Doing initialization operations on images, please wait\n");
    while ((ent = readdir(dir)) != NULL) {
        ++number_of_img;
        if (ent -> d_type == DT_REG) {
            if (strrchr(ent -> d_name, '~')) {
                fprintf(stderr, "File '%s' was skipped\n", ent -> d_name);
                continue;
            }
                if ((k = strrchr(ent -> d_name, '.'))) {
                    if (strcmp(k, ".gif") != 0 && strcmp(k, ".GIF") != 0 &&
                        strcmp(k, ".jpg") != 0 && strcmp(k, ".JPG") != 0 &&
                        strcmp(k, ".png") != 0 && strcmp(k, ".PNG") != 0)
                        fprintf(stderr, "File '%s' may have an supported format\n", ent -> d_name);
                } else {
                    fprintf(stderr, "File '%s' may have an supported format\n", ent -> d_name);
                }

            char command[DIM * 2];
            memset(command, (int) '\0', DIM * 2);
            sprintf(input, "%s/%s", src_path, ent -> d_name);
            sprintf(output, "%s/%s", tmp_resized, ent -> d_name);
            sprintf(command, convert, input, perc, output);
            //operation made by imagemagick
            if (system(command))
                error_found("build_images_list: Error imagemagick not able to resize\n");

            insert_img(i, output);
            i = &(*i) -> next_img;
            add_img_tag(ent->d_name, &html, &dim);
        }
    }

    new_len_h = strlen(html);
    if (len_h == new_len_h)
        error_found("build_images_list: There aren't images in the chosen directory\n");

    h = "</body></html>";
    if (new_len_h + DIM2 / 4 > dim * DIM) {
        ++dim;
        html = realloc(html, (size_t) dim * DIM);
        if (!html)
            error_found("build_images_list: Error in realloc\n");
        memset(html + new_len_h, (int) '\0', (size_t) dim * DIM - new_len_h);
    }
    k = html;
    k += strlen(html);
    strcpy(k, h);

    HTML[0] = html;

    if (closedir(dir))
        error_found("build_images_list: Error in closedir\n");

    fprintf(stdout, "Images resized with default quality settings in: '%s' \n", tmp_resized);
}



int parse_q_factor(char *h_accept) {
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

void clean_time_and_HTTTP(char *time, char *http) {
    free(time);
    free(http);
}

// Find and send resource for client
int complete_http_reply(int sock, char **line, char *log_string) {
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

        strcat(log_string,"400 Bad Request");
        if (ctrl_send(sock, http_rep, strlen(http_rep)) == -1) {
            fprintf(stderr, "Error while sending data to client(400 bad request)\n");
            clean_time_and_HTTTP(t, http_rep);
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
        strcat(log_string,"200 OK");
        if (ctrl_send(sock, http_rep, strlen(http_rep)) == -1) {
            fprintf(stderr, "Error while sending data to client\n");
            clean_time_and_HTTTP(t, http_rep);
            return -1;
        }
    }else {
        struct image *i = img;
        char *p_name;
        if (!(p_name = strrchr(line[1], '/')))
            i = NULL;
        ++p_name;
        char *p = tmp_resized + strlen("/tmp");
        // Finding image in the image list
        while (i) {
            if (!strncmp(p_name, i->name, strlen(i->name))) {
                ssize_t dim = 0;
                char *img_to_send = NULL;
                int favicon = 1;
                if (!strncmp(p, line[1], strlen(p) - strlen(".XXXXXX")) ||
                    !(favicon = strncmp(p_name, "favicon.ico", strlen("favicon.ico")))) {
                    // Looking for resized image or favicon.ico
                    if (strncmp(line[0], "HEAD", 4)) {
                        img_to_send = get_img(p_name, i->size_r, favicon ? tmp_resized : ".");
                        if (!img_to_send) {
                            fprintf(stderr, "complete_http_reply: Error in get_img\n");
                            clean_time_and_HTTTP(t, http_rep);
                            return -1;
                        }
                    }
                    dim = i->size_r;
                } else {
                    // Find image in memory cache
                    char name_cached_img[DIM / 2];
                    memset(name_cached_img, (int) '\0', sizeof(char) * DIM / 2);
                    struct cache *c;
                    int def_val = 70;
                    int processing_accept = parse_q_factor(line[5]);
                    if (processing_accept == -1)
                        fprintf(stderr, "complete_http_reply: Unexpected error in strtod\n");
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
                            // If it has not yet reached
                            //  the maximum cache size
                            // %s/%s = path/name_image; %d = factor quality
                            char *format = "convert %s/%s -quality %d %s/%s;exit";
                            char command[DIM];
                            memset(command, (int) '\0', DIM);
                            sprintf(command, format, src_path, p_name, q, tmp_cache, name_cached_img);
                            if (system(command)) {
                                fprintf(stderr, "complete_http_reply: Unexpected error while refactoring image\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }

                            struct stat buf;
                            memset(&buf, (int) '\0', sizeof(struct stat));
                            errno = 0;
                            if (stat(path, &buf) != 0) {
                                if (errno == ENAMETOOLONG) {
                                    fprintf(stderr, "Path too long\n");
                                    clean_time_and_HTTTP(t, http_rep);

                                    return -1;
                                }
                                fprintf(stderr, "complete_http_reply: Invalid path\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            } else if (!S_ISREG(buf.st_mode)) {
                                fprintf(stderr, "Non-regular files can not be analysed!\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }

                            struct cache *new_entry = malloc(sizeof(struct cache));
                            struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                            memset(new_entry, (int) '\0', sizeof(struct cache));
                            memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                            if (!new_entry || !new_hit) {
                                fprintf(stderr, "complete_http_reply: Error in malloc\n");
                                clean_time_and_HTTTP(t, http_rep);

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
                            printf("%d\n", CACHE_N);
                            --CACHE_N;
                            printf("dopo decr: %d\n", CACHE_N);
                        } else if (!CACHE_N){
                            printf("caso cache piena: %d\n", CACHE_N);
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
                                    fprintf(stderr, "complete_http_reply: Error in opendir: Permission denied\n");
                                    clean_time_and_HTTTP(t, http_rep);

                                    return -1;
                                }
                                fprintf(stderr, "complete_http_reply: Error in opendir\n");
                                clean_time_and_HTTTP(t, http_rep);

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
                                fprintf(stderr, "complete_http_reply: Error in closedir\n");
                                free(img_to_send);
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }

                            // %s/%s = path/name_image; %d = factor quality
                            char *format = "convert %s/%s -quality %d %s/%s;exit";
                            char command[DIM];
                            memset(command, (int) '\0', DIM);
                            sprintf(command, format, src_path, p_name, q, tmp_cache, name_cached_img);
                            if (system(command)) {
                                fprintf(stderr, "complete_http_reply: Unexpected error while refactoring image\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }

                            struct stat buf;
                            memset(&buf, (int) '\0', sizeof(struct stat));
                            errno = 0;
                            if (stat(path, &buf) != 0) {
                                if (errno == ENAMETOOLONG) {
                                    fprintf(stderr, "Path too long\n");
                                    clean_time_and_HTTTP(t, http_rep);

                                    return -1;
                                }
                                fprintf(stderr, "complete_http_reply: Invalid path\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            } else if (!S_ISREG(buf.st_mode)) {
                                fprintf(stderr, "Non-regular files can not be analysed!\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }

                            struct cache *new_entry = malloc(sizeof(struct cache));
                            memset(new_entry, (int) '\0', sizeof(struct cache));
                            if (!new_entry) {
                                fprintf(stderr, "complete_http_reply: Error in malloc\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }
                            new_entry->q = q;
                            strcpy(new_entry->img_q, name_cached_img);
                            new_entry->size_q = (size_t) buf.st_size;
                            new_entry->next_img_c = i->img_c;
                            i->img_c = new_entry;
                            c = i->img_c;

                            // To find and delete oldest requested image
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
                                        fprintf(stderr, "complete_http_reply: Error! struct cache compromised\n"
                                                "-Cache size automatically set to Unlimited\n\t\tfinding: %s\n", name_i);
                                        clean_time_and_HTTTP(t, http_rep);
                                        CACHE_N = -1;

                                        return -1;
                                    }
                                    break;
                                }
                                img_ptr = img_ptr->next_img;
                            }
                            if (!img_ptr) {
                                CACHE_N = -1;
                                fprintf(stderr, "complete_http_reply: Unexpected error while looking for image in struct image\n"
                                        "-Cache size automatically set to Unlimited\n\t\tfinding: %s\n", name_i);
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }

                            struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                            memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                            if (!new_hit) {
                                fprintf(stderr, "complete_http_reply: Error in malloc\n");
                                clean_time_and_HTTTP(t, http_rep);

                                return -1;
                            }

                            strncpy(new_hit->cache_name, name_cached_img, strlen(name_cached_img));
                            struct cache_hit *to_be_removed = cache_hit_tail;
                            new_hit->next_hit = cache_hit_head->next_hit;
                            cache_hit_head->next_hit = new_hit;
                            cache_hit_head = cache_hit_head->next_hit;
                            cache_hit_tail = cache_hit_tail->next_hit;
                            free(to_be_removed);
                        }
                    }



                    if (strncmp(line[0], "HEAD", 4)) {
                        DIR *dir;
                        struct dirent *ent;
                        errno = 0;
                        dir = opendir(tmp_cache);
                        if (!dir) {
                            if (errno == EACCES) {
                                fprintf(stderr, "complete_http_reply: Error in opendir: Permission denied\n");
                                clean_time_and_HTTTP(t, http_rep);
                                return -1;
                            }
                            fprintf(stderr, "complete_http_reply: Error in opendir\n");
                            clean_time_and_HTTTP(t, http_rep);
                            return -1;
                        }

                        while ((ent = readdir(dir)) != NULL) {
                            if (ent->d_type == DT_REG) {
                                if (!strncmp(ent->d_name, name_cached_img, strlen(name_cached_img))) {
                                    img_to_send = get_img(name_cached_img, c->size_q, tmp_cache);
                                    if (!img_to_send) {
                                        fprintf(stderr, "complete_http_reply: Error in get_img\n");
                                        clean_time_and_HTTTP(t, http_rep);
                                        return -1;
                                    }
                                    break;
                                }
                            }
                        }

                        if (closedir(dir)) {
                            fprintf(stderr, "complete_http_reply: Error in closedir\n");
                            free(img_to_send);
                            clean_time_and_HTTTP(t, http_rep);
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
                            fprintf(stderr, "complete_http_reply: Error in realloc\n");
                            clean_time_and_HTTTP(t, http_rep);
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
                if (ctrl_send(sock, http_rep, dim_tot) == -1) {
                    fprintf(stderr, "complete_http_reply: Error while sending data to client(good request)\n");
                    clean_time_and_HTTTP(t, http_rep);
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
            strcat(log_string,"404 Not Found");
            if (ctrl_send(sock, http_rep, strlen(http_rep)) == -1) {
                fprintf(stderr, "Error while sending data to client(404 not found)\n");
                clean_time_and_HTTTP(t, http_rep);
                return -1;
            }
        }else strcat(log_string,"200 OK");
    }

    clean_time_and_HTTTP(t, http_rep);
    return 0;
}

// Used to parse request message
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
