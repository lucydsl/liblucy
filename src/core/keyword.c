#include <stdlib.h>
#include <string.h>
#include "keyword.h"

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

dict *keywords;

#define INITIAL_SIZE (1024)
#define GROWTH_FACTOR (2)
#define MAX_LOAD_FACTOR (1)

static void dict_insert(dict*, const char*, unsigned short);

/* dictionary initialization code used in both DictCreate and grow */
static dict* internal_dict_create(int size) {
    dict *d;
    int i;

    d = malloc(sizeof(*d));

    d->size = size;
    d->n = 0;
    d->table = malloc(sizeof(struct elt *) * d->size);

    for(i = 0; i < d->size; i++) d->table[i] = 0;

    return d;
}

static dict* dict_create(void) {
    return internal_dict_create(INITIAL_SIZE);
}

static void dict_destroy(dict *d) {
    int i;
    struct elt *e;
    struct elt *next;

    for(i = 0; i < d->size; i++) {
        for(e = d->table[i]; e != 0; e = next) {
            next = e->next;

            free(e->key);
            free(e);
        }
    }

    free(d->table);
    free(d);
}

#define MULTIPLIER (97)

static unsigned long hash_function(const char *s) {
    unsigned const char *us;
    unsigned long h;

    h = 0;

    for(us = (unsigned const char *) s; *us; us++) {
        h = h * MULTIPLIER + *us;
    }

    return h;
}

static void grow(dict *d) {
    dict *d2;            /* new dictionary we'll create */
    struct dict swap;   /* temporary structure for brain transplant */
    int i;
    struct elt *e;

    d2 = internal_dict_create(d->size * GROWTH_FACTOR);

    for(i = 0; i < d->size; i++) {
        for(e = d->table[i]; e != 0; e = e->next) {
            /* note: this recopies everything */
            /* a more efficient implementation would
             * patch out the strdups inside DictInsert
             * to avoid this problem */
            dict_insert(d2, e->key, e->value);
        }
    }

    /* the hideous part */
    /* We'll swap the guts of d and d2 */
    /* then call DictDestroy on d2 */
    swap = *d;
    *d = *d2;
    *d2 = swap;

    dict_destroy(d2);
}

/* insert a new key-value pair into an existing dictionary */
static void dict_insert(dict *d, const char *key, unsigned short value) {
    struct elt *e;
    unsigned long h;

    e = malloc(sizeof(*e));

    e->key = strdup(key);
    e->value = value;

    h = hash_function(key) % d->size;

    e->next = d->table[h];
    d->table[h] = e;

    d->n++;

    /* grow table if there is not enough room */
    if(d->n >= d->size * MAX_LOAD_FACTOR) {
        grow(d);
    }
}

static unsigned short dict_search(dict *d, const char *key) {
    struct elt *e;

    for(e = d->table[hash_function(key) % d->size]; e != 0; e = e->next) {
        if(!strcmp(e->key, key)) {
            /* got it */
            return e->value;
        }
    }

    return 0;
}

/* delete the most recently inserted record with the given key */
/* if there is no such record, has no effect */
static void dict_delete(dict *d, const char *key) {
    struct elt **prev;          /* what to change when elt is deleted */
    struct elt *e;              /* what to delete */

    for(prev = &(d->table[hash_function(key) % d->size]); 
        *prev != 0; 
        prev = &((*prev)->next)) {
        if(!strcmp((*prev)->key, key)) {
            /* got it */
            e = *prev;
            *prev = e->next;

            free(e->key);
            free(e);

            return;
        }
    }
}

bool is_keyword(char* key) {
  return dict_search(keywords, key) > 0;
}

unsigned short keyword_get(char* key) {
  return dict_search(keywords, key);
}

void keyword_init() {
  keywords = dict_create();

  dict_insert(keywords, "import", KW_IMPORT);
  dict_insert(keywords, "state", KW_STATE);
  dict_insert(keywords, "initial", KW_INITIAL);
  dict_insert(keywords, "final", KW_FINAL);
  dict_insert(keywords, "action", KW_ACTION);
  dict_insert(keywords, "guard", KW_GUARD);
  dict_insert(keywords, "assign", KW_ASSIGN);
  dict_insert(keywords, "invoke", KW_INVOKE);
  dict_insert(keywords, "machine", KW_MACHINE);
}