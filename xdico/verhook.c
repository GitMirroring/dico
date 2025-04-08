#include <config.h>
#include <stdio.h>
#include <libi18n.h>
#include <dico/diag.h>
#include <parseopt.h>

static int copyright_year = 2025;
static char gplv3[] = N_("\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n");

void
format_version(void (*printer)(void *, char const *, ...), void *data,
	       char const *progname)
{
    printer(data, "%s (%s) %s\n",
	    progname,
	    PACKAGE_NAME,
	    PACKAGE_VERSION);
    printer(data, "Copyright %s 2005-%d Sergey Poznyakoff\n",
	    gettext("(C)"),
	    copyright_year);
    printer(data, gettext(gplv3));
}

static void
printer_wordwrap(void *data, char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    wordwrap_vprintf(data, fmt, ap);
    va_end(ap);
}

void
xdico_version_hook(WORDWRAP_FILE wf, struct parseopt *po)
{
    format_version(printer_wordwrap, wf, po->po_program_name);
}

static void
printer_file(void *data, char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(data, fmt, ap);
    va_end(ap);
}

void
xdico_print_version(FILE *fp)
{
    format_version(printer_file, fp, dico_program_name);
}
