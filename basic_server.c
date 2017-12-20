//
// Created by kkyse on 12/12/2017.
//

#include "basic_server.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pipe_networking.h"
#include "util/utils.h"
#include "util/stacktrace.h"
#include "util/io.h"
#include "modify_text.h"

int run() {
    const TwoWayPipe pipes = server_handshake_with_client(SERVER_PIPE);
    if (pipes.error == -1) {
        return -1;
    }
    
    size_t length;
    {
        const ssize_t bytes_read = read(pipes.in_fd, &length, sizeof(length));
        if (bytes_read != sizeof(length)) {
            if (bytes_read == -1) {
                perror("read(pipes.in_fd, &length, sizeof(length))");
                goto error_close;
            } else {
//                fprintf(stderr, "%zd != %zu", bytes_read, sizeof(length));
                length = (size_t) bytes_read;
            }
        }
    }
    
    if (length == 0 || errno == EPIPE) {
        printf("Client terminated session.\n");
        close(pipes.in_fd);
        close(pipes.out_fd);
        return 0;
    }

//    pz(length);
    char *const text = (char *) calloc(length + 1, sizeof(char));
    if (!text) {
        perror("calloc(length + 1, sizeof(char))");
        goto error_close;
    }
    
    {
        const ssize_t bytes_read = read(pipes.in_fd, text, length);
        if (bytes_read != length) {
            perror("read(pipes.in_fd, text, length)");
            if (bytes_read == -1) {
                goto error_free;
            } else {
                fprintf(stderr, "%zd != %zu", bytes_read, length);
            }
        }
    }
    
    printf("Received: \"%s\"\n", text);
    modify_text(text, length);
    printf("Modified: \"%s\"\n", text);
    
    {
        const ssize_t bytes_written = write(pipes.out_fd, &length, sizeof(length));
        if (bytes_written != sizeof(length)) {
            perror("write(pipes.out_fd, &length, sizeof(length))");
            if (bytes_written == -1) {
                goto error_free;
            } else {
                fprintf(stderr, "%zd != %zu", bytes_written, sizeof(length));
            }
        }
    }
    
    {
        const ssize_t bytes_written = write(pipes.out_fd, text, length);
        if (bytes_written != length) {
            perror("write(pipes.out_fd, text, length)");
            if (bytes_written == -1) {
                goto error_free;
            } else {
                fprintf(stderr, "%zd != %zu", bytes_written, length);
            }
        }
    }
    
    free(text);
    close(pipes.in_fd);
    close(pipes.out_fd);
    return 0;
    
    error_free:
    free(text);
    
    error_close:
    close(pipes.in_fd);
    close(pipes.out_fd);
    return -1;
}

void remove_server_pipe() {
    printf("removed server_pipe\n");
    unlink_if_exists(SERVER_PIPE);
}

int main() {
    set_stack_trace_signal_handler();
    
    p("THIS BASIC SERVER IS DEPRECATED");
    p("IT MAY NOT WORK CORRECTLY ANYMORE BECAUSE OF CHANGES MADE FOR THE FORKING SERVER");
    p("USE THE FORKING SERVER INSTEAD (`./fserver` or `make run`)");
    
    if (atexit(remove_server_pipe) != 0) {
        perror("atexit(remove_server_pipe)");
    }
    
    for (;;) {
        if (run() == -1) {
            return EXIT_FAILURE;
        }
    }
    
    return EXIT_SUCCESS;
}