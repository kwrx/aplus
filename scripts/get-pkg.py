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



def GetPackage(remote, cache, repo, p):
    if os.path.exists('%s/%s/%s.tar.xz' % (cache, repo, p)) == False:
        wget.download('%s/%s/releases/latest/download/%s.tar.xz' % (remote, repo, p), '%s/%s/%s.tar.xz' % (cache, repo, p))
        print('')
    else:
        if verbose:
            print('(Cached) %s %s.tar.xz' % (repo, p))



def ProcessPackage(z, trigger, prefix):
    
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




def ExtractPackage(archive, prefix):
    z = tarfile.open(archive, 'r:xz')

    ProcessPackage(z, 'pre-install', prefix)

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


    ProcessPackage(z, 'post-install', prefix)

    z.close()



def InstallPackages(cache, repo, packages, prefix):

    for p in packages:
        print('GET %s %s.tar.xz' % (repo, p))
        GetPackage(remote, cache, repo, p)


    for p in packages:
        print("Unpacking %s.tar.xz..." % (p))
        ExtractPackage('%s/%s/%s.tar.xz' % (cache, repo, p), prefix)


def RemovePackages(cache, repo, packages, prefix):
    pass

def ListPackages(cache, repo, packages):
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
        InstallPackages(cache, repo, argv.install, prefix)
    
    if argv.remove is not None:
        RemovePackages(cache, repo, argv.remove, prefix)

    if argv.list is not None:
        ListPackages(cache, repo, argv.list)

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

