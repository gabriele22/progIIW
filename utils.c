
#include "basic.h"




FILE *log_file = NULL;
volatile int cache_size=0;
char src_path[DIM / 2];
pid_t pid;
int pipefd[2];



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
//writes on file log_file
void write_fstream(char *s, FILE *file) {
    fprintf(file,"%s\n",s);
    fseek(file, sizeof(s),SEEK_CUR);
    memset(s,(int)'\0',MAXLINE);
}



/*prints on stderr and updates log_file file when an error occours */
void error_found(char *s) {
    fprintf(stderr, "%s", s);
    char f[MAXLINE];
    strcpy(f,s);
    if(f[strlen(f)-1]=='\n')
        f[strlen(f)-1]='\0';

    char fail[MAXLINE*2];
    sprintf(fail,"FAILURE--> %s [%s]",f,get_time());
    write_fstream(fail,log_file);
    clean_resources();

    exit(EXIT_FAILURE);
}
//send string to pipe (avoid incomplete write)
void write_pipe(char *s, int fd){

    int size = (int) (strlen(s));

    while(size>0) {
        int rc = (int) write(fd, s, (size_t) size);
        if (rc==-1) {
            error_found("write pipe: error write\n");
        }
        s+=rc ;
        size-=rc ;
    }

}

//send int to pipe
void write_int(int n, int fd){

    int rc = (int) write(fd, &n, sizeof(n));
    if ( rc == sizeof ( n ) )
        return ;
    if ( rc == -1 && errno != EINTR ) {
        error_found("write_int: error write -1 o eintr\n");
    }
    error_found("write_int: error write\n");
}

//read int from pipe
int read_int(int fd)
{
    size_t len = sizeof ( int) ;
    int rc , v ;
    char * p = ( char *) & v ;
    do {
        rc = (int) read (fd , p , len );
        if ( rc == -1) {
            error_found("read_int: error read");
        }
        if ( rc == 0) {
            return 0;
        }
        len -= rc ;
        p += rc ;
    } while ( len > 0) ;
    return v ;
}
//read string from pipe (avoid incomplete read)
void read_pipe(int fd, int size, char *q) {

    while (size > 0) {
        int rc = (int) read(fd, q, (size_t) size);
        if (rc == -1) {
            error_found("read_pipe: error  read -1\n");
        }
        if (rc == 0) {
            error_found("read_pipe: error read 0\n");
        }
        q += rc;
        size -= rc;
    }
}
//child read from pipe and write on log file
void child_work(void){

    if (close(pipefd[1]) == -1) {
        error_found("child_work: error close writing pipe\n");
    }

    for(;;){
        int n= read_int(pipefd[0]);
        if(n==0) continue;
        else {

            char buf[DIM * 2];
            read_pipe(pipefd[0], n, buf);
            if (strncmp(buf, "close",5) == 0) {
                printf("Please wait...\n ");
                break;
            }
            char *slash= strchr(buf,'/');
            slash=slash;
            strcat(buf,"\0");
            write_fstream(buf, log_file);
        }

    }
    if (close(pipefd[0]) == -1) {
        error_found("child_work: error close reading pipe \n");
    }
    exit(EXIT_SUCCESS);
}


//controls return value of recv()
ssize_t  ctrl_recv(int sockfd, void *buf, size_t len, int flags){
    ssize_t s;
    errno=0;
    s= recv(sockfd,buf,len,flags);

    if (s <0) {
        switch (errno) {
            case EFAULT:
                fprintf(stderr, "ctr_recv: The receive  buffer  pointer(s)  point  outside  the  process's address space");
                break;
            case EBADF:
                fprintf(stderr, "ctr_recv: The argument of recv() is an invalid descriptor\n");
                break;
            case ECONNREFUSED:
                fprintf(stderr, "ctr_recv: Remote host refused to allow the network connection\n");
                break;
            case ENOTSOCK:
                fprintf(stderr, "ctr_recv: The argument of recv() does not refer to a socket\n");
                break;
            case EINVAL:
                fprintf(stderr, "ctr_recv: Invalid argument passed\n");
                break;
            case EINTR:
                fprintf(stderr, "ctr_recv:Timeout receiving from socket\n");
                break;
            case EWOULDBLOCK:
                fprintf(stderr, "ctr_recv:Timeout receiving from socket\n");
                break;
            default:
                fprintf(stderr, "ctr_recv: Error in recv: error while receiving data from client\n");
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

//open log_file file and check permissions
FILE *open_file() {
    errno = 0;
    FILE *f = fopen("log", "a");
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
void check_stdin(int listensd, int active_conn) {

    char *user_command = "ENTER 'q'/'Q' to close the server, 's'/'S' to know server's state ";

    char cmd[2];
    memset(cmd, (int) '\0', 2);
    if (fscanf(stdin, "%s", cmd) != 1)
        error_found("check_stdin: Error in fscanf\n");

    if (strlen(cmd) != 1 ) {
        printf("%s\n", user_command);
    } else {
        if (cmd[0] == 's' || cmd[0] == 'S') {
            fprintf(stdout, "Number of current connections: %d\n\n", active_conn);
        }else if (cmd[0] == 'q' || cmd[0] == 'Q') {

            char *cl= "close";
            write_int((int)strlen(cl),pipefd[1]);

            write_pipe(cl,pipefd[1]);
            sleep(1);

            if (close(pipefd[1])== -1 )
               error_found("check_stdin: error close\n");

            fflush(log_file);
            wait(NULL);
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
            sprintf(textClose, "%s%s%s", "-------------SERVER CLOSED[", t, "]----------------");
            write_fstream(textClose, log_file);
            fflush(log_file);

            errno = 0;
            if (fclose(log_file) != 0)
                error_found("check_stdin: Error in fclose\n");

            clean_resources();
            exit(EXIT_SUCCESS);
        }else if (cmd[0] == 'f' || cmd[0] == 'F') {
            fflush(log_file);
        }else printf("\n%s\n", user_command);
    }
}


// Used to fill the structure buf with file or directory information (1 to check for directory, 0 to check for regular files)
void ctrl_stat(struct stat *buf, char *path, int check) {
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
int get_opt(int argc, char **argv, int *perc) {

    char *usage_str = "USAGE: %s\n \t\t[-p port]\n \t\t[-i image's path]\n \t\t[-r percentage of resized images in homepage]\n"
            "\t\t[-n number of cached images]\n \t\t[-h help]\n";

    char tmp_path[DIM];
    memset(tmp_path, (int) '\0', DIM);
    strcpy(tmp_path, ".");

    int port=50000;
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
    // -c --> size of cache;
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
                ctrl_stat(&statbuf, optarg, 1);

                if (optarg[strlen(optarg) - 1] != '/') {
                    strncpy(tmp_path, optarg, strlen(optarg));
                } else {
                    strncpy(tmp_path, optarg, strlen(optarg) - 1);
                }

                if (tmp_path[strlen(tmp_path)] != '\0')
                    tmp_path[strlen(tmp_path)] = '\0';
                strcpy(src_path, tmp_path);

                break;

            case 'r':
                errno = 0;
                *perc = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -r: Error in strtol: Invalid number\n");
                if (*perc < 1 || *perc > 100)
                    error_found("Argument -r: The number must be >=1 and <= 100\n");
                break;

            case 'c':
                errno = 0;
                int cache_s = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -c: Error in strtol: Invalid number\n");

                cache_size = cache_s;
                break;

            case '?':
                error_found("Invalid argument\n");

            default:
                error_found("Error in getopt\n");
        }
    }
    return port;
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


