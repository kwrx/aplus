#!/usr/bin/env python3

import os
import io
import sys
import argparse
import wget
import shutil
import tarfile

from subprocess import run
from subprocess import PIPE


remote  = 'https://github.com/'
repo    = 'kwrx/aplus-packages'
prefix  = '.'
verbose = False
cache   = '/tmp/get-pkg'



def pkg_get(remote, cache, repo, p):
    """
    Download package from remote repository
    """
    if os.path.exists('%s/%s/%s.tar.xz' % (cache, repo, p)) == False:
        wget.download('%s/%s/releases/latest/download/%s.tar.xz' % (remote, repo, p), '%s/%s/%s.tar.xz' % (cache, repo, p))
        print('')
    else:
        if verbose:
            print('(Cached) %s %s.tar.xz' % (repo, p))



def pkg_process(z, trigger, prefix):
    """
    Process trigger file
    """
    
    try:
        z.getmember('.pkg/%s' % (trigger))
    except:
        if verbose:
            print('%s trigger not found in package' % (trigger))
        return

    if verbose:
        print(' - Extract %s in %s/%s/%s' % (trigger, cache, repo, trigger))

    z.extract('.pkg/%s' % (trigger), '%s/%s' % (cache, repo))


    print('Processing %s...' % (trigger))
    run('%s/%s/.pkg/%s %s' % (cache, repo, trigger, prefix), shell=True)




def pkg_extract(archive, prefix):
    """
    Extract package
    """
    z = tarfile.open(archive, 'r:xz')

    pkg_process(z, 'pre-install', prefix)

    for m in z.getnames():

        if m == '.pkg':
            continue
        
        if m == '.pkg/pre-install':
            continue

        if m == '.pkg/post-install':
            continue



        if os.path.exists('%s/%s' % (prefix, m)) == False:
            
            if verbose:
                print(' - Extract %s in %s' % (m, prefix))

            z.extract(m, path=prefix)


    pkg_process(z, 'post-install', prefix)

    z.close()



def pkg_install(cache, repo, packages, prefix):
    """
    Install package
    """
    for p in packages:
        print('GET %s %s.tar.xz' % (repo, p))
        pkg_get(remote, cache, repo, p)


    for p in packages:
        print("Unpacking %s.tar.xz..." % (p))
        pkg_extract('%s/%s/%s.tar.xz' % (cache, repo, p), prefix)


def pkg_rm(cache, repo, packages, prefix):
    """
    Remove package
    """
    pass

def pkg_list(cache, repo, packages):
    """
    List files from package
    """
    pass



def main(argv):

    global repo
    global prefix
    global verbose
    
    repo = argv.repo
    prefix = argv.prefix
    verbose = argv.verbose

    if argv.clean:
        if os.path.exists(cache):
            shutil.rmtree(cache)

    os.makedirs(cache, 0o755, exist_ok = True)
    os.makedirs('%s/%s' % (cache, repo), 0o755, exist_ok = True)


    if argv.install is not None:
        pkg_install(cache, repo, argv.install, prefix)
    
    if argv.remove is not None:
        pkg_rm(cache, repo, argv.remove, prefix)

    if argv.list is not None:
        pkg_list(cache, repo, argv.list)

    sys.exit(0)




if __name__ == '__main__':
    
    argp = argparse.ArgumentParser(prog='get-pkg.py')

    argp.add_argument('-i', '--install', nargs='+', type=str, help='Install [Package-Name]')
    argp.add_argument('-q', '--list', nargs='+', type=str, help='List files from [Package-Name]')
    argp.add_argument('-r', '--remove', nargs='+', type=str, help='Remove [Package-Name]')


    argp.add_argument('--prefix', type=str, default=prefix, help='Destionation path')
    argp.add_argument('--repo', type=str, default=repo, help="Select another repository")
    argp.add_argument('--clean', const=True, action='store_const', default=False, help='Clean cache directory')
    argp.add_argument('--verbose', const=True, action='store_const', default=verbose, help='Verbose output on stdout')
    argp.add_argument('--version', action='version', version='%(prog)s 1.0')


    main(argp.parse_args())

