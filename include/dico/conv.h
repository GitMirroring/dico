#ifndef __dico_conv_h
#define __dico_conv_h

#include <dico/types.h>
#include <dico/list.h>

typedef int (*dico_convfun_t) (void *, char const *, char **);

typedef struct dico_conv {
    char *name;
    dico_convfun_t conv;
    void *data;
} *dico_conv_t;

int dico_conv_register(char const *name, dico_convfun_t fun, void *data);
dico_conv_t dico_conv_find(char const *name);
int dico_conv_apply(dico_list_t list, char const *input, char **output);

#endif
