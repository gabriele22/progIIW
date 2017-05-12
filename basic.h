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


#endif //PROGETTOIIW_BASIC_H
