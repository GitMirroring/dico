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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dico.h>
#include <dico/conv.h>

/*
 *  Given input string in Greek transliteration, convert it to
 *  an equivalent Greek word in UTF-8 encoding. The input string is
 *  supposed to follow the traditional Greek keyboard layout:
 *
 *      +----------------------------------------------------------------+
 *      | 1! | 2@ | 3# | 4$ | 5% | 6^ | 7& | 8* | 9( | 0) | -_ | =+ | `~ |
 *      +----------------------------------------------------------------+
 *        | ·― | ςΣ | εΕ | ρΡ | τΤ | υΥ | θΘ | ιΙ | οΟ | πΠ | [{ | ]} |
 *        +------------------------------------------------------------+
 *         | αΑ | σΣ | δΔ | φΦ | γΓ | ηΗ | ξΞ | κΚ | λΛ | ΄¨ | '" | \| |
 *         +-----------------------------------------------------------+
 *           | ζΖ | χΧ | ψΨ | ωΩ | βΒ | νΝ | μΜ | ,; | .: | /? |
 *           +-------------------------------------------------+
 *		    +-----------------------------+
 *		    |          space bar          |
 *		    +-----------------------------+
 */

enum {
    V_NORMAL,
    V_ACUTE,
    V_DIAER,
    V_DIAER_ACUTE,
    V_MAX
};

typedef char *KBD_MAP[V_MAX];

static KBD_MAP greek_kbd_map[] = {
    /* A */ { "Α", "Ά" },
    /* B */ { "Β" },
    /* C */ { "Ψ" },
    /* D */ { "Δ" },
    /* E */ { "Ε", "Έ" },
    /* F */ { "Φ" },
    /* G */ { "Γ" },
    /* H */ { "Η", "Ή" },
    /* I */ { "Ι", "Ί", "Ϊ", "Ϊ" },
    /* J */ { "Ξ" },
    /* K */ { "Κ" },
    /* L */ { "Λ" },
    /* M */ { "Μ" },
    /* M */ { "Ν" },
    /* O */ { "Ο", "Ό" },
    /* P */ { "Π" },
    /* Q */ { "―" },
    /* R */ { "Ρ" },
    /* S */ { "Σ" },
    /* T */ { "Τ" },
    /* U */ { "Θ" },
    /* V */ { "Ω", "Ώ" },
    /* W */ { "Σ" },
    /* X */ { "Χ" },
    /* Y */ { "Υ", "Ύ", "Ϋ", "Ϋ" },
    /* Z */ { "Ζ" },
    /* a */ { "α", "ά" },
    /* b */ { "β" },
    /* c */ { "ψ" },
    /* d */ { "δ" },
    /* e */ { "ε", "έ" },
    /* f */ { "φ" },
    /* g */ { "γ" },
    /* h */ { "η", "ή" },
    /* i */ { "ι", "ί", "ϊ", "ΐ" },
    /* j */ { "ξ" },
    /* k */ { "κ" },
    /* l */ { "λ" },
    /* m */ { "μ" },
    /* n */ { "ν" },
    /* o */ { "ο", "ό" },
    /* p */ { "π" },
    /* q */ { "·" },
    /* r */ { "ρ" },
    /* s */ { "σ" },
    /* t */ { "τ" },
    /* u */ { "θ" },
    /* v */ { "ω", "ώ" },
    /* w */ { "ς" },
    /* x */ { "χ" },
    /* y */ { "υ", "ύ", "ϋ", "ΰ" },
    /* z */ { "ζ" },
};

#define ISUPPER(c) ('A' <= (c) && (c) <= 'Z')
#define ISLOWER(c) ('a' <= (c) && (c) <= 'z')
#define ISALPHA(c) (ISUPPER(c) || ISLOWER(c))

typedef enum {
    CASE_KEEP,
    CASE_LOWER,
    CASE_UPPER
} CASEMAP;

struct greek_kbd_conf {
    CASEMAP casemap;
};

static int
kbd_i_conv(char const *input, char **output, CASEMAP casemap)
{
    char *outbuf;
    size_t outlen;
    size_t outcap;
    int c;

    outcap = strlen(input);
    outbuf = malloc(outcap);
    if (!outbuf)
	return -1;
    outlen = 0;

    while ((c = *input++) != 0) {
	char const *repl;
	size_t replen;

	switch (casemap) {
	case CASE_KEEP:
	    break;

	case CASE_LOWER:
	    if (ISUPPER(c))
		c += 'a' - 'A';
	    break;

	case CASE_UPPER:
	    if (ISLOWER(c))
		c -= 'a' - 'A';
	    break;
	}

	if (c == 's' && *input == 0) {
	    repl = "ς";
	    replen = strlen(repl);
	} else if (ISALPHA(c)) {
	    char **map;
	    int v = V_NORMAL;

	    if (ISLOWER(c))
		c -= 'a' - ('Z' - 'A' + 1);
	    else
		c -= 'A';

	    map = greek_kbd_map[c];

	    if (map[V_DIAER] && *input == ':') {
		v = V_DIAER;
		input++;
	    }
	    if (map[v+1] && *input == '\'') {
		v++;
		input++;
	    }
	    repl = map[v];
	    replen = strlen(repl);
	} else {
	    repl = input-1;
	    replen = 1;
	}

	if (outlen + replen > outcap) {
	    char *p;

	    while (outlen + replen > outcap) {
		if ((size_t) -1 / 3 * 2 <= outcap) {
		    free(outbuf);
		    errno = ENOMEM;
		    return -1;
		}
		outcap += (outcap + 1) / 2;
	    }
	    if ((p = realloc(outbuf, outcap)) == NULL) {
		int ec = errno;
		free(outbuf);
		errno = ec;
		return -1;
	    }
	    outbuf = p;
	}

	memcpy(outbuf + outlen, repl, replen);
	outlen += replen;
    }

    if (outlen + 1 > outcap) {
	char *p = realloc(outbuf, outlen + 1);
	if (!p) {
	    int ec = errno;
	    free(outbuf);
	    errno = ec;
	    return -1;
	}
	outbuf = p;
    }

    outbuf[outlen] = 0;
    *output = outbuf;

    return 0;
}

static int
kbd_conv(void *data, char const *input, char **output)
{
    struct greek_kbd_conf *conf = data;

    if (('a' <= input[0] && input[0] <= 'z') ||
	('A' <= input[0] && input[0] <= 'Z'))
	return kbd_i_conv(input, output, conf ? conf->casemap : CASE_KEEP);
    else
	*output = NULL;
    return 0;
}

static char const *casemap_choice[] = {
    "keep",
    "lower",
    "upper",
    NULL
};

#define CONV_NAME "greek_kbd"

static int
greek_kbd_init(int argc, char **argv)
{
    struct greek_kbd_conf *conf = NULL;
    int casemap;
    char *conv_name = CONV_NAME;
    struct dico_option init_option[] = {
	{ DICO_OPTSTR(casemap), dico_opt_enum, &casemap, { .enumstr = casemap_choice } },
	{ NULL }
    };

    while (argc) {
	int idx;

	conv_name = *argv;
	argc--;
	argv++;

	casemap = CASE_KEEP;
	conf = NULL;
	if (dico_parseopt(init_option, argc, argv, DICO_PARSEOPT_PARSE_ARGV0,
			  &idx))
	    return 1;

	if (casemap != CASE_KEEP) {
	    conf = malloc(sizeof(*conf));
	    if (!conf) {
		DICO_LOG_MEMERR();
		return 1;
	    }
	    conf->casemap = casemap;
	}

	if (dico_conv_register(conv_name, kbd_conv, conf)) {
	    if (errno == EEXIST)
		dico_log(L_ERR, 0, "can't register converter %s: already registered",
			 conv_name);
	    else
		dico_log(L_ERR, errno, "can't register converter %s",
			 conv_name);
	}

	argc -= idx;
	argv += idx;
    }
    return 0;
}

static int
greek_kbd_run_test(int argc, char **argv)
{
    int i;
    dico_conv_t conv = dico_conv_find(argv[0]);
    if (!conv)
	abort();
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == ':') {
	    conv = dico_conv_find(argv[i]+1);
	    if (!conv) {
		dico_log(L_ERR, 0, "%s: no such converter", argv[i]+1);
		return 1;
	    }
	} else {
	    char *output;

	    if (conv->conv(conv->data, argv[i], &output) == 0) {
		printf("%s: %s\n", argv[i], output);
		free(output);
	    } else {
		printf("%s:\n", argv[i]);
	    }
	}
    }
    return 0;
}

struct dico_database_module DICO_EXPORT(greek_kbd, module) = {
    .dico_version        =  DICO_MODULE_VERSION,
    .dico_capabilities   =  DICO_CAPA_NODB,
    .dico_init           =  greek_kbd_init,
    .dico_run_test       =  greek_kbd_run_test,
};
