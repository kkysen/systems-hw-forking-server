//
// Created by kkyse on 12/12/2017.
//

#ifndef SYSTEMS_CLIENT_H
#define SYSTEMS_CLIENT_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

int run();

int main();

#endif // SYSTEMS_CLIENT_H
