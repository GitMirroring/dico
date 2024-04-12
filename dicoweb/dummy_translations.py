"""
#  This file is part of GNU Dico.
#  Copyright (C) 2008-2009, 2012, 2013, 2023 Wojciech Polak
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

from django.utils.translation import gettext_noop

DUMMY_TRANSLATIONS = (
    # gnu.org.ua
    gettext_noop('Match words exactly'),
    gettext_noop('Match word prefixes'),
    gettext_noop('Match using SOUNDEX algorithm'),
    gettext_noop('Match everything (experimental)'),
    gettext_noop('Match headwords within given Levenshtein distance'),
    gettext_noop('Match headwords within given Levenshtein distance (normalized)'),
    gettext_noop('Match headwords within given Damerau-Levenshtein distance'),
    gettext_noop('Match headwords within given Damerau-Levenshtein distance (normalized)'),
    gettext_noop('POSIX 1003.2 (modern) regular expressions'),
    gettext_noop('Old (basic) regular expressions'),
    gettext_noop('Match word suffixes'),
    gettext_noop('Reverse search in Quechua databases'),
    # dict.org
    gettext_noop('Match headwords exactly'),
    gettext_noop('Match prefixes'),
    gettext_noop('Match substring occurring anywhere in a headword'),
    gettext_noop('Match suffixes'),
    gettext_noop('Match headwords within Levenshtein distance one'),
    gettext_noop('Match separate words within headwords'),
)
