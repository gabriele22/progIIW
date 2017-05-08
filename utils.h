//
// Created by gabriele on 03/05/17.
//

#ifndef PROGETTOIIW_UTILS_H
#define PROGETTOIIW_UTILS_H

extern int port;
extern int BACKLOG;
extern int listensd;
extern FILE *LOG;
extern int maxi, maxd;
extern fd_set	rset, allset;
extern int client[FD_SETSIZE];
extern int active;
extern char tmp_resized[DIM2];
extern char tmp_cache[DIM2];
extern char src_path[DIM/2];
extern int number_of_img;
extern volatile int CACHE_N;
extern char *HTML[3];

ssize_t  ctrl_recv(int sockfd, void *buf, size_t len, int flags);
void alloc_reply_error(char **HTML);
void build_images_list(int perc);
void error_found(char *s);
void get_opt(int argc, char **argv, char *path, int *perc);
FILE *open_file(const char *path);
void check_stdin(void);
void error_found(char *s);
char *get_time(void);
void parse_http(char *s, char **d);
void catch_signal(void);
int complete_http_reply(int sock, char **line, char *log_string);
void write_fstream(char *s, FILE *file);
void init(int argc, char **argv);


#endif //PROGETTOIIW_UTILS_H
