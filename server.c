#include "basic.h"
#include "utils.h"


int main(int argc, char **argv)
{

    init(argc,argv);
    catch_signal();
    listen_connections();

    start_multiplexing_io();


}