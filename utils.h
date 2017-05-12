//
// Created by gabriele on 03/05/17.
//

#ifndef PROGETTOIIW_UTILS_H
#define PROGETTOIIW_UTILS_H

extern char src_path[DIM / 2];
extern int cache_size;
extern FILE *log_file;


int remove_directory(const char *path);
void remove_file(char *path);
char *get_img(char *name, size_t img_dim, char *directory);
void ctrl_stat(struct stat *buf, char *path, int check);
ssize_t  ctrl_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t ctrl_send(int sd, char *s, ssize_t dim);
void error_found(char *s);
int get_opt(int argc, char **argv, int *perc);
FILE *open_file();
void check_stdin(int listensd, int active_conn);
void error_found(char *s);
char *get_time(void);
//void parse_http(char *s, char **d);
void catch_signal(void);
//int complete_http_reply(int sock, char **line, char *log_string, char **http_rep, int *dim_t);
void write_fstream(char *s, FILE *file);



#endif //PROGETTOIIW_UTILS_H
