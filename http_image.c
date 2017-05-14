//
// Created by gabriele on 11/05/17.
//

#include "basic.h"
#include "utils.h"

struct image *img;
int number_of_img=0;
char *HTML[3];
struct cache_hit *cache_hit_tail, *cache_hit_head;
char img_home[DIM2] = "/tmp/img_home.XXXXXX";
char img_cache[DIM2] = "/tmp/img_cache.XXXXXX";

// Deallocates memory and remove utility folders
void clean_resources() {
    free(HTML[0]);
    free(HTML[1]);
    free(HTML[2]);
    if (cache_size >= 0 && cache_hit_head && cache_hit_tail) {
        struct cache_hit *to_be_removed;
        while (cache_hit_tail) {
            to_be_removed = cache_hit_tail;
            cache_hit_tail = cache_hit_tail->next_hit;
            free(to_be_removed);
        }
    }
    remove_directory(img_home);
    remove_directory(img_cache);
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
    ctrl_stat(&statbuf, path, 0);

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
    if (!(w = strrchr(img_home, '/')))
        error_found("add_img_tag: Error creating HTML file\n");
    ++w;

    char *q = *html + len;
    sprintf(q, k, s, s, w, s);
}

//ATTENTION: "imagemagick" package needed
// Used to build the html homepage and the dynamic list of images searching for them
//  in the current folder (favicon.ico) and in the folder specified by the user.
void creat_imgList_html(int perc) {

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
    memset(input, (int) '\0', DIM);
    memset(output, (int) '\0', DIM);

    errno = 0;
    dirCorr = opendir(".");
    if (!dirCorr) {
        if (errno == EACCES)
            error_found("Permission denied\n");
        error_found("creat_imgList_html: Impossible to open current directory\n");
    }
    while ((entCurr = readdir(dirCorr)) != NULL) {
        if (entCurr->d_type == DT_REG) {
            if (!strcmp(entCurr->d_name, "favicon.ico")) {
                char pathFav[DIM], pwd[DIM];
                if (getcwd(pwd, sizeof(pwd)) == NULL)
                    perror("creat_imgList_html: not able to get current directory path");
                sprintf(pathFav, "%s/%s", pwd, entCurr->d_name);
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
            error_found("creat_imgList_html: Permission denied\n");
        printf("ATTENTION: remember to add images directory path after -i to the list of program arguments\n\n");
        error_found("creat_imgList_html: Impossible to open images directory\n");
    }

    size_t len_h = strlen(html), new_len_h;

    fprintf(stdout, "Doing initialization operations on images, please wait\n");
    while ((ent = readdir(dir)) != NULL) {
        ++number_of_img;
        if (ent->d_type == DT_REG) {
            if (strrchr(ent->d_name, '~')) {
                fprintf(stderr, "File '%s' was skipped\n", ent->d_name);
                continue;
            }
            if ((k = strrchr(ent->d_name, '.'))) {
                if (strcmp(k, ".gif") != 0 && strcmp(k, ".GIF") != 0 &&
                    strcmp(k, ".jpg") != 0 && strcmp(k, ".JPG") != 0 &&
                    strcmp(k, ".png") != 0 && strcmp(k, ".PNG") != 0)
                    fprintf(stderr, "File '%s' may have an supported format\n", ent->d_name);
            } else {
                fprintf(stderr, "File '%s' may have an supported format\n", ent->d_name);
            }

            char command[DIM * 2];
            memset(command, (int) '\0', DIM * 2);
            sprintf(input, "%s/%s", src_path, ent->d_name);
            sprintf(output, "%s/%s", img_home, ent->d_name);
            sprintf(command, convert, input, perc, output);
            //operation made by imagemagick
            if (system(command))
                error_found("creat_imgList_html: Error imagemagick not able to resize\n");

            insert_img(i, output);
            i = &(*i)->next_img;
            add_img_tag(ent->d_name, &html, &dim);
        }
    }

    new_len_h = strlen(html);
    if (len_h == new_len_h)
        error_found("creat_imgList_html: There aren't images in the chosen directory\n");

    h = "</body></html>";
    if (new_len_h + DIM2 / 4 > dim * DIM) {
        ++dim;
        html = realloc(html, (size_t) dim * DIM);
        if (!html)
            error_found("creat_imgList_html: Error in realloc\n");
        memset(html + new_len_h, (int) '\0', (size_t) dim * DIM - new_len_h);
    }
    k = html;
    k += strlen(html);
    strcpy(k, h);

    HTML[0] = html;

    if (closedir(dir))
        error_found("creat_imgList_html: Error in closedir\n");

    //Size of cache is setted to an appropriate number
    if (!cache_size) {
        if (number_of_img != 0)
            cache_size = ((number_of_img - 2) * 30);
    }
    fprintf(stdout, "Images resized with default quality settings in: '%s' \n", img_home);
}

// Allocates responses with error 400 or error 404
void creat_reply_error() {
    printf("cache size dopo %d\n", cache_size);
    char *s = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head>\t<link rel=\"shortcut icon\" href=\"/favicon.ico\">\n"
            " <title>%s</title></head><body><h1>%s</h1><p>%s</p></body></html>\0";
    size_t len = strlen(s) + 2 * DIM2 * sizeof(char);

    char *mm1 = malloc(len);
    char *mm2 = malloc(len);
    if (!mm1 || !mm2)
        error_found(" creat_reply_error: Error in malloc\n");
    memset(mm1, (int) '\0', len); memset(mm2, (int) '\0', len);
    sprintf(mm1, s, "404 Not Found", "404 Not Found", "The requested URL was not found on this server.");
    sprintf(mm2, s, "400 Bad Request", "Bad Request", "Your browser sent a request that this server could not understand.");
    HTML[1] = mm1;
    HTML[2] = mm2;
}



int parse_q_factor(char *h_accept) {
   /*
    double q;
    char *chr;

    if(h_accept) {
        chr = strrchr(h_accept, 'q');
        if (!chr)
            q = 1.0;
        else {
            errno = 0;
            q = strtod(chr + 2, NULL);

            if (errno != 0)
                return -1;
        }
    }else q =0.1;
    */

    double images, others, q;
    images = others = q = -2.0;
    char *chr, *t1 = strtok(h_accept, ",");
    if (!h_accept || !t1) {
        return (int) (q * 100);
    }
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
        fprintf(stderr, "parse_q_factor: quality factor not found in '%s\t\t\n", h_accept);

    return (int) (q * 100);
}

// Find and send resource for client
int complete_http_reply(char **line, char *log_string, char **http_rep, ssize_t *dim_t) {
    *http_rep = malloc(DIM * DIM * 2 * sizeof(char));
    if (!*http_rep)
        error_found("complete_http_reply: Error in malloc\n");
    memset(*http_rep, (int) '\0',DIM * DIM * 2);

    // %d status code; %s status code; %s date; %s server; %s content type; %d content's length; %s connection type
    char *header = "HTTP/1.1 %d %s\r\nDate: %s\r\nServer: %s\r\nAccept-Ranges: bytes\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: %s\r\n\r\n";
    //char *t = get_time();
    char *server = "ProjectIIW";
    char *h;

    //build the reply in case of bad request with the 400 message error

    if (!line[0] || !line[1] || !line[2] ||
        ((strncmp(line[0], "GET", 3) && strncmp(line[0], "HEAD", 4)) ||
         (strncmp(line[2], "HTTP/1.1", 8) && strncmp(line[2], "HTTP/1.0", 8)))) {
        sprintf(*http_rep, header, 400, "Bad Request", get_time(), server, "text/html", strlen(HTML[2]), "close");
        h = *http_rep;
        h += strlen(*http_rep);
        memcpy(h, HTML[2], strlen(HTML[2]));

        strcat(log_string,"400 Bad Request");
        *dim_t=strlen(*http_rep);
        return 0;
    }

    if (strncmp(line[1], "/", strlen(line[1])) == 0) {
        sprintf(*http_rep, header, 200, "OK", get_time(), server, "text/html", strlen(HTML[0]), "keep-alive");
        if (strncmp(line[0], "HEAD", 4)) {
            h = *http_rep;
            h += strlen(*http_rep);
            memcpy(h, HTML[0], strlen(HTML[0]));
        }
        strcat(log_string,"200 OK");
        *dim_t=strlen(*http_rep);
    }else {
        struct image *i = img;
        char *p_name;
        if (!(p_name = strrchr(line[1], '/')))
            i = NULL;
        ++p_name;
        char *p = img_home + strlen("/tmp");
        // Finding image in the image list
        while (i) {
            if (!strncmp(p_name, i->name, strlen(i->name))) {
                ssize_t dim_img = 0;
                char *img_to_send = NULL;
                int favicon = 1;
                if (!strncmp(p, line[1], strlen(p) - strlen(".XXXXXX")) || !(favicon = strncmp(p_name, "favicon.ico", strlen("favicon.ico")))) {
                    // Looking for resized image or favicon.ico
                    if (strncmp(line[0], "HEAD", 4)) {
                        img_to_send = get_img(p_name, i->size_r, favicon ? img_home : ".");
                        if (!img_to_send) {
                            fprintf(stderr, "complete_http_reply: Error in get_img\n");
                            return -1;
                        }
                    }
                    dim_img = i->size_r;
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
                    printf("QUAL: %d\n", q);
                    c = i->img_c;
                    while (c) {
                        if (c->q == q) {
                            strcpy(name_cached_img, c->img_q);
                            // If an image has been accessed, move it on top of the list
                            //  in order to keep the image with less hit in the bottom of the list
                            if (cache_size >= 0 && strncmp(cache_hit_head->cache_name, name_cached_img, strlen(name_cached_img))) {
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
                        sprintf(path, "%s/%s", img_cache, name_cached_img);

                        if (cache_size > 0) {
                            // If it has not yet reached
                            //  the maximum cache size
                            // %s/%s = path/name_image; %d = factor quality
                            char *format = "convert %s/%s -quality %d %s/%s;exit";
                            char command[DIM];
                            memset(command, (int) '\0', DIM);
                            sprintf(command, format, src_path, p_name, q, img_cache, name_cached_img);
                            if (system(command)) {
                                fprintf(stderr, "complete_http_reply: Unexpected error while refactoring image\n");

                                return -1;
                            }

                            struct stat buf;
                            memset(&buf, (int) '\0', sizeof(struct stat));
                            errno = 0;
                            if (stat(path, &buf) != 0) {
                                if (errno == ENAMETOOLONG) {
                                    fprintf(stderr, "complete_http_reply: Path too long\n");

                                    return -1;
                                }
                                fprintf(stderr, "complete_http_reply: Invalid path\n");

                                return -1;
                            } else if (!S_ISREG(buf.st_mode)) {
                                fprintf(stderr, "Non-regular files can not be analysed!\n");

                                return -1;
                            }

                            struct cache *new_entry = malloc(sizeof(struct cache));
                            struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                            memset(new_entry, (int) '\0', sizeof(struct cache));
                            memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                            if (!new_entry || !new_hit) {
                                fprintf(stderr, "complete_http_reply: Error in malloc\n");

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
                            printf("%d\n", cache_size);
                            --cache_size;
                            printf("dopo decr: %d\n", cache_size);
                        } else if (!cache_size){
                            printf("caso cache piena: %d\n", cache_size);
                            // Cache full.
                            // You have to delete an item.
                            // You choose to delete the oldest requested element.
                            char name_to_remove[DIM / 2];
                            memset(name_to_remove, (int) '\0', DIM / 2);
                            sprintf(name_to_remove, "%s/%s", img_cache, cache_hit_tail->cache_name);

                            DIR *dir;
                            struct dirent *ent;
                            errno = 0;
                            dir = opendir(img_cache);
                            if (!dir) {
                                if (errno == EACCES) {
                                    fprintf(stderr, "complete_http_reply: Error in opendir: Permission denied\n");

                                    return -1;
                                }
                                fprintf(stderr, "complete_http_reply: Error in opendir\n");

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

                                return -1;
                            }

                            // %s/%s = path/name_image; %d = factor quality
                            char *format = "convert %s/%s -quality %d %s/%s;exit";
                            char command[DIM];
                            memset(command, (int) '\0', DIM);
                            sprintf(command, format, src_path, p_name, q, img_cache, name_cached_img);
                            if (system(command)) {
                                fprintf(stderr, "complete_http_reply: Unexpected error while refactoring image\n");

                                return -1;
                            }

                            struct stat buf;
                            memset(&buf, (int) '\0', sizeof(struct stat));
                            //---___----DA TOGLIERE E USARE LA "CTRL_STAT"------__----__---__--
                            errno = 0;
                            if (stat(path, &buf) != 0) {
                                if (errno == ENAMETOOLONG) {
                                    fprintf(stderr, "Path too long\n");

                                    return -1;
                                }
                                fprintf(stderr, "complete_http_reply: Invalid path\n");

                                return -1;
                            } else if (!S_ISREG(buf.st_mode)) {
                                fprintf(stderr, "Non-regular files can not be analysed!\n");

                                return -1;
                            }
                            //__----__---_---_-----______-----__--_----_____-------------____
                            struct cache *new_entry = malloc(sizeof(struct cache));
                            memset(new_entry, (int) '\0', sizeof(struct cache));
                            if (!new_entry) {
                                fprintf(stderr, "complete_http_reply: Error in malloc\n");

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
                                        cache_size = -1;
                                        return -1;
                                    }
                                    break;
                                }
                                img_ptr = img_ptr->next_img;
                            }
                            if (!img_ptr) {
                                cache_size = -1;
                                fprintf(stderr, "complete_http_reply: Unexpected error while looking for image in struct image\n"
                                        "-Cache size automatically set to Unlimited\n\t\tfinding: %s\n", name_i);

                                return -1;
                            }

                            struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                            memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                            if (!new_hit) {
                                fprintf(stderr, "complete_http_reply: Error in malloc\n");

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
                        dir = opendir(img_cache);
                        if (!dir) {
                            if (errno == EACCES) {
                                fprintf(stderr, "complete_http_reply: Error in opendir: Permission denied\n");
                                return -1;
                            }
                            fprintf(stderr, "complete_http_reply: Error in opendir\n");
                            return -1;
                        }

                        while ((ent = readdir(dir)) != NULL) {
                            if (ent->d_type == DT_REG) {
                                if (!strncmp(ent->d_name, name_cached_img, strlen(name_cached_img))) {
                                    img_to_send = get_img(name_cached_img, c->size_q, img_cache);
                                    if (!img_to_send) {
                                        fprintf(stderr, "complete_http_reply: Error in get_img\n");
                                        return -1;
                                    }
                                    break;
                                }
                            }
                        }

                        if (closedir(dir)) {
                            fprintf(stderr, "complete_http_reply: Error in closedir\n");
                            free(img_to_send);
                            return -1;
                        }
                    }
                    dim_img = c->size_q;
                }

                sprintf(*http_rep, header, 200, "OK", get_time(), server, "image/gif", dim_img, "keep-alive");
                ssize_t dim_tot = (size_t) strlen(*http_rep);
                if (strncmp(line[0], "HEAD", 4)) {
                    if (dim_tot + dim_img > DIM * DIM * 2) {
                        *http_rep = realloc(*http_rep, (dim_tot + dim_img) * sizeof(char));
                        if (!*http_rep) {
                            fprintf(stderr, "complete_http_reply: Error in realloc\n");
                            free(img_to_send);
                            return -1;
                        }
                        memset(*http_rep + dim_tot, (int) '\0', (size_t) dim_img);
                    }
                    h = *http_rep;
                    h += dim_tot;
                    memcpy(h, img_to_send, (size_t) dim_img);
                    dim_tot += dim_img;

                }
                *dim_t=dim_tot;
                free(img_to_send);
                break;
            }
            i = i->next_img;
        }

        if (!i) {
            sprintf(*http_rep, header, 404, "Not Found", get_time(), server, "text/html", strlen(HTML[1]), "close");
            if (strncmp(line[0], "HEAD", 4)) {
                h = *http_rep;
                h += strlen(*http_rep);
                memcpy(h, HTML[1], strlen(HTML[1]));
            }
            printf("%s\n", *http_rep);
            strcat(log_string,"404 Not Found");
            *dim_t=strlen(*http_rep);
        }else strcat(log_string,"200 OK");
    }

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


int init(int argc, char **argv){

    log_file= open_file();
    int perc = 50;
    int port = get_opt(argc, argv,&perc);

    if (!mkdtemp(img_home) || !mkdtemp(img_cache))
        error_found("Error in mkdtemp\n");
    creat_imgList_html(perc);
    creat_reply_error();
    //to manage SIGPIPE signal
    catch_signal();
    return port;

}
