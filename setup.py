#!/usr/bin/env python3

import os
import sys
import io
import platform
import wget
import shutil

from subprocess import run
from subprocess import PIPE



paths = {
    'setup'     : 'config.mk',
    'header'    : 'config.h',
    'kconfig'   : 'Kconfig',
    'kconfiglib': 'extra/third-party/kconfiglib',
}


config = { }



#
# Clone kconfiglib repository and run 'menuconfig.py'
#

def step1(): 
    print('Preparing enviroment')


    if run ( 'command -v git', shell=True, stdout=PIPE, stderr=PIPE).returncode != 0:
        print('error: dependency not satisfied, please install git and try again.')
        sys.exit(1)

    else:
        if not os.path.exists(paths['kconfiglib']):
            run ('git clone --depth=1 https://github.com/kwrx/aplus-kconfiglib %s' % (paths['kconfiglib']), shell=True, stdout=PIPE, stderr=PIPE)


    # Add setup shortcut
    run ('KCONFIG_CONFIG=%s %s/menuconfig.py %s' % (paths['setup'], paths['kconfiglib'], paths['kconfig']), shell=True)

    if not os.path.exists(paths['setup']):
        sys.exit(1)




#
# Generate config.h with 'genconfig.py'
#

def step2(): 
    print('Generate config.h')

    if os.path.exists(paths['header']):
        os.remove(paths['header'])

    run('KCONFIG_CONFIG=%s %s/genconfig.py %s' % (paths['setup'], paths['kconfiglib'], paths['kconfig']), shell=True)






#
# Parsing kconfig and generate config list
#

def step3(): 
    print('Parsing rules')


    with open(paths['setup'], 'r') as fp:

        for j in fp.readlines():
               
            if j.strip() == '':
                continue

            if '#' in j:
                continue


            i = j.strip().split('=')[0]
            d = j.strip().split('=')[1]

            if d[0] == '\"':
                d = d[1:-1]
            
            config[i] = d


#
# Fix $PATH in system environment
#  

def step4():
    print('Configure enviroment')

    os.environ['PATH'] = '%s:%s/bin' % (os.environ['PATH'], config['CONFIG_SYSTEM_PATH_TOOLCHAIN'])



#
# Check dependencies
#  

def step5():
    print('Checking dependencies')


    def required(d):
        print(' - Checking for %s...' % d, end='')
        
        p = run ('command -v %s' % (d), shell=True, stdout=PIPE, stderr=PIPE, universal_newlines=True)
        
        if p.returncode != 0:
            print('not found, dependency not satisfied, please install \'%s\' and try again.' % (d))
            return 1

        else:
            print('%s' % str(p.stdout), end='')
            return 0
    ###


    e = 0

    # Required programs
    e += required('gcc')
    e += required('ld')
    e += required('parted')
    e += required('mkfs.ext2')
    e += required('dd')
    e += required('sudo')
    e += required('sync')
    e += required('bash')
    e += required('make')
    e += required('find')
    e += required('awk')
    e += required('automake')
    e += required('autoconf')
    e += required('fc-scan')

    if e > 0:
        sys.exit(1)


    # x86 only
    if config['CONFIG_COMPILER_HOST'] == 'x86_64' or config['CONFIG_COMPILER_HOST'] == 'i686':
        if required('nasm') == 1:
            sys.exit(1)




#
# Check and configure Sysroot
#

def step6():
    print('Checking sysroot')

    run ('./extra/utils/check-root %s %s' % (config['CONFIG_SYSTEM_PATH_SYSROOT'], config['CONFIG_SYSTEM_TIMEZONE']), shell=True)



#
# Download Packages
#

def step7():
    print('Download Packages')

    run('./extra/utils/get-pkg.py --repo kwrx/aplus-toolchain --prefix %s -i %s-aplus-toolchain' % (config['CONFIG_SYSTEM_PATH_TOOLCHAIN'], config['CONFIG_COMPILER_HOST']), shell=True)
    run('./extra/utils/get-pkg.py --prefix %s -i system-base system-cursors system-fonts system-images system-keymaps' % (config['CONFIG_SYSTEM_PATH_SYSROOT']), shell=True)


#
# Generate build wrapper
#

def step8():
    print('Generate build wrapper')

    fp = open('makew', 'w')
    fp.write('#!/bin/sh\n')
    fp.write('export PATH="$PATH:$(pwd)/%s/bin"\n' % config['CONFIG_SYSTEM_PATH_TOOLCHAIN'])
    fp.write('make --no-print-directory $@\n')
    fp.close()

    run('chmod +x makew', shell=True)



#
# Setup completed, print your settings
#

def complete():
    print (
        '''\
Setup completed!

    Your configuration is:

        Project:        %s-%s v%s
        BuildType:      %s

        Host:           %s
        Sysroot:        %s
        Toolchain:      %s
        Locale:         %s
        Timezone:       %s

        GUI:            %s
        SMP:            %s
        Networking:     %s

    Run:
        $ ./makew [ARGS]

    See %s to more details.
        ''' % 
        (
            config['CONFIG_SYSTEM_NAME'],
            config['CONFIG_SYSTEM_CODENAME'],
            config['CONFIG_SYSTEM_VERSION'],

            ('CONFIG_HAVE_DEBUG' in config.keys() 
                and 'Debug') or 'Release',
            
            config['CONFIG_COMPILER_HOST'],
            config['CONFIG_SYSTEM_PATH_SYSROOT'],
            config['CONFIG_SYSTEM_PATH_TOOLCHAIN'],
            config['CONFIG_SYSTEM_LOCALE'],
            config['CONFIG_SYSTEM_TIMEZONE'],
            
            ('CONFIG_HAVE_GUI' in config.keys() 
                and 'True') or 'False',
            
            ('CONFIG_HAVE_NETWORK' in config.keys() 
                and 'True') or 'False',

            ('CONFIG_HAVE_SMP' in config.keys() 
                and 'True') or 'False',

            paths['header']
        )
    )



steps = [
    step1,
    step2,
    step3,
    step4,
    step5,
    step6,
    step7,
    step8,
    complete,
]

for i in range(0, len(steps)):
    print('(%3d%%) ' % (100 * (i / (len(steps) - 1))), end='')
    steps[i]()


