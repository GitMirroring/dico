"""
#  This file is part of GNU Dico.
#  Copyright (C) 2008-2010, 2012, 2013, 2014, 2023 Wojciech Polak
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

from django.urls import re_path
from django.contrib.staticfiles.urls import staticfiles_urlpatterns

from . import views as app_views

urlpatterns = [
    re_path(r'^$', app_views.index, name='index'),
    re_path(r'^opensearch\.xml$', app_views.opensearch, name='opensearch'),
]

urlpatterns += staticfiles_urlpatterns()
