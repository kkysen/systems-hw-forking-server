//
// Created by kkyse on 12/12/2017.
//

#include "client.h"

#include <unistd.h>

#include "pipe_networking.h"
#include "util/stacktrace.h"
#include "util/io.h"
#include "util/utils.h"

static int send_receive_and_print_text(const int in_fd, const int out_fd, const String text) {
//    pz(text.length);
    if (text.length == 0) {
        return -1; // no input
    }
    
    text.chars[text.length] = 0; // cutoff '\n' at end
    printf("Entered: \"%s\"\n", text.chars);
    
    check(write_string(out_fd, text));
    
    const String modified_text = read_string(in_fd);
    if (!modified_text.chars) {
        return -1;
    }
    
    printf("Modified Text from Server: \"%s\"\n", modified_text.chars);
    
    free(modified_text.chars);
    return 0;
}

static int read_send_receive_and_print_text(const int in_fd, const int out_fd) {
    size_t buf_size;
    char *line = NULL;
    printf("Enter Text: ");
    fflush(stdout);
    const ssize_t signed_length = getline(&line, &buf_size, stdin);
//    pd(signed_length);
    check(signed_length);
    if (signed_length == 0) {
//        p("\tNo input, exiting");
        return -1;
    }
    
    const int ret_val = send_receive_and_print_text(
            in_fd, out_fd, (String) {.length = (size_t) signed_length - 1, .chars = line});
//    pd(ret_val);
    free(line);
    return ret_val;
}

int run() {
    const TwoWayPipe pipes = client_handshake_with_server(SERVER_PIPE);
    if (pipes.error == -1) {
        return -1;
    }
    
    while (read_send_receive_and_print_text(pipes.in_fd, pipes.out_fd) != -1) {
//        perror("");
    }
//    p("Finishing");
    check(close(pipes.in_fd));
    check(close(pipes.out_fd));
    return 0;
}

void exit_cleanly_on_sigint(const int signal) {
    if (signal == SIGINT) {
        printf("\n");
        p("\tExiting");
        exit(EXIT_SUCCESS);
    }
}

int main() {
    set_stack_trace_signal_handler();
    
    add_signal_handler(SIGINT, &exit_cleanly_on_sigint);
    
    if (run() == -1) {
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}