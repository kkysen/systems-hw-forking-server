//
// Created by kkyse on 12/12/2017.
//

#include "pipe_networking.h"
#include "util/utils.h"
#include "util/random.h"
#include "util/hash.h"
#include "util/io.h"
#include "util/string_utils.h"
#include "util/sigaction.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#define NUM_RANDOM_LETTERS 64

#ifndef NAME_MAX
    #define NAME_MAX 255
#endif

const TwoWayPipe BAD_TWO_WAY_PIPE = {.error = -1};

static char client_owned_client_pipe_name[NAME_MAX + 1] = {0};

static char server_owned_client_pipe_name[NAME_MAX + 1] = {0};

static void print_removed_pipe(const char *const path) {
    printf("Removed Pipe: \"%s\"\n", path);
}

static void remove_client_owned_client_pipe() {
    if (client_owned_client_pipe_name[0]) {
        unlink_if_exists_and_then(client_owned_client_pipe_name, print_removed_pipe);
    }
}

static void remove_server_owned_client_pipe() {
    if (server_owned_client_pipe_name[0]) {
        unlink_if_exists_and_then(server_owned_client_pipe_name, print_removed_pipe);
    }
}

static void remove_client_owned_client_pipe_on_sigint(const int signal) {
    if (signal == SIGINT) {
        remove_client_owned_client_pipe();
    }
}

static void remove_server_owned_client_pipe_on_sigint(const int signal) {
    if (signal == SIGINT) {
        remove_server_owned_client_pipe();
    }
}

static void run_on_exit_and_sigint(const Runnable on_exit, const SignalHandler on_sigint) {
    if (atexit(on_exit) != 0) {
        perror("atexit(on_exit)");
    }
    add_signal_handler(SIGINT, on_sigint);
}

static int send_acknowledgement(const int fd) {
    check(write_int(fd, ACKNOWLEDGEMENT_SIGNAL));
    return 0;
}

static int verify_acknowledgement(const int fd) {
    int acknowledgment_signal = 0;
    check(read_int(fd, &acknowledgment_signal));
    
    if (acknowledgment_signal != ACKNOWLEDGEMENT_SIGNAL) {
        fprintf(stderr, "Invalid Acknowledgment Signal: %x != %x\n",
                acknowledgment_signal, ACKNOWLEDGEMENT_SIGNAL);
        return -1;
    }
    
    return 0;
}

static int create_in_pipe(const char *const pipe_name) {
    // only this program will read, all other programs only write
    const mode_t in_pipe_mode = S_IRUSR | S_IWUSR | S_IWGRP | S_IWGRP;
    check_perror(mkfifo(pipe_name, in_pipe_mode));
    return 0;
}

static int create_and_open_server_in_pipe(const char *const pipe_name) {
    check(create_in_pipe(pipe_name));
    printf("Created Server Pipe: \"%s\"\n", pipe_name);
    printf("Waiting for Client\n");
    const int in_fd = open(pipe_name, O_RDONLY);
    check_msg(in_fd, "open(pipe_name, O_RDONLY)");
    return in_fd;
}

static int read_pipe_name(const int in_fd, char *const out_pipe_name, size_t *name_length) {
    check(verify_acknowledgement(in_fd));
    
    const size_t name_max = *name_length;
    const ssize_t signed_out_pipe_name_length = read(in_fd, out_pipe_name, name_max);
    check_msg(signed_out_pipe_name_length, "read(in_fd, out_pipe_name, name_max)");
    const size_t out_pipe_name_length = (size_t) signed_out_pipe_name_length;
    *name_length = out_pipe_name_length;
    out_pipe_name[out_pipe_name_length] = 0; // set null-terminator in case it wasn't send over the pipe
    printf("Client pipe received: \"%s\"\n", out_pipe_name);
    
    return 0;
}

static int open_out_pipe(const char *const out_pipe_name) {
    const int out_fd = open(out_pipe_name, O_WRONLY);
    check_msg(out_fd, "open(out_pipe_name, O_WRONLY");
    strcpy(server_owned_client_pipe_name, out_pipe_name);
    run_on_exit_and_sigint(
            remove_server_owned_client_pipe, remove_server_owned_client_pipe_on_sigint);
    return out_fd;
}

static int subserver_handshake_with_client(const int out_fd,
                                           const char *const in_pipe_name,
                                           const char *const out_pipe_name,
                                           const size_t out_pipe_name_length) {
    check(send_acknowledgement(out_fd));
    check(send_sha256(out_fd, out_pipe_name, out_pipe_name_length));
    check(unlink_if_exists(out_pipe_name));
//    check(unlink_if_exists(in_pipe_name));
    return 0;
}

static int server_handshake_with_client_existing_in_pipe(const int in_fd,
                                                         const char *const in_pipe_name) {
    char out_pipe_name[NAME_MAX + 1];
    size_t out_pipe_name_length = NAME_MAX;
    check(read_pipe_name(in_fd, out_pipe_name, &out_pipe_name_length));
    const int out_fd = open_out_pipe(out_pipe_name);
    check(subserver_handshake_with_client(
            out_fd, in_pipe_name, out_pipe_name, out_pipe_name_length));
    return out_fd;
}

TwoWayPipe server_handshake_with_client(const char *in_pipe_name) {
    const int in_fd = create_and_open_server_in_pipe(in_pipe_name);
    if (in_fd == -1) {
        return BAD_TWO_WAY_PIPE;
    }
    
    const int out_fd = server_handshake_with_client_existing_in_pipe(in_fd, in_pipe_name);
    if (out_fd == -1) {
        if (close(in_fd) == -1) {
            perror("close(in_fd)");
        }
        return BAD_TWO_WAY_PIPE;
    }
    
    printf("Handshake with client finished: in = %d, out = %d\n", in_fd, out_fd);
    return (TwoWayPipe) {.in_fd = in_fd, .out_fd = out_fd};
}

static ssize_t generate_random_pipe_name_of_size(char *const pipe_name,
                                                 const size_t name_max,
                                                 const size_t max_num_random_letters) {
    pipe_name[name_max] = 0; // to be safe
    const int signed_offset = snprintf(pipe_name, name_max, "%d", getpid());
    check_msg(signed_offset, "snprintf(pipe_name, name_max, \"%d\", getpid())");
    const size_t offset = (size_t) signed_offset;
    const size_t num_random_letters = min(max_num_random_letters, name_max - offset);
    random_lowercase_letters(pipe_name + offset, num_random_letters);
    const size_t name_length = offset + num_random_letters;
    pipe_name[name_length] = 0;
    return name_length;
}

static int open_with_timeout(const char *const path, int flags, uint seconds) {
    int fd;
    uint elapsed = 0;
    const uint interval = 1;
    do {
        fd = open(path, flags);
        if (fd != -1) {
            return fd;
        }
        sleep(interval);
        elapsed += interval;
    } while (elapsed < seconds);
    return -1;
}

typedef unsigned int uint;

static int connect_to_out_pipe(const char *const out_pipe_name) {
    const int out_fd = open_with_timeout(out_pipe_name, O_WRONLY, 3);
    if (out_fd == -1) {
        perror("open_with_timeout(out_pipe_name, O_WRONLY, 3)");
        printf("open_with_timeout(%s, O_WRONLY, 3): %s\n", out_pipe_name, strerror(errno));
        return -1;
    }
//    check(unlink_if_exists(out_pipe_name));
    return out_fd;
}

static ssize_t create_secure_pipe(char *const pipe_name, const size_t name_max) {
    const ssize_t signed_name_length =
            generate_random_pipe_name_of_size(pipe_name, name_max, NUM_RANDOM_LETTERS);
    check(signed_name_length);
    const size_t name_length = (size_t) signed_name_length;
    check(create_in_pipe(pipe_name));
    run_on_exit_and_sigint(
            remove_client_owned_client_pipe, remove_client_owned_client_pipe_on_sigint);
    return name_length;
}

static int verify_secure_in_pipe_name_hash(const int in_fd,
                                           const char *const in_pipe_name, const size_t name_length) {
    check(verify_sha256(in_fd, in_pipe_name, name_length, "Client Pipe"));
    return 0;
}

static int securely_connect_to_client_in_pipe(const char *const in_pipe_name, const size_t name_length) {
    const int in_fd = open(in_pipe_name, O_RDONLY);
    check_msg(in_fd, "open(in_pipe_name, O_RDONLY)");
    check(verify_acknowledgement(in_fd));
    check(verify_secure_in_pipe_name_hash(in_fd, in_pipe_name, name_length));
    check(unlink_if_exists(in_pipe_name));
    return in_fd;
}

static int client_handshake_with_server_existing_out_pipe(const int out_fd) {
    _Static_assert(NUM_RANDOM_LETTERS < NAME_MAX, "NUM_RANDOM_LETTERS is too big, >= NAME_MAX");
    char *const in_pipe_name = client_owned_client_pipe_name;
    const ssize_t signed_name_length = create_secure_pipe(in_pipe_name, NAME_MAX);
    check(signed_name_length);
    const size_t name_length = (size_t) signed_name_length;
    
    printf("Client pipe created: \"%s\"\n", in_pipe_name);
    
    check(send_acknowledgement(out_fd));
    
    // must send name to server before opening pipe, b/c will block on open()
    check(write_bytes(out_fd, in_pipe_name, name_length));
    
    const int in_fd = securely_connect_to_client_in_pipe(in_pipe_name, name_length);
    check(in_fd);
    return in_fd;
}

#undef NUM_RANDOM_LETTERS

TwoWayPipe client_handshake_with_server(const char *out_pipe_name) {
    const int out_fd = connect_to_out_pipe(out_pipe_name);
    if (out_fd == -1) {
        return BAD_TWO_WAY_PIPE;
    }
    
    const int in_fd = client_handshake_with_server_existing_out_pipe(out_fd);
    if (in_fd == -1) {
        if (close(out_fd) == -1) {
            perror("close(out_fd)");
        }
        return BAD_TWO_WAY_PIPE;
    }
    
    printf("Handshake with server finished: in = %d, out = %d\n", in_fd, out_fd);
    return (TwoWayPipe) {.in_fd = in_fd, .out_fd = out_fd};
}

static int run_child_subserver_function(const int in_fd, const int out_fd,
                                        const char *const in_pipe_name,
                                        const char *const out_pipe_name,
                                        const size_t out_pipe_name_length,
                                        const SubserverFunction subserver_function
) {
    check(subserver_handshake_with_client(out_fd, in_pipe_name, out_pipe_name, out_pipe_name_length));
    printf("Handshake with client finished: in = %d, out = %d\n", in_fd, out_fd);
    while (subserver_function(in_fd, out_fd) != -1) {
//        perror("");
    };
    check(close(in_fd));
    check(close(out_fd));
    return 0;
}

int run_subserver_function(const char *in_pipe_name, const SubserverFunction subserver_function) {
//    perror("");
//    p("\tRunning subserver");
    const int in_fd = create_and_open_server_in_pipe(in_pipe_name);
    check(in_fd);
    
    char out_pipe_name[NAME_MAX + 1];
    size_t out_pipe_name_length = NAME_MAX;
    check(read_pipe_name(in_fd, out_pipe_name, &out_pipe_name_length));
    const int out_fd = open_out_pipe(out_pipe_name);
    check(out_fd);
    check(unlink_if_exists(in_pipe_name));
    
    const pid_t cpid = fork();
    check(cpid);
    if (cpid == 0) {
        const int ret_val = run_child_subserver_function(
                in_fd, out_fd, in_pipe_name,
                out_pipe_name, out_pipe_name_length,
                subserver_function);
//        perror("exiting");
        exit(ret_val == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    } else {
//        perror("\tParent");
        check(close(in_fd));
        check(close(out_fd));
//        perror("\tParent returning");
        return 0;
    }
}