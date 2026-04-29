#ifndef ERR_H
#define ERR_H

typedef enum error {
    HC_OK = 0,
    HC_ERR_INVALID = -1,
    HC_ERR_NWORDS = -2,
    HC_ERR_CHUNK = -3,
    HC_ERR_OOM = -4,
    HC_ERR_DICT = -5,
    HC_ERR_PARSE = -6,
    HC_ERR_RANGE = -7,
    HC_ERR_NOTFOUND = -8,
    HC_ERR_MATH = -9,
} error_t;

#endif /* ERR_H */