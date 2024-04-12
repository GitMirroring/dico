"""
#  This file is part of GNU Dico.
#  Copyright (C) 2008-2010, 2012, 2013, 2015, 2023 Wojciech Polak
#
#  GNU Dico is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3, or (at your option)
#  any later version.
#
#  GNU Dico is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with GNU Dico.  If not, see <https://www.gnu.org/licenses/>.
"""

import re
import socket
import base64
import quopri
from functools import reduce
from io import TextIOWrapper
from typing import cast, TypeAlias


__version__ = '1.2'

DicoResponse: TypeAlias = list[str | list[str] | tuple[str, list[str]]]


class DicoClient:

    """GNU Dico client module written in Python
       (a part of GNU Dico software)"""

    host = None
    levenshtein_distance = 0
    mime = False

    verbose = 0
    fd: TextIOWrapper
    socket: socket.socket
    server_capas: list[str]
    server_msgid: str
    timeout = 10
    transcript = False
    __connected = False

    def __init__(self, host: str | None = None):
        if host is not None:
            self.host = host

    def __del__(self):
        if self.__connected:
            self.socket.close()

    def open(self, host: str | None = None, port: int = 2628) -> None:
        """Open the connection to the DICT server."""
        if host is not None:
            self.host = host
        if self.verbose:
            self.__debug('Connecting to %s:%d' % (self.host, port))
        socket.setdefaulttimeout(int(self.timeout))
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.connect((self.host, port))
        self.__connected = True
        self.fd = self.socket.makefile()

        server_banner: str = cast(str, self.__read()[0])
        capas, msgid = re.search('<(.*)> (<.*>)$', server_banner).groups()
        self.server_capas = capas.split('.')
        self.server_msgid = msgid

        self.__send_client()
        self.__read()

    def close(self) -> None:
        """Close the connection."""
        if self.__connected:
            self.__send_quit()
            self.__read()
            self.socket.close()
            self.__connected = False

    def option(self, name: str, *args) -> bool:
        """Send the OPTION command."""
        if self.__connected:
            self.__send('OPTION %s%s' %
                        (name, reduce(lambda x, y: str(x) + ' ' + str(y),
                                      args, '')))
            res = self.__read()
            code, _msg = cast(str, res[0]).split(' ', 1)
            if int(code) == 250:
                if name.lower() == 'mime':
                    self.mime = True
                return True
        return False

    def __get_mime(self, lines: list[str]) -> dict:
        cnt = 0
        mimeinfo = {}
        firstline = lines[0].lower()
        if firstline.find('content-type:') != -1 or \
           firstline.find('content-transfer-encoding:') != -1:
            cnt += 1
            for line in lines:
                if line == '':
                    break
                t = line.split(':', 1)
                mimeinfo[t[0].lower()] = t[1].strip()
                cnt += 1
            for _i in range(0, cnt):
                lines.pop(0)
        else:
            lines.pop(0)
        if 'content-transfer-encoding' in mimeinfo:
            if mimeinfo['content-transfer-encoding'].lower() == 'base64':
                buf = base64.decodestring('\n'.join(lines))
                lines[:] = buf.split('\r\n')
                if lines[-1] == '':
                    del lines[-1]
                del mimeinfo['content-transfer-encoding']
            elif mimeinfo['content-transfer-encoding'].lower() == 'quoted-printable':
                buf = quopri.decodestring('\n'.join(lines))
                try:
                    lines[:] = buf.split('\r\n')
                except TypeError:
                    lines[:] = buf.decode('utf-8').split('\r\n')
                if lines[-1] == '':
                    del lines[-1]
                del mimeinfo['content-transfer-encoding']
        return mimeinfo

    def __get_rs(self, line: str) -> tuple[int, str]:
        code: str
        text: str
        code, text = line.split(' ', 1)
        return int(code), text

    def __read(self) -> DicoResponse:
        if not self.__connected:
            raise DicoNotConnectedError('Not connected')
        buf: DicoResponse = []
        line = self.__readline()
        if len(line) == 0:
            raise DicoNotConnectedError('Not connected')
        buf.append(line)
        code, _text = self.__get_rs(line)

        if 100 <= code < 200:
            if code == 150:
                while True:
                    rs = self.__readline()
                    code, _text = self.__get_rs(rs)
                    if code != 151:
                        buf.append(rs)
                        break
                    buf.append((rs, self.__readblock()))
            else:
                buf.append(self.__readblock())
                buf.append(self.__readline())
        return buf

    def __readline(self) -> str:
        line = self.fd.readline().rstrip()
        if self.transcript:
            self.__debug(f'S:{line}')
        return line

    def __readblock(self) -> list[str]:
        buf: list[str] = []
        while True:
            line = self.__readline()
            if line == '.':
                break
            buf.append(line)
        return buf

    def __send(self, command: str) -> None:
        if not self.__connected:
            raise DicoNotConnectedError('Not connected')
        cmd = command + "\r\n"
        try:
            self.socket.send(cmd)
        except (UnicodeEncodeError, TypeError):
            self.socket.send(cmd.encode('utf-8'))
        if self.transcript:
            self.__debug(f'C:{command}')

    def __send_client(self) -> None:
        if self.verbose:
            self.__debug('Sending client information')
        self.__send('CLIENT "%s %s"' % ("GNU Dico (Python Edition)",
                                        __version__))

    def __send_quit(self) -> None:
        if self.verbose:
            self.__debug('Quitting')
        self.__send('QUIT')

    def __send_show(self, what: str, arg=None) -> DicoResponse:
        if arg is not None:
            self.__send(f'SHOW {what} "{arg}"')
        else:
            self.__send(f'SHOW {what}')
        return self.__read()

    def __send_define(self, database: str, word: str) -> DicoResponse:
        if self.verbose:
            self.__debug('Sending query for word "%s" in database "%s"' %
                         (word, database))
        self.__send(f'DEFINE "{database}" "{word}"')
        return self.__read()

    def __send_match(self, database: str, strategy: str, word: str) -> DicoResponse:
        if self.verbose:
            self.__debug('Sending query to match word "%s" in database "%s", using "%s"'
                         % (word, database, strategy))
        self.__send('MATCH "%s" "%s" "%s"' % (database, strategy, word))
        return self.__read()

    def __send_xlev(self, distance: str) -> DicoResponse:
        self.__send('XLEV %u' % distance)
        return self.__read()

    def show_databases(self) -> dict:
        """List all accessible databases."""
        if self.verbose:
            self.__debug('Getting list of databases')
        res = self.__send_show('DATABASES')
        if self.mime:
            mimeinfo = self.__get_mime(res[1])
        dbs_res = res[1:-1][0]
        dbs = []
        for d in dbs_res:
            short_name, full_name = d.split(' ', 1)
            dbs.append([short_name, self.__unquote(full_name)])
        dct = {
            'count': len(dbs),
            'databases': dbs,
        }
        return dct

    def show_strategies(self) -> dict:
        """List available matching strategies."""
        if self.verbose:
            self.__debug('Getting list of strategies')
        res = self.__send_show('STRATEGIES')
        if self.mime:
            mimeinfo = self.__get_mime(res[1])
        sts_res = res[1:-1][0]
        sts = []
        for s in sts_res:
            short_name, full_name = s.split(' ', 1)
            sts.append([short_name, self.__unquote(full_name)])
        dct = {
            'count': len(sts),
            'strategies': sts,
        }
        return dct

    def show_info(self, database: str) -> dict:
        """Provide information about the database."""
        res = self.__send_show("INFO", database)
        code, msg = res[0].split(' ', 1)
        if int(code) < 500:
            if self.mime:
                mimeinfo = self.__get_mime(res[1])
            dsc = res[1]
            return {'desc': '\n'.join(dsc)}
        return {'error': code, 'msg': msg}

    def show_lang_db(self) -> dict:
        """Show databases with their language preferences."""
        res = self.__send_show('LANG DB')
        code, msg = res[0].split(' ', 1)
        if int(code) < 500:
            if self.mime:
                mimeinfo = self.__get_mime(res[1])
            dsc = res[1]
            lang_src = {}
            lang_dst = {}
            for i in dsc:
                pair = i.split(' ', 1)[1]
                src, dst = pair.split(':', 1)
                for _j in src:
                    lang_src[src.strip()] = True
                for _j in dst:
                    lang_dst[dst.strip()] = True
            return {
                'desc': '\n'.join(dsc),
                'lang_src': list(lang_src.keys()),
                'lang_dst': list(lang_dst.keys()),
            }
        return {'error': code, 'msg': msg}

    def show_lang_pref(self) -> dict:
        """Show server language preferences."""
        res = self.__send_show('LANG PREF')
        code, msg = res[0].split(' ', 1)
        if int(code) < 500:
            return {'msg': msg}
        return {'error': code, 'msg': msg}

    def show_server(self) -> dict:
        """Provide site-specific information."""
        res = self.__send_show('SERVER')
        code, msg = res[0].split(' ', 1)
        if int(code) < 500:
            dsc = res[1]
            return {'desc': '\n'.join(dsc)}
        return {'error': code, 'msg': msg}

    def define(self, database: str, word: str) -> dict:
        """Look up word in database."""
        database = database.replace('"', "\\\"")
        word = word.replace('"', "\\\"")
        res = self.__send_define(database, word)
        code, msg = res[-1].split(' ', 1)
        if int(code) < 500:
            defs_res = res[1:-1]
            defs = []
            rx = re.compile(
                '^\d+ ("[^"]+"|\w+) ([a-zA-Z0-9_/\-]+) ("[^"]*"|\w+)')
            for i in defs_res:
                term, db, db_fullname = rx.search(i[0]).groups()
                df = {
                    'term': self.__unquote(term),
                    'db': db,
                    'db_fullname': self.__unquote(db_fullname),
                }
                if self.mime:
                    mimeinfo = self.__get_mime(i[1])
                    df.update(mimeinfo)
                df['desc'] = '\n'.join(i[1])
                defs.append(df)
            dct = {
                'count': len(defs),
                'definitions': defs,
            }
            return dct
        return {'error': code, 'msg': msg}

    def match(self, database: str, strategy: str, word: str) -> dict:
        """Match word in database using strategy."""
        if not self.__connected:
            raise DicoNotConnectedError('Not connected')

        if self.levenshtein_distance and 'xlev' in self.server_capas:
            res = self.__send_xlev(self.levenshtein_distance)
            code, msg = res[-1].split(' ', 1)
            if int(code) != 250 and self.verbose:
                self.__debug('Server rejected XLEV command')
                self.__debug('Server reply: %s' % msg)

        database = database.replace('"', "\\\"")
        strategy = strategy.replace('"', "\\\"")
        word = word.replace('"', "\\\"")

        res = self.__send_match(database, strategy, word)
        code, msg = res[-1].split(' ', 1)
        if int(code) < 500:
            if self.mime:
                mimeinfo = self.__get_mime(res[1])
            mts_refs = res[1:-1][0]
            mts = {}
            for i in mts_refs:
                db, term = i.split(' ', 1)
                if db in mts:
                    mts[db].append(self.__unquote(term))
                else:
                    mts[db] = [self.__unquote(term)]
            dct = {
                'matches': mts,
            }
            return dct
        return {'error': code, 'msg': msg}

    def xlev(self, distance: str) -> bool:
        """Set Levenshtein distance."""
        self.levenshtein_distance = distance
        res = self.__send_xlev(distance)
        code, _msg = res[0].split(' ', 1)
        if int(code) == 250:
            return True
        return False

    def __unquote(self, s: str) -> str:
        s = s.replace("\\\\'", "'")
        if s[0] == '"' and s[-1] == '"':
            s = s[1:-1]
        try:
            s = self.__decode(s)
        except UnicodeEncodeError:
            pass
        return s

    def __decode(self, encoded: str) -> str:
        for octc in (c for c in re.findall(r'\\(\d{3})', encoded)):
            encoded = encoded.replace(r'\%s' % octc, chr(int(octc, 8)))
        return encoded

    def __debug(self, msg: str) -> None:
        print(f'dico: Debug: {msg}')


class DicoNotConnectedError(Exception):

    def __init__(self, value):
        self.parameter = value

    def __str__(self):
        return repr(self.parameter)
