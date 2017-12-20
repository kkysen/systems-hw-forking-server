//
// Created by kkyse on 12/19/2017.
//

#ifndef SYSTEMS_FORKING_SERVER_H
#define SYSTEMS_FORKING_SERVER_H

#define MAX_FAILURES 10

void remove_server_pipe();

int run_subserver(int in_fd, int out_fd);

int run();

int main();

#endif //SYSTEMS_FORKING_SERVER_H
