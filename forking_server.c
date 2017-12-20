//
// Created by kkyse on 12/19/2017.
//

#include "forking_server.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "pipe_networking.h"
#include "util/stacktrace.h"
#include "util/io.h"
#include "util/utils.h"
#include "modify_text.h"

static void print_removed_server_pipe(const char *const path) {
    printf("Removed Server Pipe: \"%s\"\n", path);
}

void remove_server_pipe() {
    unlink_if_exists_and_then(SERVER_PIPE, print_removed_server_pipe);
}

static void remove_server_pipe_on_sigint(const int signal) {
    if (signal == SIGINT) {
        printf("\n");
        remove_server_pipe();
        exit(EXIT_SUCCESS);
    }
}

static int modify_and_send_text(const int in_fd, const int out_fd, char *const text, const size_t length) {
    check(read_bytes(in_fd, text, length));
    
    printf("Received: \"%s\"\n", text);
    modify_text(text, length);
    printf("Modified: \"%s\"\n", text);
    
    check(write_string(out_fd, (String) {.length = length, .chars = text}));
    return 0;
}

static int receive_modify_and_send_text(const int in_fd, const int out_fd) {
//    perror("");
    
    size_t length;
    if (read_size(in_fd, &length) == -1) {
        printf("Client terminated session.\n");
        return -1;
    }
    
//    perror("");
    
    if (length == 0 || errno == EPIPE) {
        printf("Client terminated session.\n");
        return -1;
    }
    
    char *const text = (char *) calloc(length + 1, sizeof(char));
    if (!text) {
        perror("calloc(length + 1, sizeof(char))");
        return -1;
    }
    
    const int ret_val = modify_and_send_text(in_fd, out_fd, text, length);
    free(text);
    return ret_val;
}

int run_subserver(const int in_fd, const int out_fd) {
//    perror("\tStarting subserver");
    const int ret_val = receive_modify_and_send_text(in_fd, out_fd);
//    perror("\tFinished subserver");
//    pd(ret_val);
//    perror("");
    return ret_val;
}

int run() {
//    perror("\tRunning fserver");
    return run_subserver_function(SERVER_PIPE, run_subserver);
}

int main() {
    set_stack_trace_signal_handler();
    
//    if (atexit(remove_server_pipe) != 0) {
//        perror("atexit(remove_server_pipe)");
//    }
    
    pre_stack_trace = remove_server_pipe;
    add_signal_handler(SIGINT, &remove_server_pipe_on_sigint);
    
    uint32_t num_failures = 0;
    for (;;) {
        if (num_failures >= MAX_FAILURES) {
            fprintf(stderr,
                    "Subserver Failure: subserver failed too many times (%u times)\n",
                    num_failures);
            remove_server_pipe();
            return EXIT_FAILURE;
        }
        if (run() == -1) {
//            p("\tone subserver failure");
            ++num_failures;
        }
    }
    
    return EXIT_SUCCESS;
}