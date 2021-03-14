#pragma once

struct elt {
    struct elt *next;
    char *key;
    unsigned short value;
};

typedef struct dict {
    int size;           /* size of the pointer table */
    int n;              /* number of elements stored */
    struct elt **table;
} dict;

dict* dict_create(void);
void dict_insert(dict*, const char*, unsigned short);
unsigned short dict_search(dict*, const char*);