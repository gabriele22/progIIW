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


void check_stdin(void);
void error_found(char *s);
char *get_time(void);
void parse_http(char *s, char **d);
void catch_signal(void);
int data_to_send(int sock, char **line, char *log_string);
void write_fstream(char *s, FILE *file);
void init(int argc, char **argv);


#endif //PROGETTOIIW_UTILS_H
