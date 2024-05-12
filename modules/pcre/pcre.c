/* This file is part of GNU Dico.
   Copyright (C) 2008-2024 Sergey Poznyakoff

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

#include <config.h>
#include <dico.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <appi18n.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

struct dico_pcre_flag
{
    int c;
    int flag;
};

static struct dico_pcre_flag flagtab[] = {
    { 'a', PCRE2_ANCHORED }, /* Force pattern anchoring */
    { 'e', PCRE2_EXTENDED }, /* Ignore whitespace and # comments */
    { 'i', PCRE2_CASELESS }, /* Do caseless matching */
    { 'G', PCRE2_UNGREEDY }, /* Invert greediness of quantifiers */
    { 0 },
};

static int
pcre_flag(int c, uint32_t *pflags)
{
    struct dico_pcre_flag *p;

    for (p = flagtab; p->c; p++) {
	if (p->c == c) {
	    *pflags |= p->flag;
	    return 0;
	} else if (p->c == tolower(c) || p->c == toupper(c)) {
	    *pflags &= ~p->flag;
	    return 0;
	}
    }
    return 1;
}

static pcre2_code *
compile_pattern(const char *pattern)
{
    uint32_t cflags = PCRE2_UTF|PCRE2_NEWLINE_ANY;
    int error;
    PCRE2_SIZE length;
    PCRE2_SIZE error_offset;
    pcre2_code *pre;

    /*
     * Pcre2 documentation claims that pattern length is measured in code
     * points. This, however, doesn't seem to be the case. At least, with
     * version 10.35, length is measured in characters.
     */
    length = strlen(pattern);
    if (pattern[0] == '/') {
	char *p;

	pattern++;
	length--;
	p = strrchr(pattern, '/');
	if (!p) {
	    dico_log(L_ERR, 0, _("PCRE missing terminating /: %s"),
		     pattern - 1);
	    return NULL;
	}
	length -= strlen(p);

	while (*++p) {
	    if (pcre_flag(*p, &cflags)) {
		dico_log(L_ERR, 0, _("PCRE error: invalid flag %c"), *p);
		return NULL;
	    }
	}
    }
    pre = pcre2_compile((PCRE2_SPTR8)pattern, length, cflags, &error, &error_offset, NULL);
    if (!pre) {
	char errbuf[120];

	switch (pcre2_get_error_message(error, (PCRE2_UCHAR8*)errbuf, sizeof errbuf)) {
	case PCRE2_ERROR_NOMEMORY:
	default:
	    break;
	case PCRE2_ERROR_BADDATA:
	    strncpy(errbuf, "bad error code", sizeof(errbuf)-1);
	    break;
	}
	dico_log(L_ERR, 0,
		 _("pcre_compile(\"%s\") failed at offset %lu: %s"),
		 pattern, error_offset, errbuf);
    }
    return pre;
}

struct pcre_call_data
{
    pcre2_code *code;
    pcre2_match_data *md;
};

static int
pcre_sel(int cmd, dico_key_t key, const char *dict_word)
{
    int rc = 0;
    char const *word = key->word;
    struct pcre_call_data *cdata = key->call_data;

    switch (cmd) {
    case DICO_SELECT_BEGIN:
	if ((cdata = calloc(1, sizeof(cdata[0]))) == NULL) {
	    DICO_LOG_MEMERR();
	    return 1;
	}
	if ((cdata->code = compile_pattern(word)) == NULL) {
	    free(cdata);
	    return 1;
	}
	cdata->md = pcre2_match_data_create_from_pattern(cdata->code, NULL);
	if (cdata->md == NULL) {
	    pcre2_code_free(cdata->code);
	    free(cdata);
	    return 1;
	}
	key->call_data = cdata;
	break;

    case DICO_SELECT_RUN:
	rc = pcre2_match(cdata->code,
			 (PCRE2_SPTR8)dict_word, PCRE2_ZERO_TERMINATED,
			 0, 0, cdata->md, NULL) >= 0;
	break;

    case DICO_SELECT_END:
	pcre2_match_data_free(cdata->md);
	pcre2_code_free(cdata->code);
	free(cdata);
	break;
    }
    return rc;
}

static struct dico_strategy pcre_strat = {
    "pcre",
    "Match using Perl-compatible regular expressions",
    pcre_sel
};

static int
pcre_init(int argc, char **argv)
{
    dico_strategy_add(&pcre_strat);
    return 0;
}

struct dico_database_module DICO_EXPORT(pcre, module) = {
    DICO_MODULE_VERSION,
    DICO_CAPA_NODB,
    pcre_init,
};
