"""
#  Django settings for Dicoweb project.
#
#  This file is part of GNU Dico.
#  Copyright (C) 2008-2010, 2012-2014, 2023 Wojciech Polak
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

import os

SITE_ROOT = os.path.dirname(os.path.realpath(__file__))
BASE_DIR = SITE_ROOT

DEBUG = False

ALLOWED_HOSTS = [
    'localhost',
    '127.0.0.1',
]

ADMINS = (
    ('gray', 'gray@gnu.org'),
)
MANAGERS = ADMINS

SITE_ID = 1
USE_I18N = True

# Directories where Django looks for translation files.
LOCALE_PATHS = (
    os.path.join(SITE_ROOT, 'locale'),
)

TIME_ZONE = 'UTC'
USE_TZ = True

LANGUAGE_CODE = 'en-us'
LANGUAGE_COOKIE_NAME = 'dicoweb_lang'

SESSION_COOKIE_NAME = 'dicoweb_sid'
SESSION_ENGINE = 'django.contrib.sessions.backends.file'
SESSION_EXPIRE_AT_BROWSER_CLOSE = True

# Caching, see https://docs.djangoproject.com/en/dev/topics/cache/#topics-cache
CACHES = {
    'default': {
        'BACKEND': 'django.core.cache.backends.dummy.DummyCache',
        # 'BACKEND': 'django.core.cache.backends.memcached.PyMemcacheCache',
        'LOCATION': '127.0.0.1:11211',
        'KEY_PREFIX': 'dicoweb',
    },
}

# Absolute path to the directory static files should be collected to.
# Don't put anything in this directory yourself; store your static files
# in apps' "static/" subdirectories and in STATICFILES_DIRS.
# Example: "/var/www/example.com/static/"
STATIC_ROOT = os.path.abspath(os.path.join(SITE_ROOT, 'static'))

# URL prefix for static files.
# Example: "http://example.com/static/", "http://static.example.com/"
STATIC_URL = '/static/'

STATICFILES_STORAGE = 'django.contrib.staticfiles.storage.ManifestStaticFilesStorage'
STATICFILES_FINDERS = (
    'django.contrib.staticfiles.finders.FileSystemFinder',
    'django.contrib.staticfiles.finders.AppDirectoriesFinder',
)
STATICFILES_DIRS = (
    os.path.join(SITE_ROOT, 'assets'),
)

# Make this unique, and don't share it with anybody.
SECRET_KEY = 'SET THIS TO A RANDOM STRING'

assert SECRET_KEY != 'SET THIS TO A RANDOM STRING', 'SECRET_KEY must be long and unique.'

MIDDLEWARE = [
    'django.middleware.cache.UpdateCacheMiddleware',
    'django.contrib.sessions.middleware.SessionMiddleware',
    'django.middleware.locale.LocaleMiddleware',
    'django.middleware.gzip.GZipMiddleware',
    'django.middleware.common.CommonMiddleware',
    'django.middleware.cache.FetchFromCacheMiddleware',
]

ROOT_URLCONF = 'dicoweb.urls'

WSGI_APPLICATION = 'dicoweb.wsgi.application'

TEMPLATES = [
    {
        'BACKEND': 'django.template.backends.django.DjangoTemplates',
        'DIRS': [
            os.path.join(SITE_ROOT, '../run/templates'),
            os.path.join(SITE_ROOT, 'templates'),
        ],
        'APP_DIRS': True,
        'OPTIONS': {
            'context_processors': [
                'django.template.context_processors.debug',
                'django.template.context_processors.request',
            ],
        },
    },
]

INSTALLED_APPS = (
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.sites',
    'django.contrib.staticfiles',
    'dicoweb',
)

# A sample logging configuration. The only tangible logging
# performed by this configuration is to send an email to
# the site admins on every HTTP 500 error when DEBUG=False.
# See https://docs.djangoproject.com/en/dev/topics/logging/ for
# more details on how to customize your logging configuration.
LOGGING = {
    'version': 1,
    'disable_existing_loggers': False,
    'filters': {
        'require_debug_false': {
            '()': 'django.utils.log.RequireDebugFalse'
        }
    },
    'handlers': {
        'mail_admins': {
            'level': 'ERROR',
            'filters': ['require_debug_false'],
            'class': 'django.utils.log.AdminEmailHandler'
        }
    },
    'loggers': {
        'django.request': {
            'handlers': ['mail_admins'],
            'level': 'ERROR',
            'propagate': True,
        },
    }
}

#
# Dicoweb specific settings.
#

DICT_SERVERS = ('gnu.org.ua',)
DICT_TIMEOUT = 10

# Optional ONERROR dict controls what to do if certain errors occur.
# So far, only one key is defined:
#
# UNSUPPORTED_CONTENT_TYPE
#    Specifies action to take if a DEFINE request returns article in
#    unsupported content type (with OPTION MIME enabled).
#    Value is a dict. The only mandatory key is 'action', indicating what
#    action to take. Rest of keys depend on its value as shown in the table
#    below:
#
#    'action': 'delete':
#         The offending article will be removed, as if the server has
#         never returned it. This can lead to inconsistencies where
#         headwords returned by MATCH will not be returned by DEFINE.
#         No additional keys are required.
#
#    'action': 'replace':
#         The offending content will be replaced with the value of the
#         'message' key. Keys:
#           'message':     Text to use as a replacement [mandatory]
#           'format_html': Boolean indicating whether to treat the text as HTML.
#                          Optional. Defaults to False.
#
#    'action': 'display':
#         Display the result anyway. Additional key:
#           'format_html': Boolean indicating whether to treat the article as
#                          HTML.
#                          Optional. Defaults to False.
#

ONERROR = {
    'UNSUPPORTED_CONTENT_TYPE': {
        'action': 'replace',
        'message': 'Article cannot be displayed due to unsupported content type',
        'format_html': False,
    }
}
