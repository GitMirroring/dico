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

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

#include "gcide.h"
#include <errno.h>
#include <appi18n.h>

WATCHER
watcher_setup(char const *dbdir)
{
    WATCHER w;
    int wfd;

    w = calloc(1, sizeof(*w));
    if (!w) {
	DICO_LOG_MEMERR();
	return NULL;
    }

    if ((w->fd = inotify_init()) == -1) {
	DICO_LOG_ERRNO();
	free(w);
	return NULL;
    }

    wfd = inotify_add_watch(w->fd, dbdir,
			    IN_DELETE | IN_CREATE | IN_CLOSE_WRITE |
			    IN_MOVED_FROM | IN_MOVED_TO);

    if (wfd == -1) {
	dico_log(L_ERR, errno, "inotify_add_watch");
	close(w->fd);
	free(w);
	return NULL;
    }

    w->events = POLLIN;

    return w;
}

void
watcher_close(WATCHER w)
{
    if (w) {
	close(w->fd);
	free(w);
    }
}

int
watcher_is_modified(WATCHER w)
{
    int changed = 0;
    int rc;

    if (!w)
	return 1;

    while ((rc = poll(w, 1, 0)) != 0) {
	if (rc == -1) {
	    if (errno != EINTR)
		dico_log(L_ERR, errno, "watcher_is_modified: poll failed");
	    break;
	}

	if (w->revents & POLLIN) {
	    char buffer[4096];
	    int n;

	    n = read(w->fd, buffer, sizeof(buffer));
	    if (n == -1) {
		dico_log(L_ERR, errno, "watcher_is_modified: read");
		return 0;
	    }

	    if (!changed) {
		struct inotify_event *ep = (struct inotify_event *) buffer;
		while (n) {
		    int size;

		    if (ep->wd >= 0) {
			if (ep->mask & IN_IGNORED)
			    /* nothing */ ;
			else if (ep->mask & IN_Q_OVERFLOW)
			    dico_log(L_NOTICE, 0, "event queue overflow");
			else if (ep->mask & IN_UNMOUNT)
			    /* FIXME: no special handling for now */;
			else {
			    changed = 1;
			    break;
			}
		    }
		    size = sizeof(*ep) + ep->len;
		    ep = (struct inotify_event *) ((char*) ep + size);
		    n -= size;
		}
	    }
	}
    }
    return changed;
}
