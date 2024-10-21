#include <config.h>
#include "dico.h"

static const unsigned start[] = {
     0,
     9,    32,  5760,  8192,  8200,  8232, 12288,
};

static int count[] = {
     0,
     5,     1,     1,     7,     4,     2,     1,
};

int
utf8_wc_is_space(unsigned wc)
{
    return utf8_table_check(wc, start, count, DICO_ARRAY_SIZE(start));
}

char *
utf8_space_elim(char const *str, size_t *len)
{
    struct utf8_iterator itr;
    char *ret;

    for (utf8_iter_first(&itr, (char*) str);
	 !utf8_iter_end_p(&itr) && utf8_iter_isspace(&itr);
	 utf8_iter_next(&itr))
	/* skip leading whitespace */;

    ret = itr.curptr;

    if (len) {
	size_t n = 0;
	int wslen = 0;

	wslen = 0;
	for (;
	     !utf8_iter_end_p(&itr);
	     utf8_iter_next(&itr)) {
	    if (utf8_iter_isspace(&itr)) {
		wslen += itr.curwidth;
	    } else {
		if (wslen) {
		    n += wslen;
		    wslen = 0;
		}
		n += itr.curwidth;
	    }
	}

	*len = n;
    }

    return ret;
}

char *
utf8_space_trim(char *str)
{
    size_t n;
    str = utf8_space_elim(str, &n);
    str[n] = 0;
    return str;
}
