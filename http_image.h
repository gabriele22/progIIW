//
// Created by gabriele on 11/05/17.
//

#ifndef PROGETTOIIW_HTTP_H
#define PROGETTOIIW_HTTP_H


extern FILE *log_file;
extern pid_t pid;
extern int pipefd[2];

void child_work(void);
void catch_signal(void);
char * read_pipe(int fd, int size, char *q);
void write_int(int n, int fd);
int read_int(int fd);
void write_pipe(char *s, int fd);
void parse_http(char *s, char **d);
int complete_http_reply(char **line, char *log_string, char **http_rep, ssize_t *dim_t);
void creat_reply_error();
void creat_imgList_html(int perc);
int init(int argc, char **argv);
void error_found(char *s);
char *get_time(void);
void check_stdin(int listensd, int active_conn);
void write_fstream(char *s, FILE *file);
ssize_t  ctrl_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t ctrl_send(int sd, char *s, ssize_t dim);

#endif //PROGETTOIIW_HTTP_H
