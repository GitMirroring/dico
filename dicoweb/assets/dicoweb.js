/*
  This file is part of GNU Dico.
  Copyright (C) 2008-2009, 2012, 2013, 2023 Wojciech Polak

  GNU Dico is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3, or (at your option)
  any later version.

  GNU Dico is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with GNU Dico.  If not, see <https://www.gnu.org/licenses/>.
*/

window.onload = function() {

    const options = readCookie('dicoweb_options');
    if (options && options === '1') {
        const o = GID('options');
        if (o) {
            o.className = '';
            GID('toggle_options').innerHTML = _('less options');
        }
    }

    GID('toggle_options').onclick = function() {
        const o = GID('options');
        if (o) {
            if (o.className !== 'hidden') {
                o.className = 'hidden';
                this.innerHTML = _('more options');
                document.cookie = 'dicoweb_options=0';
            }
            else {
                o.className = '';
                this.innerHTML = _('less options');
                document.cookie = 'dicoweb_options=1';
            }
        }
        this.blur();
        return false;
    };

    GID('form').onsubmit = function() {
        const q = GID('q');
        const s = document.forms[0].strategy;
        if (q.value === '' && s.value !== 'all') {
            return false;
        }
        return true;
    };

    if (document.forms[0].server) {
        document.forms[0].server.onchange = function() {
            const q = GID('q');
            let u = '?server=' + this.value;
            if (q && q.value !== '') {
                u += '&q=' + encodeURIComponent(q.value);
            }
            window.location.replace(u);
        };
    }

    function GID(x) {
        return document.getElementById(x);
    }

    function gettext(msg) {
        if (typeof gettext_msg != 'undefined' && gettext_msg[msg]) {
            return gettext_msg[msg];
        }
        return msg;
    }

    function _(msg) {
        return gettext(msg);
    }

    function readCookie(name) {
        const nameEq = name + '=';
        const ca = document.cookie.split(';');
        for (let i = 0; i < ca.length; i++) {
            let c = ca[i];
            while (c.charAt(0) === ' ') {
                c = c.substring(1, c.length);
            }
            if (c.indexOf(nameEq) === 0) {
                return c.substring(nameEq.length, c.length);
            }
        }
        return null;
    }
};
