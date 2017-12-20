//
// Created by kkyse on 12/19/2017.
//

#include "modify_text.h"

#define in_range(c, start, end) ((c) >= (start) && (c) <= (end))

#define is_lowercase(c) in_range(c, 'a', 'z')

#define is_uppercase(c) in_range(c, 'A', 'Z')

//static void not(char *const text, const size_t length) {
//    const size_t long_length = length / (sizeof(size_t) / sizeof(char));
//    size_t *long_text = (size_t *) text;
//    size_t *long_end = long_text + long_length;
//    --long_text;
//    while (++long_text < long_end) {
//        *long_text = ~*long_text;
//    }
//
//    char *remaining_text = ((char *) long_text) - 1;
//    char *end = text + length;
//    while (++remaining_text < end) {
//        *remaining_text = ~*remaining_text;
//    }
//}

static void invert_case(char *const text, const size_t length) {
    for (size_t i = 0; i < length; ++i) {
        char c = text[i];
        if (is_lowercase(c)) {
            c += 'A' - 'a';
        } else if (is_uppercase(c)) {
            c += 'a' - 'A';
        }
        text[i] = c;
    }
}

void modify_text(char *const text, const size_t length) {
    invert_case(text, length);
}