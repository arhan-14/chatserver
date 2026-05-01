#ifndef CHATD_H
#define CHATD_H

typedef struct User {
    int fd;
    int active;
    int has_name;
    char name[33];
    char status[65];
} User;

typedef struct Message {
    char version;
    char code[4];
    int body_len;
    char sender[33];
    char recipient[33]; 
    char content[81];
} Message;

enum HeaderResult {
    HEADER_VALID,
    NEED_MORE,
    BAD_HEADER
};

enum ValidationResult {
    VALID = -1,
    ERR_UNREADABLE = 0,
    ERR_NAME_IN_USE = 1,
    ERR_UNKNOWN_USER = 2,
    ERR_ILLEGAL_CHAR = 3,
    ERR_TOO_LONG = 4
};

#endif
