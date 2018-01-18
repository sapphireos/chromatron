from fabric.api import *
import json
import os

APP_HOME = "/opt/sapphire/"
LOG_HOME = "/var/log/sapphire/"
CONF_HOME = "/etc/supervisor/conf.d"

app_settings = {}

with open('settings.json', 'r') as f:
    app_settings = json.loads(f.read())

APP_PATH = os.path.join(APP_HOME, app_settings['APP_NAME'])


def setup_directories():
    sudo('mkdir -p %s' % (APP_HOME))
    sudo('mkdir -p %s' % (LOG_HOME))
    sudo('chmod 557 %s' % (LOG_HOME))


def setup_user():
    with settings(warn_only=True):
        sudo('adduser sapphire')


def install_dependencies(): 
    for dep in app_settings['DEPS']:
        sudo('pip install --upgrade %s' % (dep))


def copy_files():
    sudo('mkdir -p %s' % (APP_PATH))
    sudo('chown %s %s' % (env.user, APP_PATH))
    sudo('chmod 777 %s' % (APP_PATH))
    put(local_path='*.py', remote_path=APP_PATH)
    put(local_path='*.json', remote_path=APP_PATH)
    put(local_path='*.conf', remote_path=APP_PATH)


def clean_app():
    with settings(warn_only=True):
        with cd(APP_PATH):
            sudo('rm -rf *')

        with cd(CONF_HOME):
            sudo('rm %s.conf' % (app_settings['APP_NAME']))


conf_template = """
[program:(APP_NAME)]
command=python (APP_NAME).py
directory=(APP_PATH)
process_name=%(program_name)s
autostart=true
autorestart=true
stopsignal=TERM
user=sapphire
"""

def make_conf_file():
    conf = conf_template.replace('(APP_NAME)', app_settings['APP_NAME'])
    conf = conf.replace('(APP_PATH)', APP_PATH)

    with open('%s.conf' % (app_settings['APP_NAME']), 'w') as f:
        f.write(conf)

def install_conf_file():
    sudo('mkdir -p %s' % (CONF_HOME))

    with cd(APP_PATH):
        sudo('mv %s.conf %s' % (app_settings['APP_NAME'], CONF_HOME))

def reload_supervisor_config():
    # update processes
    sudo('supervisorctl reread')
    sudo('supervisorctl update')

def restart():
    sudo('supervisorctl restart %s' % (app_settings['APP_NAME']))    

def stop():
    sudo('supervisorctl stop %s' % (app_settings['APP_NAME']))    

def clean():
    with cd(APP_PATH):
        run('rm -rf *')

    sudo('rmdir %s' % (APP_PATH))

    with cd(CONF_HOME):
        sudo('rm %s.conf' % (app_settings['APP_NAME']))

def uninstall():
    stop()
    clean()
    reload_supervisor_config()

def deploy():
    setup_directories()
    setup_user()
    
    install_dependencies()

    make_conf_file()    
    
    clean_app()
    copy_files()
    install_conf_file()
    
    reload_supervisor_config()
    restart()



