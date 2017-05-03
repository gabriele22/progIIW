//
// Created by gabriele on 03/05/17.
//

#ifndef PROGETTOIIW_UTILS_H
#define PROGETTOIIW_UTILS_H


extern int maxi, maxd;
extern fd_set		rset, allset;
extern int client[FD_SETSIZE];

extern char esito[MAXLINE];

extern FILE *LOG;
extern char *HTML[3];
extern int PORT;
extern int MINTH;
extern int MAXCONN;
extern int LISTENsd;
extern volatile int CACHE_N;
extern char IMG_PATH[DIM / 2];
extern char tmp_resized[DIM2];
extern char tmp_cache[DIM2];
extern char *usage_str;
extern char *user_command;

extern struct image *img;

void error_found(char *s);
void add_time(char *text);
void write_fstream(char *s, FILE *file);
FILE *open_file(const char *path);
void check_and_build(char *s, char **html, size_t *dim);
void alloc_r_img(struct image **h, char *path);
void get_info(struct stat *buf, char *path, int check);
void check_images(int perc);
void usage(const char *p);
void get_opt(int argc, char **argv, char **path, int *perc);
void map_html_error(char *HTML[3]);
void init(int argc, char **argv);
void listen_connections(void);
void catch_signal(void);
void start_multiplexing_io(void);
void check_stdin(void);
void remove_file(char *path);
int parse_q_factor(char *h_accept);
char *get_img(char *name, size_t img_dim, char *directory);
int data_to_send(int sock, char **line);
void parse_http(char *s, char **d);
char *get_time(void);
ssize_t send_http_msg(int sd, char *s, ssize_t dim);
void free_time_http(char *time, char *http);


#endif //PROGETTOIIW_UTILS_H
