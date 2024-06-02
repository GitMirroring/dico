/* This file is part of GNU Dico.
   Copyright (C) 2024 Sergey Poznyakoff

   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

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
