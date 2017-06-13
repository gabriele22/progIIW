//
// Created by gabriele on 03/05/17.
//

#ifndef PROGETTOIIW_BASIC_H
#define PROGETTOIIW_BASIC_H

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>


#define MAXLINE	1024
#define DIM 512
#define DIM2 64


// Struct which contains all cache references with related quality factors
struct cache {

    int q; //resized img q factor
    char img_q[DIM / 2]; //resized img name
    size_t size_q;  //resized img_size
    struct cache *next_img_c; // next node with a different quality factor for the same image
};


// Struct to keep trace of images requests in the correct order (referred to the last request)
struct cache_hit {
    char cache_name[DIM / 2]; //name of the resized image, corresponding to resized img name in the cache node
    struct cache_hit *next_hit;
};

// Struct used to map all server images to the given path in memory
struct image {

    char name[DIM2 * 2];
    size_t size_r;
    struct cache *img_c;
    struct image *next_img;
};


#endif //PROGETTOIIW_BASIC_H
