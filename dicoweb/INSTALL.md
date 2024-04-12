# GNU Dico - Dicoweb INSTALL

## Dicoweb requirements

- Django -- a Python Web framework (https://www.djangoproject.com/)
- wikitrans -- a wiki translator (https://pypi.org/project/wikitrans/)
- pymemcache -- a comprehensive, fast, pure Python memcached client
                (https://pypi.org/project/pymemcache/)

## Installation instructions

Install all needed dependencies using [Poetry](https://python-poetry.org/) or PIP.

```shell
$ poetry install
or
$ pip install -r requirements.txt
```

### Apache deployment

The instructions below assume dicoweb sources are located in
`/var/www/dicoweb`.

1. Create the virtual environment directory in `/var/www/dicoweb`.
Assuming the directory will be named `venv`:

    $ cd /var/www/dicoweb
    $ virtualenv venv

2. Activate the environment:

    $ . venv/bin/activate

3. Install the prerequisites

    $ pip install -r requirements.txt

4. Deactivate the environment

    $ deactivate

5. Rename `settings_sample.py` to `settings.py` and edit your
   local Dicoweb site configuration.
6. Run `python manage.py compilemessages` (if you have 'gettext' installed)
7. Run `python manage.py collectstatic`
8. Enable the following Apache modules: mod_alias, mod_env, mod_wsgi.

   The exact instructions depend on your Apache configuration layout. In
   general, they boil down to adding the following three statements to
   the Apache configuration:

   ```
     LoadModule alias_module modules/mod_alias.so
     LoadModule env_module modules/mod_env.so
     LoadModule wsgi_module modules/mod_wsgi.so
   ```

9. Configure Apache virtual host. The minimal configuration is as
follows:

```
  <VirtualHost *:80>
     ServerName dict.example.org
     DocumentRoot /var/www/dicoweb

     # Configure virtual environment.
     # (If not using virtual environment, omit the WSGIDaemonProcess and
     # WSGIProcessGroup statements).

     # Edit the statement below to match to the actual virtual
     # environment location.
     # Notice the 'locale=' setting.  It is necessary to force the
     # UTF-8 locale.
     WSGIDaemonProcess dicoweb python-home=/var/www/dicoweb/venv locale=en_US.UTF-8
     WSGIProcessGroup dicoweb

     # Start up the module.
     WSGIScriptAlias / /var/www/dicoweb/wsgi.py
     # Provide access to the static files.
     Alias /static /var/www/dicoweb/static
     <Directory "/var/www/dicoweb/">
        AllowOverride All
	Options None
	Require all granted
     </Directory>
  </VirtualHost>
```

For a general discussion of deployment of the Django applications,
please see https://docs.djangoproject.com/en/dev/howto/deployment.

## The development/test server

To start the stand-alone development server, change to `/var/www/dicoweb`
and run the command `python manage.py runserver`. You will see
the following output:

    Performing system checks...
    System check identified no issues (0 silenced).
    October 15, 2023 - 09:35:36
    Django version 4.2.6, using settings 'dicoweb.settings'
    Starting development server at http://127.0.0.1:8000/
    Quit the server with CONTROL-C.

Use your web browser to query the displayed URL.
