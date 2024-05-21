/* This file is part of GNU Dico.
   Copyright (C) 2012-2024 Sergey Poznyakoff

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
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dico.h>
#include "gcide.h"
#include <errno.h>
#include <appi18n.h>

#define GCIDE_NOPR     0x01
#define GCIDE_DBGLEX   0x02
#define GCIDE_WATCHER  0x04
#define GCIDE_IDX_FAIL 0x08
#define GCIDE_HTML     0x10

struct gcide_db {
    char *db_dir;
    char *idx_dir;
    char *tmpl_name;
    char *tmpl_letter;
    char *idxgcide;
    int flags;
    time_t latest_change;

    int file_letter;
    dico_stream_t file_stream;

    size_t idx_cache_size;
    gcide_idx_file_t idx;

    WATCHER watcher;
};

enum result_type {
    result_match,
    result_define
};

struct gcide_result {
    enum result_type type;
    struct gcide_db *db;
    size_t compare_count;
    dico_iterator_t itr;
    dico_list_t list;
};

static char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static char *idxgcide_program = LIBEXECDIR "/idxgcide";

static void
free_db(struct gcide_db *db)
{
    free(db->db_dir);
    free(db->idx_dir);
    free(db->tmpl_name);
    free(db->idxgcide);
    if (db->file_stream) {
	dico_stream_close(db->file_stream);
	dico_stream_destroy(&db->file_stream);
    }
    gcide_idx_file_close(db->idx);
    watcher_close(db->watcher);
    free(db);
}

static int
gcide_check_dir(const char *dir)
{
    struct stat st;

    if (stat(dir, &st)) {
	dico_log(L_ERR, errno, _("gcide: cannot stat `%s'"), dir);
	return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
	dico_log(L_ERR, 0, _("gcide: `%s' is not a directory"), dir);
	return 1;
    }
    if (access(dir, R_OK)) {
	dico_log(L_ERR, 0, _("gcide: `%s' is not readable"), dir);
	return 1;
    }
    return 0;
}

static char *
gcide_template_name(struct gcide_db *db, int let)
{
    *db->tmpl_letter = let;
    return db->tmpl_name;
}

static int
run_idxgcide(char *idxname, struct gcide_db *db)
{
    pid_t pid;
    int status;
    char *idxgcide = db->idxgcide ? db->idxgcide : idxgcide_program;

    dico_log(L_NOTICE, 0, _("gcide_open_idx: creating index %s"),
	     idxname);
    if (access(idxgcide, X_OK)) {
	dico_log(L_ERR, errno, _("gcide_open_idx: cannot run %s"),
		 idxgcide);
	return 1;
    }
    pid = fork();
    if (pid == 0) {
	execl(idxgcide, idxgcide, db->db_dir, db->idx_dir, NULL);
    }
    if (pid == -1) {
	dico_log(L_ERR, errno, _("gcide_open_idx: fork failed"));
	return 1;
    }
    if (waitpid(pid, &status, 0) != pid) {
	dico_log(L_ERR, errno, _("gcide_open_idx: %s failed"), idxgcide);
	kill(pid, SIGKILL);
	return 1;
    }
    if (!WIFEXITED(status)) {
	dico_log(L_ERR, 0, _("gcide_open_idx: %s failed"), idxgcide);
	return 1;
    }

    status = WEXITSTATUS(status);
    if (status) {
	dico_log(L_ERR, 0,
		 _("gcide_open_idx: %s exited with status %d"),
		 idxgcide, status);
	return 1;
    }
    return 0;
}

static int
gcide_check_files(struct gcide_db *db)
{
    int i;
    time_t t = 0;
    struct stat st;

    for (i = 0; letters[i]; i++) {
	char *p = gcide_template_name(db, letters[i]);
	if (access(p, R_OK)) {
	    dico_log(L_ERR, 0, _("gcide: `%s' is not readable"), p);
	    return 1;
	}
	if (stat(p, &st)) {
	    dico_log(L_ERR, errno, _("gcide: can't stat `%s'"), p);
	    return 1;
	}
	if (st.st_mtime > t)
	    t = st.st_mtime;
    }
    db->latest_change = t;
    return 0;
}

/* Try to access IDXNAME.  Return 0 on success, 1 if it should be (re)created
   and -1 on error */
static int
gcide_access_idx(struct gcide_db *db, char *idxname)
{
    int rc = 1;
    struct stat st;

    if (access(idxname, R_OK) == 0) {
	if (stat(idxname, &st)) {
	    dico_log(L_ERR, errno, _("gcide: can't stat `%s'"), idxname);
	    /* try to create it, anyway */
	} else if (db->latest_change <= st.st_mtime)
	    rc = 0;
	else
	    dico_log(L_NOTICE, 0,
		     _("gcide: index file older than database, reindexing"));
    } else if (errno != ENOENT) {
	dico_log(L_ERR, errno, _("gcide_open_idx: cannot access %s"),
		 idxname);
	rc = -1;
    }
    return rc;
}

static int
gcide_open_idx(struct gcide_db *db)
{
    int rc = 1;
    char *idxname;

    idxname = dico_full_file_name(db->idx_dir, "GCIDE.IDX");
    if (!idxname) {
	DICO_LOG_MEMERR();
	return 1;
    }

    rc = gcide_access_idx(db, idxname);
    if (rc == 1)
	rc = run_idxgcide(idxname, db);

    if (rc == 0) {
	if (db->idx) {
	    gcide_idx_file_close(db->idx);
	    db->idx = NULL;
	}
	db->idx = gcide_idx_file_open(idxname, db->idx_cache_size);
	if (!db->idx)
	    rc = 1;
    }

    free(idxname);
    return rc;
}

static dico_handle_t
gcide_init_db(const char *dbname, int argc, char **argv)
{
    char *db_dir = NULL;
    char *idx_dir = NULL;
    char *idxgcide = NULL;
    long idx_cache_size = 16;
    int flags = 0;
    struct gcide_db *db;

    struct dico_option init_db_option[] = {
	{ DICO_OPTSTR(dbdir), dico_opt_string, &db_dir },
	{ DICO_OPTSTR(idxdir), dico_opt_string, &idx_dir },
	{ DICO_OPTSTR(index-program), dico_opt_string, &idxgcide },
	{ DICO_OPTSTR(index-cache-size), dico_opt_long, &idx_cache_size },
	{ DICO_OPTSTR(suppress-pr), dico_opt_bitmask, &flags,
	  .v.value = GCIDE_NOPR },
	{ DICO_OPTSTR(debug-lex), dico_opt_bitmask, &flags,
	  .v.value = GCIDE_DBGLEX },
	{ DICO_OPTSTR(watch), dico_opt_bitmask, &flags,
	  .v.value = GCIDE_WATCHER },
	{ DICO_OPTSTR(html), dico_opt_bitmask, &flags,
	  .v.value = GCIDE_HTML },
	{ NULL }
    };

    if (dico_parseopt(init_db_option, argc, argv, 0, NULL))
	return NULL;
    if (!db_dir) {
	dico_log(L_ERR, 0,
		 _("gcide_init_db: database directory not given"));
	return NULL;
    }
    if (!idx_dir) {
	idx_dir = strdup(db_dir);
	if (!idx_dir) {
	    DICO_LOG_ERRNO();
	    free(db_dir);
	    return NULL;
	}
    }

    db = calloc(1, sizeof(*db));
    if (!db) {
	DICO_LOG_MEMERR();
	free(db_dir);
	free(idx_dir);
	return NULL;
    }
    db->db_dir = db_dir;
    db->idx_dir = idx_dir;
    db->idx_cache_size = idx_cache_size;
    db->flags = flags;

    if (gcide_check_dir(db->db_dir) || gcide_check_dir(db->idx_dir)) {
	free_db(db);
	return NULL;
    }

    db->tmpl_name = dico_full_file_name(db->db_dir, "CIDE.A");
    db->tmpl_letter = db->tmpl_name + strlen(db->tmpl_name) - 1;
    if (gcide_check_files(db)) {
	free_db(db);
	return NULL;
    }
    db->idxgcide = idxgcide;
    if (gcide_open_idx(db)) {
	free_db(db);
	return NULL;
    }
    if (flags & GCIDE_WATCHER) {
	db->watcher = watcher_setup(db->db_dir);
    }
    return (dico_handle_t)db;
}

static int
gcide_free_db(dico_handle_t hp)
{
    struct gcide_db *db = (struct gcide_db *) hp;
    free_db(db);
    return 0;
}

static int
reload_if_changed(struct gcide_db *db)
{
    if ((db->flags & GCIDE_WATCHER) && watcher_is_modified(db->watcher)) {
	gcide_check_files(db);
	if (gcide_open_idx(db)) {
	    db->flags |= GCIDE_IDX_FAIL;
	} else {
	    db->flags &= ~GCIDE_IDX_FAIL;
	}
    }
    if (db->flags & GCIDE_IDX_FAIL)
	return -1;
    return 0;
}

static int
_is_nl_or_punct(int c)
{
    return !!strchr("\r\n!,-./:;?", c);
}

static char *
read_info_file(const char *fname, int first_line)
{
    dico_stream_t stream;
    int rc;
    char *bufptr = NULL;
    size_t bufsize = 0;

    stream = dico_mapfile_stream_create(fname, DICO_STREAM_READ);
    if (!stream) {
	dico_log(L_NOTICE, errno, _("cannot create stream `%s'"), fname);
	return NULL;
    }

    rc = dico_stream_open(stream);
    if (rc) {
	dico_log(L_ERR, 0,
		 _("cannot open stream `%s': %s"),
		 fname, dico_stream_strerror(stream, rc));
	dico_stream_destroy(&stream);
	return NULL;
    }

    if (first_line) {
	size_t n;

	rc = dico_stream_getline(stream, &bufptr, &bufsize, &n);
	if (rc) {
	    dico_log(L_ERR, 0,
		     _("read error in stream `%s': %s"),
		     fname, dico_stream_strerror(stream, rc));
	} else
	    dico_string_trim(bufptr, n, _is_nl_or_punct);
    } else {
	off_t size;
	rc = dico_stream_size(stream, &size);
	if (rc) {
	    dico_log(L_ERR, 0,
		     _("cannot get size of stream `%s': %s"),
		     fname, dico_stream_strerror(stream, rc));
	} else {
	    bufsize = size;
	    bufptr = malloc (bufsize + 1);
	    if (!bufptr) {
		dico_log(L_ERR, errno,
			 _("cannot allocate dictionary information buffer"));
	    } else if ((rc = dico_stream_read(stream, bufptr, bufsize, NULL))) {
		dico_log(L_ERR, 0,
			 _("read error in stream `%s': %s"),
			 fname, dico_stream_strerror(stream, rc));
		free(bufptr);
		bufptr = NULL;
	    } else
		bufptr[bufsize] = 0;
	}
    }

    dico_stream_destroy(&stream);
    return bufptr;
}

static char *
read_dictionary_info(struct gcide_db *db, int first_line)
{
    char *fname = dico_full_file_name(db->db_dir, "INFO");
    char *info = read_info_file(fname, first_line);
    free(fname);
    return info;
}

char *
gcide_info(dico_handle_t hp)
{
    return read_dictionary_info((struct gcide_db *) hp, 0);
}

char *
gcide_descr(dico_handle_t hp)
{
    return read_dictionary_info((struct gcide_db *) hp, 1);
}

static gcide_iterator_t
exact_match(struct gcide_db *db, const char *hw)
{
    return gcide_idx_locate(db->idx, (char*)hw, 0);
}

static gcide_iterator_t
prefix_match(struct gcide_db *db, const char *hw)
{
    return gcide_idx_locate(db->idx, (char*)hw, utf8_strlen(hw));
}

typedef gcide_iterator_t (*matcher_t)(struct gcide_db *, const char *);

struct strategy_def {
    struct dico_strategy strat;
    matcher_t matcher;
};

static struct strategy_def strat_tab[] = {
    { { "exact", "Match words exactly" }, exact_match },
    { { "prefix", "Match word prefixes" }, prefix_match },
};

static int
gcide_init(int argc, char **argv)
{
    int i;

    for (i = 0; i < DICO_ARRAY_SIZE(strat_tab); i++)
	dico_strategy_add(&strat_tab[i].strat);

    return 0;
}

static matcher_t
find_matcher(const char *strat)
{
    int i;
    for (i = 0; i < DICO_ARRAY_SIZE(strat_tab); i++)
	if (strcmp(strat, strat_tab[i].strat.name) == 0)
	    return strat_tab[i].matcher;
    return NULL;
}

static int
compare_ref(const void *a, const void *b, void *closure)
{
    struct gcide_ref const *aref = a;
    struct gcide_ref const *bref = b;

    return utf8_strcasecmp(aref->ref_headword, bref->ref_headword);
}

static int
free_ref(void *a, void *b)
{
    struct gcide_ref *ref = a;
    free(ref->ref_headword);
    free(ref);
    return 0;
}

static dico_list_t
gcide_create_result_list(int unique)
{
    dico_list_t list;

    list = dico_list_create();
    if (!list) {
	DICO_LOG_ERRNO();
	return NULL;
    }
    if (unique) {
	dico_list_set_comparator(list, compare_ref, NULL);
	dico_list_set_flags(list, DICO_LIST_COMPARE_TAIL);
    }
    dico_list_set_free_item(list, free_ref, NULL);
    return list;
}

static int
gcide_result_list_append(dico_list_t list, struct gcide_ref *ref)
{
    struct gcide_ref *copy = calloc(1,sizeof(*copy));
    if (!copy) {
	DICO_LOG_ERRNO();
	return -1;
    }
    *copy = *ref;
    copy->ref_headword = strdup(ref->ref_headword);
    if (!copy->ref_headword ||
	(dico_list_append(list, copy) && errno == ENOMEM)) {
	DICO_LOG_ERRNO();
	free(copy);
	return -1;
    }
    return 0;
}

struct match_closure {
    dico_strategy_t strat;
    dico_list_t list;
    struct dico_key key;
};

static int
match_key(struct gcide_ref *ref, void *data)
{
    struct match_closure *clos = data;

    if (dico_key_match(&clos->key, ref->ref_headword)) {
	if (gcide_result_list_append(clos->list, ref))
	    return 1;
    }
    return 0;
}

static dico_result_t
gcide_match_all(struct gcide_db *db, const dico_strategy_t strat,
		const char *word)
{
    struct gcide_result *res;
    struct match_closure clos;

    clos.list = gcide_create_result_list(1);
    if (!clos.list)
	return NULL;

    if (dico_key_init(&clos.key, strat, word)) {
	dico_log(L_ERR, 0, _("%s: key initialization failed"), __func__);
	dico_list_destroy(&clos.list);
	return NULL;
    }

    clos.strat = strat;
    gcide_idx_enumerate(db->idx, match_key, &clos);

    dico_key_deinit(&clos.key);

    if (dico_list_count(clos.list) == 0) {
	dico_list_destroy(&clos.list);
	return NULL;
    }

    res = calloc(1, sizeof(*res));
    if (!res) {
	DICO_LOG_ERRNO();
	dico_list_destroy(&clos.list);
    } else {
	res->type = result_match;
	res->db = db;
	res->list = clos.list;
	res->compare_count = gcide_idx_defs(db->idx);
    }

    return (dico_result_t) res;
}

static dico_result_t
gcide_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    struct gcide_db *db = (struct gcide_db *) hp;
    matcher_t matcher = find_matcher(strat->name);
    gcide_iterator_t itr;
    struct gcide_result *res = NULL;

    if (reload_if_changed(db))
	return NULL;

    if (!matcher)
	return gcide_match_all(db, strat, word);
    itr = matcher(db, word);
    if (itr) {
	res = calloc(1, sizeof(*res));
	if (!res) {
	    DICO_LOG_ERRNO();
	    gcide_iterator_free(itr);
	    return NULL;
	}

	res->type = result_match;
	res->db = db;
	res->list = gcide_create_result_list(1);
	if (!res->list) {
	    free(res);
	    gcide_iterator_free(itr);
	    return NULL;
	}

	do
	    gcide_result_list_append(res->list, gcide_iterator_ref(itr));
	while (gcide_iterator_next(itr) == 0);
	res->compare_count = gcide_iterator_compare_count(itr);
	gcide_iterator_free(itr);
    }
    return (dico_result_t) res;
}

static dico_result_t
gcide_define(dico_handle_t hp, const char *word)
{
    struct gcide_db *db = (struct gcide_db *) hp;
    gcide_iterator_t itr;
    struct gcide_result *res = NULL;

    if (reload_if_changed(db))
	return NULL;

    itr = exact_match(db, word);
    if (itr) {
	res = calloc(1, sizeof(*res));
	if (!res) {
	    DICO_LOG_ERRNO();
	    gcide_iterator_free(itr);
	    return NULL;
	}

	res->type = result_define;
	res->db = db;
	res->list = gcide_create_result_list(0);
	if (!res->list) {
	    free(res);
	    gcide_iterator_free(itr);
	    return NULL;
	}

	do
	    gcide_result_list_append(res->list, gcide_iterator_ref(itr));
	while (gcide_iterator_next(itr) == 0);
	res->compare_count = gcide_iterator_compare_count(itr);
	gcide_iterator_free(itr);
    }
    return (dico_result_t) res;
}

static struct gcide_ref *
gcide_result_ref(struct gcide_result *res)
{
    struct gcide_ref *ref = NULL;
    if (!res->itr) {
	res->itr = dico_list_iterator(res->list);
	if (!res->itr)
	    return NULL;
	ref = dico_iterator_first(res->itr);
    } else
	ref = dico_iterator_next(res->itr);
    return ref;
}

struct print_text_closure;
typedef void (*tag_text_printer)(struct gcide_tag *, struct print_text_closure *);

struct print_text_closure {
    tag_text_printer printer;
    dico_stream_t stream;
    unsigned indent;
    int flags;
    int newline;
};

static int
print_text_helper(void *item, void *data)
{
    struct gcide_tag *tag = item;
    struct print_text_closure *p = data;
    p->printer(tag, p);
    return 0;
}

static void print_text_tag(struct gcide_tag *tag, struct print_text_closure *);

static void
print_text_taglist(struct gcide_tag *tag, struct print_text_closure *clos)
{
    struct print_text_closure c = *clos;
    c.printer = print_text_tag;
    dico_list_iterate(tag->v.tag.taglist, print_text_helper, &c);
    clos->newline = c.newline;
}

static void print_text_as(struct gcide_tag *, struct print_text_closure *);
static void print_text_er(struct gcide_tag *, struct print_text_closure *);
static void print_text_pr(struct gcide_tag *, struct print_text_closure *);
static void print_text_a(struct gcide_tag *, struct print_text_closure *);
static void print_text_source(struct gcide_tag *, struct print_text_closure *);
static void print_text_rj(struct gcide_tag *, struct print_text_closure *);
static void print_text_qau(struct gcide_tag *, struct print_text_closure *);
static void print_text_sn(struct gcide_tag *, struct print_text_closure *);
static void print_text_q(struct gcide_tag *, struct print_text_closure *);

static struct tagdef_text {
    char const *tag;
    tag_text_printer printer;
} tagdef_text[] = {
    { "as",     print_text_as },
    { "er",     print_text_er },
    { "pr",     print_text_pr },
    { "a",      print_text_a },
    { "source", print_text_source },
    { "qau",    print_text_qau },
    { "rj",     print_text_rj },
    { "sn",     print_text_sn },
    { "q",      print_text_q },
    { NULL }
};

static tag_text_printer
find_text_printer(char const *name)
{
    struct tagdef_text *p;
    for (p = tagdef_text; p->tag; p++)
	if (strcmp(p->tag, name) == 0)
	    return p->printer;
    return NULL;
}

static void
do_indent(struct print_text_closure *clos)
{
    static char indent[] = "    ";
    int i;
    for (i = 0; i < clos->indent; i++)
	dico_stream_write(clos->stream, indent, strlen(indent));
    clos->newline = 0;
}

static void
print_text_tag(struct gcide_tag *tag, struct print_text_closure *clos)
{
    tag_text_printer printer;

    switch (tag->tag_type) {
    case gcide_content_top:
	print_text_taglist(tag, clos);
	break;

    case gcide_content_tag:
	if (gcide_is_block_tag(tag)) {
	    if (clos->newline == 0) {
		dico_stream_write(clos->stream, "\n", 1);
		clos->newline++;
	    }
	    if (clos->newline < 2) {
		dico_stream_write(clos->stream, "\n", 1);
		clos->newline++;
	    }
	} else if (clos->newline && clos->indent)
	    do_indent(clos);
	printer = find_text_printer(tag->tag_name);
	if (printer)
	    printer(tag, clos);
	else
	    print_text_taglist(tag, clos);
	if (gcide_is_block_tag(tag)) {
	    if (clos->newline == 0) {
		dico_stream_write(clos->stream, "\n", 1);
		clos->newline++;
	    }
	    if (clos->newline < 2) {
		dico_stream_write(clos->stream, "\n", 1);
		clos->newline++;
	    }
	}
	break;

    case gcide_content_text:
	if (clos->newline && clos->indent)
	    do_indent(clos);
	dico_stream_write(clos->stream, tag->v.text, strlen(tag->v.text));
	clos->newline = 0;
	break;

    case gcide_content_nl:
	if (clos->newline && clos->indent)
	    do_indent(clos);
	dico_stream_write(clos->stream, " ", 1);
	clos->newline = 0;
	break;

    case gcide_content_br:
	dico_stream_write(clos->stream, "\n", 1);
	clos->newline++;
	break;

    default:
	break;
    }
}

static void
print_text_as(struct gcide_tag *tag, struct print_text_closure *clos)
{
    static char *quote[2] = { "“", "”" };

    dico_stream_write(clos->stream, quote[0], strlen(quote[0]));
    print_text_taglist(tag, clos);
    dico_stream_write(clos->stream, quote[1], strlen(quote[1]));
    clos->newline = 0;
}

static void
print_text_er(struct gcide_tag *tag, struct print_text_closure *clos)
{
    static char *ref[2] = { "{" , "}" };
    dico_stream_write(clos->stream, ref[0], strlen(ref[0]));
    print_text_taglist(tag, clos);
    dico_stream_write(clos->stream, ref[1], strlen(ref[1]));
}

static void
print_text_pr(struct gcide_tag *tag, struct print_text_closure *clos)
{
    if (!(clos->flags & GCIDE_NOPR))
	print_text_taglist(tag, clos);
}

static char *
gcide_tag_get_param(struct gcide_tag *tag, char const *name)
{
    size_t i;
    size_t namelen = strlen(name);

    for (i = 0; i < tag->v.tag.tag_parmc; i++) {
	size_t len = strcspn(tag->v.tag.tag_parmv[i], "=");
	if (len == namelen &&
	    memcmp(tag->v.tag.tag_parmv[i], name, namelen) == 0)
	    return tag->v.tag.tag_parmv[i]+namelen+1;
    }
    return NULL;
}

static void
print_text_a(struct gcide_tag *tag, struct print_text_closure *clos)
{
    char *href = gcide_tag_get_param(tag, "href");
    print_text_taglist(tag, clos);
    if (href != NULL) {
	dico_stream_write(clos->stream, " (see ", 6);
	dico_stream_write(clos->stream, href, strlen(href));
	dico_stream_write(clos->stream, ")", 1);
    }
}

static void
print_text_source(struct gcide_tag *tag, struct print_text_closure *clos)
{
    dico_stream_write(clos->stream, "[", 1);
    print_text_taglist(tag, clos);
    dico_stream_write(clos->stream, "]", 1);
}

static void
print_text_qau(struct gcide_tag *tag, struct print_text_closure *clos)
{
    dico_stream_write(clos->stream, "-- ", 3);
    print_text_taglist(tag, clos);
}

static void
print_text_sn(struct gcide_tag *tag, struct print_text_closure *clos)
{
    print_text_taglist(tag, clos);
    dico_stream_write(clos->stream, " ", 1);
    clos->newline = 2;
}

static void
print_text_taglist_indent(struct gcide_tag *tag, struct print_text_closure *clos,
		     unsigned level)
{
    struct print_text_closure c = {
	.printer = print_text_tag,
	.stream  = clos->stream,
	.flags   = clos->flags,
	.newline = clos->newline,
	.indent  = level
    };
    print_text_taglist(tag, &c);
    clos->newline = c.newline;
}

static void
print_text_q(struct gcide_tag *tag, struct print_text_closure *clos)
{
    print_text_taglist_indent(tag, clos, 1);
}

static void
print_text_rj(struct gcide_tag *tag, struct print_text_closure *clos)
{
    print_text_taglist_indent(tag, clos, 3);
}

static int
output_def_text(dico_stream_t str, struct gcide_db *db,
		struct gcide_parse_tree *tree)
{
	struct print_text_closure c = {
	    .printer = print_text_tag,
	    .stream  = str,
	    .flags   = db->flags,
	    .newline = 2,
	};
	print_text_tag(tree->root, &c);
	return 0; // FIXME
}

struct html_closure;
typedef void (*tag_html_printer)(struct gcide_tag *, struct html_closure *);

struct html_closure {
    tag_html_printer printer;
    int flags;
    dico_stream_t stream;
};

static int
print_html_helper(void *item, void *data)
{
    struct gcide_tag *tag = item;
    struct html_closure *p = data;
    p->printer(tag, p);
    return 0;
}

static void print_html_tag(struct gcide_tag *tag, struct html_closure *);

static void
print_html_taglist(struct gcide_tag *tag, struct html_closure *clos)
{
    struct html_closure c = *clos;
    c.printer = print_html_tag;
    dico_list_iterate(tag->v.tag.taglist, print_html_helper, &c);
}

static void
copy_html_tag(struct gcide_tag *tag, struct html_closure *clos)
{
    int i;

    dico_stream_write(clos->stream, "<", 1);
    dico_stream_write(clos->stream, tag->tag_name,
		      strlen(tag->tag_name));
    for (i = 1; i < tag->v.tag.tag_parmc; i++) {
	size_t len = strcspn(tag->v.tag.tag_parmv[i], "=");
	dico_stream_write(clos->stream, " ", 1);
	dico_stream_write(clos->stream, tag->v.tag.tag_parmv[i], len);
	if (tag->v.tag.tag_parmv[i][len]) {
	    char *arg = tag->v.tag.tag_parmv[i] + len + 1;
	    dico_stream_write(clos->stream, "=\"", 2);
	    dico_stream_write(clos->stream, arg, strlen(arg));
	    dico_stream_write(clos->stream, "\"", 1);
	}
    }
    dico_stream_write(clos->stream, ">", 1);
    print_html_taglist(tag, clos);
    dico_stream_write(clos->stream, "</", 2);
    dico_stream_write(clos->stream, tag->tag_name, strlen(tag->tag_name));
    dico_stream_write(clos->stream, ">", 1);
}

static void
override_html_tag(struct gcide_tag *tag, struct html_closure *clos,
		  char **parmv)
{
    struct gcide_tag tag_copy = *tag;
    int i;
    for (i = 0; parmv[i] != NULL; i++)
	;
    tag_copy.tag_type = gcide_content_tag;
    tag_copy.v.tag.tag_parmc = i;
    tag_copy.v.tag.tag_parmv = parmv;
    copy_html_tag(&tag_copy, clos);
}

static void
print_html_pr(struct gcide_tag *tag, struct html_closure *clos)
{
    static char *params[] = {
	"span",
	"class=pron",
	NULL
    };
    if (!(clos->flags & GCIDE_NOPR))
	override_html_tag(tag, clos, params);
}

static void print_html_override(struct gcide_tag *tag,
				struct html_closure *clos,
				char const *tagname, char const *class);

static void
print_html_er(struct gcide_tag *tag, struct html_closure *clos)
{
    struct gcide_tag *elt;

    if (dico_list_count(tag->v.tag.taglist) == 1 &&
	(elt = dico_list_head(tag->v.tag.taglist)) != NULL &&
	elt->tag_type == gcide_content_text) {
	static char *params[] = {
	    "a",
	    "class=ref",
	    NULL,
	    NULL
	};
	static char href[] = "href=/define/";

	size_t len = strlen(elt->v.text);
	char *ref = malloc(sizeof(href) + len);
	strcat(strcpy(ref, href), elt->v.text);
	params[2] = ref;
	override_html_tag(tag, clos, params);
	free(ref);
    } else {
	print_html_override(tag, clos, "span", tag->tag_name);
    }
}

static struct tagdef_html {
    char const *tag;
    tag_html_printer printer;
    char const *html_tag;
    char const *class;
} tagdef_html[] = {
    { "p",   copy_html_tag },
    { "a",   copy_html_tag },
    { "pr",  print_html_pr },
    { "er",  print_html_er },
    { "hw",  NULL, "span", "hw" },
    { "sn",  NULL, "li", "def" },
    { "ol",  NULL, "ol", "def" },
    { "grk", NULL, "span", "ets" },
    { "q",   NULL, "blockquote" },
    { NULL }
};

static struct tagdef_html *
find_tagdef_html(char const *name)
{
    struct tagdef_html *p;
    for (p = tagdef_html; p->tag; p++)
	if (strcmp(p->tag, name) == 0)
	    return p;
    return NULL;
}

static void
print_html_override(struct gcide_tag *tag, struct html_closure *clos,
		    char const *tagname, char const *class)
{
    char *params[] = {
	(char*) tagname,
	NULL,
	NULL
    };
    if (class) {
	static char const prefix[] = "class=";
	char *p = malloc(sizeof(prefix) + strlen(class));
	if (!p)
	    DICO_LOG_MEMERR();
	else {
	    strcpy(p, prefix);
	    strcat(p, class);
	    params[1] = p;
	}
    }
    override_html_tag(tag, clos, params);
    free(params[1]);
}

static void
print_html_top(struct gcide_tag *tag, struct html_closure *clos)
{
    print_html_override(tag, clos, "div", "definition");
}

static void
print_html_tag(struct gcide_tag *tag, struct html_closure *clos)
{
    struct tagdef_html *td;

    switch (tag->tag_type) {
    case gcide_content_top:
	print_html_top(tag, clos);
	break;

    case gcide_content_tag:
	td = find_tagdef_html(tag->tag_name);
	if (td) {
	    if (td->html_tag)
		print_html_override(tag, clos, td->html_tag, td->class);
	    else
		td->printer(tag, clos);
	} else if (gcide_is_block_tag(tag))
	    print_html_override(tag, clos, "div", tag->tag_name);
	else
	    print_html_override(tag, clos, "span", tag->tag_name);
	break;

    case gcide_content_text:
	dico_stream_write(clos->stream, tag->v.text, strlen(tag->v.text));
	break;

    case gcide_content_nl:
	dico_stream_write(clos->stream, " ", 1);
	break;

    case gcide_content_br:
	dico_stream_write(clos->stream, "<br/>", 5);
	break;

    default:
	abort ();
    }
}

/*
 * Whenever a <p><sn>(...)</sn>(...)</p> is encountered, replace it with
 * <sn>\2</sn>.  Throw away the original "sn" tag.
 */
static int
p_sn_join(void *item, void *data)
{
    struct gcide_tag *tag = item;
    struct gcide_tag *head;

    if (gcide_is_tag(tag, "p") &&
	gcide_is_tag(head = dico_list_head(tag->v.tag.taglist), "sn")) {
	/* Replace p with sn */
	free(tag->tag_name);
	tag->tag_name = head->tag_name;
	head->tag_name = NULL;
	dico_list_remove(tag->v.tag.taglist, head, NULL);
    }
    return 0;
}

static void
q_fixup(dico_list_t list)
{
    struct gcide_tag *tag;
    dico_iterator_t itr;
    itr = dico_list_iterator(list);
    tag = dico_iterator_first(itr);
    while (tag) {
	if (gcide_is_tag(tag, "q")) {
	    struct gcide_tag *next = dico_iterator_next(itr), *head;
	    if (gcide_is_tag(next, "rj") &&
		(head = dico_list_head(next->v.tag.taglist)) != NULL &&
		gcide_is_tag(head, "qau")) {
		char const tagname[] = "gcide_quote";
		struct gcide_tag *gq = gcide_tag_alloc(tagname, sizeof(tagname)-1);

		/* Get back to <q> and remove it. */
		dico_iterator_prev(itr);
		dico_iterator_remove_current(itr, (void**)&tag);
		/* Add <q> to the <gcide_quote> tag. */
		dico_list_append(gq->v.tag.taglist, tag);

		/* Advance to <rj> and remove it, */
		dico_iterator_next(itr);
		dico_iterator_remove_current(itr, (void**)&tag);
		/* Add <rj> to the <gcide_quote> tag. */
		dico_list_append(gq->v.tag.taglist, tag);

		/* Insert <gcide_quote> */
		dico_iterator_insert_before(itr, gq);
	    }
	} else if (tag->tag_type == gcide_content_top ||
		   tag->tag_type == gcide_content_tag)
	    q_fixup(tag->v.tag.taglist);
	tag = dico_iterator_next(itr);
    }
    dico_iterator_destroy(&itr);
}

static int
output_def_html(dico_stream_t str, struct gcide_db *db,
		struct gcide_parse_tree *tree)
{
    struct html_closure c = {
	.printer = print_html_tag,
	.stream  = str,
	.flags   = db->flags,
    };
    dico_iterator_t itr;
    struct gcide_tag *tag;

    /* Additional fix-ups: */

    /* Join "<p><sn>(.*)</sn>(.*)</p>" -> "<sn>\2</sn>". */
    dico_list_iterate(tree->root->v.tag.taglist, p_sn_join, NULL);

    /* Additional fixups for <q></q><rj><qau> sequences/ */
    q_fixup(tree->root->v.tag.taglist);

    itr = dico_list_iterator(tree->root->v.tag.taglist);
    tag = dico_iterator_first(itr);

    /*
     * Replace initial <p> tag with its content.
     */
    if (gcide_is_tag(tag, "p")) {
	void *t;
	dico_iterator_t pitr;
	struct gcide_tag *p;

	dico_iterator_remove_current(itr, &t);
	pitr = dico_list_iterator(tag->v.tag.taglist);
	p = dico_iterator_last(pitr);
	while (p) {
	    dico_iterator_remove_current(pitr, &t);
	    dico_list_prepend(tree->root->v.tag.taglist, p);
	    p = dico_iterator_prev(pitr);
	}
	dico_iterator_destroy(&pitr);
	gcide_tag_free(tag);
	tag = dico_iterator_first(itr);
    }

    /* Find first <sn> tag. */
    while (tag && !gcide_is_tag(tag, "sn")) {
	tag = dico_iterator_next(itr);
    }

    if (tag) {
	/*
	 * Move it and all subsequent tags under a new <ol> tag, and
	 * append that to the end of tag list.
	 */
	struct gcide_tag *oltag = gcide_tag_alloc("ol", 2);
	do {
	    struct gcide_tag *p;
	    dico_iterator_remove_current(itr, (void **)&p);
	    dico_list_append(oltag->v.tag.taglist, p);
	} while ((tag = dico_iterator_next(itr)) != NULL);
	dico_list_append(tree->root->v.tag.taglist, oltag);

	dico_iterator_destroy(&itr);

	/*
	 * Rewrite the content of <ol> so that it contains only
	 * <sn> tags.  Tuck anything between the two consecutive
	 * <sn> tags to the tag list of the first one.
	 */
	itr = dico_list_iterator(oltag->v.tag.taglist);
	tag = dico_iterator_first(itr);

	while (tag) {
	    if (gcide_is_tag(tag, "sn")) {
		struct gcide_tag *p;

		if (dico_list_count(tag->v.tag.taglist) == 1 &&
		    (p = dico_list_head(tag->v.tag.taglist)) != NULL &&
		    p->tag_type == gcide_content_text)
		    dico_list_clear(tag->v.tag.taglist);
		while ((p = dico_iterator_next(itr)) != NULL &&
		       !gcide_is_tag(p, "sn")) {
		    dico_iterator_remove_current(itr, (void **)&p);
		    dico_list_append(tag->v.tag.taglist, p);
		}
		tag = p;
	    } else
		tag = dico_iterator_next(itr);
	}
    }

    dico_iterator_destroy(&itr);
    /* End of fixups. Now output the resulting document. */
    print_html_tag(tree->root, &c);
    return 0; // FIXME
}

static int
output_def(dico_stream_t str, struct gcide_db *db, struct gcide_ref *ref)
{
    char *buffer;
    struct gcide_parse_tree *tree;
    struct gcide_locus locus;
    int rc;

    if (db->file_letter != ref->ref_letter) {
	int rc;

	if (db->file_stream) {
	    dico_stream_close(db->file_stream);
	    dico_stream_destroy(&db->file_stream);
	    db->file_letter = 0;
	}

	*db->tmpl_letter = ref->ref_letter;

	db->file_stream =
	    dico_mapfile_stream_create(db->tmpl_name,
				       DICO_STREAM_READ|DICO_STREAM_SEEK);
	if (!db->file_stream) {
	    dico_log(L_ERR, errno, _("cannot create stream `%s'"),
		     db->tmpl_name);
	    return 1;
	}
	rc = dico_stream_open(db->file_stream);
	if (rc) {
	    dico_log(L_ERR, 0,
		     _("cannot open stream `%s': %s"),
		     db->tmpl_name, dico_stream_strerror(db->file_stream, rc));
	    dico_stream_destroy(&db->file_stream);
	    return 1;
	}
	db->file_letter = ref->ref_letter;
    }

    if (dico_stream_seek(db->file_stream, ref->ref_offset, SEEK_SET) < 0) {
	dico_log(L_ERR, errno,
		 _("seek error on `%s' while positioning to %lu: %s"),
		 db->tmpl_name, ref->ref_offset,
		 dico_stream_strerror(db->file_stream,
				      dico_stream_last_error(db->file_stream)));
	return 1;
    }

    buffer = malloc(ref->ref_size);
    if (!buffer) {
	DICO_LOG_ERRNO();
	return 1;
    }

    if ((rc = dico_stream_read(db->file_stream, buffer, ref->ref_size, NULL))) {
	dico_log(L_ERR, 0, _("%s: read error: %s"),
		 db->tmpl_name,
		 dico_stream_strerror(db->file_stream, rc));
	free(buffer);
	return 1;
    }

    locus.file = db->tmpl_name;
    locus.offset = ref->ref_offset;
    tree = gcide_markup_parse(buffer, ref->ref_size, db->flags & GCIDE_DBGLEX,
			      &locus);
    if (!tree) {
	// FIXME: if html?
	rc = dico_stream_write(str, buffer, ref->ref_size);
    } else if (db->flags & GCIDE_HTML)
	rc = output_def_html(str, db, tree);
    else
	rc = output_def_text(str, db, tree);
    free(buffer);
    return rc;
}

static int
gcide_output_result(dico_result_t rp, size_t n, dico_stream_t str)
{
    struct gcide_result *res = (struct gcide_result *) rp;
    struct gcide_ref *ref;

    ref = gcide_result_ref(res);
    if (!ref)
	return 1;
    switch (res->type) {
    case result_match:
	dico_stream_write(str, ref->ref_headword, ref->ref_hwbytelen - 1);
	break;

    case result_define:
	return output_def(str, res->db, ref);
    }
    return 0;
}

static size_t
gcide_result_count(dico_result_t rp)
{
    struct gcide_result *res = (struct gcide_result *) rp;
    return dico_list_count(res->list);
}

static size_t
gcide_compare_count(dico_result_t rp)
{
    struct gcide_result *res = (struct gcide_result *) rp;
    return res->compare_count;
}

static void
gcide_free_result(dico_result_t rp)
{
    struct gcide_result *res = (struct gcide_result *) rp;
    dico_iterator_destroy(&res->itr);
    dico_list_destroy(&res->list);
}

static char const *mime_headers[] = {
    "Content-Type: text/plain; charset=utf-8\n"
    "Content-Transfer-Encoding: 8bit\n",

    "Content-Type: text/html; charset=utf-8\n"
    "Content-Transfer-Encoding: 8bit\n"
};

static char *
gcide_db_mime_header(dico_handle_t hp)
{
    struct gcide_db *db = (struct gcide_db *) hp;
    return strdup(mime_headers[(db->flags & GCIDE_HTML) != 0]);
}

struct dico_database_module DICO_EXPORT(gcide, module) = {
    .dico_version = DICO_MODULE_VERSION,
    .dico_capabilities = DICO_CAPA_NONE,
    .dico_init = gcide_init,
    .dico_init_db = gcide_init_db,
    .dico_free_db = gcide_free_db,
    .dico_db_info = gcide_info,
    .dico_db_descr = gcide_descr,
    .dico_match = gcide_match,
    .dico_define = gcide_define,
    .dico_output_result = gcide_output_result,
    .dico_result_count = gcide_result_count,
    .dico_compare_count = gcide_compare_count,
    .dico_free_result = gcide_free_result,
    .dico_db_mime_header = gcide_db_mime_header
};
