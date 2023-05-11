"""fips verb to build the webpage"""

import os
import yaml
import shutil
import subprocess
import glob
from string import Template

from mod import log, util, project

BuildConfig = 'wasm-ninja-release'

systems = [
    'raw', 'raw-ui'
]

#-------------------------------------------------------------------------------
def deploy_webpage(fips_dir, proj_dir, webpage_dir) :
    emsc_deploy_dir = util.get_deploy_dir(fips_dir, 'rawgl', BuildConfig)

    # generate emu HTML pages
    for system in systems :
        log.info('> generate emscripten HTML page: {}'.format(system))
        for ext in ['wasm', 'js'] :
            src_path = '{}/{}.{}'.format(emsc_deploy_dir, system, ext)
            if os.path.isfile(src_path) :
                shutil.copy(src_path, '{}/'.format(webpage_dir))
        with open(proj_dir + '/common/emsc.html', 'r') as f :
            templ = Template(f.read())
        html = templ.safe_substitute(name=system, prog=system)
        with open('{}/{}.html'.format(webpage_dir, system), 'w') as f :
            f.write(html)

#-------------------------------------------------------------------------------
def build_deploy_webpage(fips_dir, proj_dir, rebuild) :
    # if webpage dir exists, clear it first
    proj_build_dir = util.get_deploy_root_dir(fips_dir, 'rawgl')
    webpage_dir = '{}/raw'.format(proj_build_dir)
    if rebuild :
        if os.path.isdir(webpage_dir) :
            shutil.rmtree(webpage_dir)
    if not os.path.isdir(webpage_dir) :
        os.makedirs(webpage_dir)

    # compile samples
    project.gen(fips_dir, proj_dir, BuildConfig)
    project.build(fips_dir, proj_dir, BuildConfig)

    # deploy the webpage
    deploy_webpage(fips_dir, proj_dir, webpage_dir)

    log.colored(log.GREEN, 'Generated Samples web page under {}.'.format(webpage_dir))

#-------------------------------------------------------------------------------
def serve_webpage(fips_dir, proj_dir) :
    proj_build_dir = util.get_deploy_root_dir(fips_dir, 'rawgl')
    webpage_dir = '{}/raw'.format(proj_build_dir)
    p = util.get_host_platform()
    if p == 'osx' :
        try :
            subprocess.call(
                'http-server -c-1 -g -o'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt :
            pass
    elif p == 'win':
        try:
            subprocess.call(
                'http-server -c-1 -g -o'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt:
            pass
    elif p == 'linux':
        try:
            subprocess.call(
                'http-server -c-1 -g -o'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt:
            pass

#-------------------------------------------------------------------------------
def run(fips_dir, proj_dir, args) :
    if len(args) > 0 :
        if args[0] == 'build' :
            build_deploy_webpage(fips_dir, proj_dir, False)
        elif args[0] == 'rebuild' :
            build_deploy_webpage(fips_dir, proj_dir, True)
        elif args[0] == 'serve' :
            serve_webpage(fips_dir, proj_dir)
        else :
            log.error("Invalid param '{}', expected 'build' or 'serve'".format(args[0]))
    else :
        log.error("Param 'build' or 'serve' expected")

#-------------------------------------------------------------------------------
def help() :
    log.info(log.YELLOW +
             'fips webpage build\n' +
             'fips webpage rebuild\n' +
             'fips webpage serve\n' +
             log.DEF +
             '    build raw webpage')
