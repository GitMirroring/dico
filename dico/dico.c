/* This file is part of GNU Dico.
   Copyright (C) 1998-2025 Sergey Poznyakoff

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

#include "dico-priv.h"
#include <parseopt.h>

struct dico_url dico_url;
struct auth_cred default_cred;
char *client = DICO_CLIENT_ID;
enum dico_client_mode mode = mode_define;
int transcript;
char *source_addr;
int noauth_option;
unsigned levenshtein_threshold;
char *autologin_file;
int quiet_option;

int debug_level;
int debug_source_info;
dico_stream_t debug_stream;

enum {
    OPT_HOST = 256,
    OPT_SOURCE,
    OPT_LEVDIST,
    OPT_SASL,
    OPT_NOSASL,
    OPT_AUTOLOGIN,
    OPT_TIME_STAMP,
    OPT_SOURCE_INFO,
};

static struct optdef options[] = {
    {
	.opt_flags = OPTFLAG_DOC,
	.opt_doc = N_("Server selection")
    },

    {
	.opt_name = "host",
	.opt_argdoc = N_("SERVER"),
	.opt_doc = N_("connect to this server"),
	.opt_code = OPT_HOST
    },

    {
	.opt_name = "port",
	.opt_argdoc = N_("SERVICE"),
	.opt_doc = N_("specify port to connect to")
    },
    {
	.opt_name = "p",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "database",
	.opt_argdoc = N_("NAME"),
	.opt_doc = N_("select a database to search")
    },
    {
	.opt_name = "d",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "source",
	.opt_argdoc = N_("ADDR"),
	.opt_doc = N_("set a source address for TCP connections"),
	.opt_code = OPT_SOURCE
    },

    {
	.opt_flags = OPTFLAG_DOC,
	.opt_doc = N_("Operation modes")
    },

    {
	.opt_name = "match",
	.opt_doc = N_("match instead of define")
    },
    {
	.opt_name = "m",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "strategy",
	.opt_argdoc = N_("NAME"),
	.opt_doc =
	N_("select a strategy for matching; implies --match")
    },
    {
	.opt_name = "s",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "levdist",
	.opt_argdoc = N_("N"),
	.opt_doc = N_("set maximum Levenshtein distance"),
	.opt_code = OPT_LEVDIST
    },
    {
	.opt_name = "levenshtein-distance",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "dbs",
	.opt_doc = N_("show available databases")
    },
    {
	.opt_name = "D",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "strategies",
	.opt_doc = N_("show available search strategies")
    },
    {
	.opt_name = "S",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "serverhelp",
	.opt_doc = N_("show server help")
    },
    {
	.opt_name = "H",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "info",
	.opt_argdoc = N_("DBNAME"),
	.opt_doc = N_("show information about database DBNAME")
    },
    {
	.opt_name = "i",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "serverinfo",
	.opt_doc = N_("show information about the server")
    },
    {
	.opt_name = "I",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "quiet",
	.opt_doc = N_("do not print the normal dico welcome")
    },
    {
	.opt_name = "q",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_flags = OPTFLAG_DOC,
	.opt_doc = N_("Authentication")
    },

    {
	.opt_name = "noauth",
	.opt_doc = N_("disable authentication")
    },
    {
	.opt_name = "a",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "sasl",
	.opt_doc = N_("enable SASL authentication (default)"),
	.opt_code = OPT_SASL
    },

    {
	.opt_name = "nosasl",
	.opt_doc = N_("disable SASL authentication"),
	.opt_code = OPT_NOSASL
    },

    {
	.opt_name = "user",
	.opt_argdoc = N_("NAME"),
	.opt_doc = N_("set user name for authentication")
    },
    {
	.opt_name = "u",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "key",
	.opt_argdoc = N_("STRING"),
	.opt_doc = N_("set shared secret for authentication")
    },
    {
	.opt_name = "k",
	.opt_flags = OPTFLAG_ALIAS
    },
    {
	.opt_name = "password",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "autologin",
	.opt_argdoc = N_("NAME"),
	.opt_doc = N_("set the name of autologin file to use"),
	.opt_code = OPT_AUTOLOGIN
    },

    {
	.opt_name = "client",
	.opt_argdoc = N_("STRING"),
	.opt_doc = N_("additional text for client command")
    },
    {
	.opt_name = "c",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_flags = OPTFLAG_DOC,
	.opt_doc = N_("Debugging")
    },

    {
	.opt_name = "transcript",
	.opt_doc = N_("enable session transcript")
    },
    {
	.opt_name = "t",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "verbose",
	.opt_doc = N_("increase debugging verbosity level")
    },
    {
	.opt_name = "v",
	.opt_flags = OPTFLAG_ALIAS
    },

    {
	.opt_name = "time-stamp",
	.opt_doc = N_("include time stamp in the debugging output"),
	.opt_code = OPT_TIME_STAMP
    },

    {
	.opt_name = "source-info",
	.opt_doc =
	N_("include source line information in the debugging output"),
	.opt_code = OPT_SOURCE_INFO
    },

    { NULL }
}, *optdef[] = { options, NULL };

static void
setopt(struct parseopt *po, int code, char *optarg)
{
    char *p;
    int n = 1;

    switch (code) {
    case OPT_HOST:
	xdico_assign_string(&dico_url.host, optarg);
	break;

    case 'p':
	xdico_assign_string(&dico_url.port, optarg);
	break;

    case 'd':
	xdico_assign_string(&dico_url.req.database, optarg);
	break;

    case OPT_SOURCE:
	source_addr = optarg;
	break;

    case 'm':
	mode = mode_match;
	break;

    case 's':
	xdico_assign_string(&dico_url.req.strategy, optarg);
	mode = mode_match;
	break;

    case OPT_LEVDIST:
	levenshtein_threshold = strtoul(optarg, &p, 10);
	if (*p)
	    dico_die(1, L_ERR, 0, _("%s: invalid number"), optarg);
	break;

    case 'D':
	mode = mode_dbs;
	break;

    case 'S':
	mode = mode_strats;
	break;

    case 'H':
	mode = mode_help;
	break;

    case 'i':
	mode = mode_info;
	dico_url.req.database = optarg;
	break;

    case 'I':
	mode = mode_server;
	break;

    case 'q':
	quiet_option = 1;
	break;

    case 'a':
	noauth_option = 1;
	break;

    case OPT_SASL:
	sasl_enable(1);
	break;

    case OPT_NOSASL:
	sasl_enable(0);
	break;

    case 'u':
	default_cred.user = optarg;
	break;

    case 'k':
	default_cred.pass = optarg;
	break;

    case OPT_AUTOLOGIN:
	autologin_file = optarg;
	break;

    case 'c':
	client = optarg;
	break;

    case 't':
	transcript = 1;
	break;

    case 'v':
	debug_level++;
	break;

    case OPT_TIME_STAMP:
	dico_stream_ioctl(debug_stream, DICO_DBG_CTL_SET_TS, &n);
	break;

    case OPT_SOURCE_INFO:
	debug_source_info = 1;
	break;

    default:
	po->po_error(po, PO_MSG_ERR,
		     "internal error at %s:%d\n", __FILE__, __LINE__);
	abort();
    }
}

extern void xdico_version_hook(WORDWRAP_FILE wf, struct parseopt *po);

static void
doc_hook(WORDWRAP_FILE wf, struct parseopt *po)
{
    wordwrap_printf(wf, "%s\n", _("URL syntax"));
    wordwrap_para(wf);
    wordwrap_printf(wf, "%s:\n", _("Define word"));
    wordwrap_set_left_margin(wf, parseopt_help_format.short_opt_col);
    wordwrap_printf(wf, "dict://[%s;%s@]%s[:%s]/d:%s[:%s:%s]\n",
		    _("USER"), _("PASS"), _("HOST"), _("PORT"),
		    _("WORD"), _("DATABASE"), _("N"));
    wordwrap_set_left_margin(wf, 0);
    wordwrap_printf(wf, "%s:\n", _("Match word"));
    wordwrap_set_left_margin(wf, parseopt_help_format.short_opt_col);
    wordwrap_printf(wf, "dict://[%s;%s@]%s[:%s]/m:%s[:%s:%s:%s]\n",
		    _("USER"), _("PASS"), _("HOST"), _("PORT"),
		    _("WORD"), _("DATABASE"), _("STRAT"), _("N"));
}

static struct parseopt po = {
    .po_descr = N_("GNU dictionary client program"),
    .po_argdoc = N_("[URL-or-WORD]"),
    .po_optdef = optdef,
    .po_setopt = setopt,
    .po_package_name = PACKAGE_NAME,
    .po_package_url = PACKAGE_URL,
    .po_bugreport_address = PACKAGE_BUGREPORT,
    .po_prog_doc_hook = doc_hook,
    .po_version_hook = xdico_version_hook
};

static char gplv3_text[] = "\
   GNU Dico is free software; you can redistribute it and/or modify\n\
   it under the terms of the GNU General Public License as published by\n\
   the Free Software Foundation; either version 3, or (at your option)\n\
   any later version.\n\
\n\
   GNU Dico is distributed in the hope that it will be useful,\n\
   but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
   GNU General Public License for more details.\n\
\n\
   You should have received a copy of the GNU General Public License\n\
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>.\n";

void
ds_warranty(int argc, char **argv)
{
    printf("%s (%s) %s\n", dico_program_name, PACKAGE_NAME, PACKAGE_VERSION);
    putchar('\n');
    printf("%s", gplv3_text);
}

extern void xdico_print_version(FILE *fp);

void
shell_banner(void)
{
    xdico_print_version(stdout);
    putchar('\n');
    printf("%s\n\n", _("Type ? for help summary"));
}

void
fixup_url(void)
{
    xdico_assign_string(&dico_url.proto, "dict");
    if (!dico_url.host)
	xdico_assign_string(&dico_url.host, DEFAULT_DICT_SERVER);
    if (!dico_url.port)
	xdico_assign_string(&dico_url.port, DICO_DICT_PORT_STR);
    if (!dico_url.req.database)
	xdico_assign_string(&dico_url.req.database, "!");
    if (!dico_url.req.strategy)
	xdico_assign_string(&dico_url.req.strategy, ".");
    if (mode == mode_match)
	dico_url.req.type = DICO_REQUEST_MATCH;
}

int
main(int argc, char **argv)
{
    int rc = 0;

    appi18n_init();
    dico_set_program_name(argv[0]);
    debug_stream = dico_dbg_stream_create();
    parse_init_scripts();

    if (parseopt_getopt(&po, argc, argv) == OPT_ERR) {
	dico_log(L_CRIT, errno, _("parseopt_getopt failed"));
	exit(1);
    }
    parseopt_argv(&po, &argc, &argv);

    fixup_url();
    set_quoting_style(NULL, escape_quoting_style);
    signal(SIGPIPE, SIG_IGN);

    switch (mode) {
    case mode_define:
    case mode_match:
	if (!argc) {
	    dico_shell();
	    exit(0);
	}
	break;

    default:
	if (argc)
	    dico_log(L_WARN, 0,
		     _("extra command line arguments ignored"));
    }

    switch (mode) {
    case mode_define:
    case mode_match:
	if (!argc)
	    dico_die(1, L_ERR, 0,
		     _("you should give a word to look for or an URL"));
	while (argc--)
	    rc |= dict_word((*argv)++) != 0;
	break;

    case mode_dbs:
	rc = dict_single_command("SHOW DATABASES", NULL, "110");
	break;

    case mode_strats:
	rc = dict_single_command("SHOW STRATEGIES", NULL, "111");
	break;

    case mode_help:
	rc = dict_single_command("HELP", NULL, "113");
	break;

    case mode_info:
	if (!dico_url.req.database)
	    dico_die(1, L_ERR, 0, _("Database name not specified"));
	rc = dict_single_command("SHOW INFO", dico_url.req.database, "112");
	break;

    case mode_server:
	rc = dict_single_command("SHOW SERVER", NULL, "114");
	break;
    }

    return rc;
}
