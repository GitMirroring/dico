#include <config.h>
#include <string.h>
#include <errno.h>
#include <dico.h>
#include <dico/conv.h>

static dico_list_t /* of struct dico_conv */ conv_list;

static int
conv_name_cmp(const void *item, const void *data, void *unused)
{
    dico_conv_t p = (dico_conv_t) item;
    const char *name = data;
    return strcmp(p->name, name);
}

static int
conv_free(void *item, void *data)
{
    free(item);
    return 0;
}

int
dico_conv_register(char const *name, dico_convfun_t fun, void *data)
{
    dico_conv_t cp;

    if (!conv_list) {
	conv_list = dico_list_create();
	if (!conv_list)
	    return -1;
	dico_list_set_comparator(conv_list, conv_name_cmp, NULL);
	dico_list_set_free_item(conv_list, conv_free, NULL);
    }
	
    if (dico_conv_find(name)) {
	errno = EEXIST;
	return -1;
    }
    
    cp = malloc(sizeof(*cp) + strlen(name) + 1);
    if (!cp)
	return -1;
    cp->name = (char*)(cp + 1);
    strcpy(cp->name, name);
    cp->conv = fun;
    cp->data = data;

    dico_list_append(conv_list, cp);

    return 0;
}

dico_conv_t
dico_conv_find(char const *name)
{
    return dico_list_locate(conv_list, (void*)name);
}
    
int
dico_conv_apply(dico_list_t list, char const *input, char **output)
{
    dico_iterator_t itr;
    dico_conv_t cp;
    char *tmp = NULL;
    
    itr = dico_list_iterator(list);
    if (!itr)
	return -1;

    for (cp = dico_iterator_first(itr); cp; cp = dico_iterator_next(itr)) {
	int rc;
	char *p;

	rc = cp->conv(cp->data, input, &p);
	free(tmp);
	if (rc) {
	    free(p);
	    return rc;
	}
	tmp = p;
	input = tmp;
    }

    dico_iterator_destroy(&itr);
    *output = tmp;
    return 0;
}
