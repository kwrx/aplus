#!/usr/bin/env python3

import os
import sys
import time
import argparse
import hashlib
import sqlite3
import wget
import shutil
import tarfile

from subprocess import run


remote       = 'https://github.com/'
repo         = 'kwrx/aplus-packages'
repo_version = 'latest'
prefix       = '.'
verbose      = False
cache        = '/tmp/get-pkg'
db           = None
schema       = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'apps', 'core', 'pkg', 'schema.sql')




def pkg_archive(cache, repo, repo_version, p):
    """
    @brief Path of a package archive in the cache, one directory per repository version.
    @param cache The cache root.
    @param repo The repository.
    @param repo_version The release, or 'latest'.
    @param p The package name.
    @return The archive path.
    """
    return '%s/%s/%s/%s.tar.xz' % (cache, repo, repo_version, p)



def pkg_get(remote, cache, repo, repo_version, p):
    """
    @brief Downloads a package unless that version is cached; 'latest' is always downloaded again.
    @param remote The host serving the releases.
    @param cache The cache root.
    @param repo The repository.
    @param repo_version The release, or 'latest'.
    @param p The package name.
    """
    archive = pkg_archive(cache, repo, repo_version, p)

    if repo_version == 'latest' and os.path.exists(archive):
        os.unlink(archive)

    if os.path.exists(archive) == False:
        if repo_version == 'latest':
            wget.download('%s/%s/releases/latest/download/%s.tar.xz' % (remote, repo, p), archive)
        else:
            wget.download('%s/%s/releases/download/%s/%s.tar.xz' % (remote, repo, repo_version, p), archive)
        print('')
    else:
        if verbose:
            print('(Cached) %s %s %s.tar.xz' % (repo, repo_version, p))



def pkg_dropped(member):
    """
    @brief Reports whether a member is a link into a package staging directory, which is never extracted.
    @param member The member.
    @return True when it is dropped.
    """
    if (member.issym() or member.islnk()) and '/__out/' in member.linkname:
        if verbose:
            print(' - Skip %s -> %s' % (member.name, member.linkname))
        return True

    return False



def pkg_filter(member, path):
    """
    Extraction filter dropping symlinks that point into a package staging directory.
    """
    if pkg_dropped(member):
        return None

    return tarfile.tar_filter(member, path)



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

    z.extract('.pkg/%s' % (trigger), '%s/%s' % (cache, repo), filter=pkg_filter)


    print('Processing %s...' % (trigger))
    run('%s/%s/.pkg/%s %s' % (cache, repo, trigger, prefix), shell=True)




def pkg_target(prefix, member):
    """
    @brief Where a member lands in the prefix.
    @param prefix Where the package is installed.
    @param member The member.
    @return The path.
    """
    return os.path.join(prefix, member.name.lstrip('/'))



def pkg_clear(prefix, member):
    """
    @brief Removes what an older install left where a file, link or device is about to be extracted.
    @param prefix Where the package is installed.
    @param member The member about to be extracted.
    @return False when the member is skipped: a real directory is in the way, or the path resolves outside the prefix.
    """
    target = pkg_target(prefix, member)

    if os.path.lexists(target) == False:
        return True

    root   = os.path.realpath(prefix)
    parent = os.path.realpath(os.path.dirname(target))

    if os.path.commonpath([root, parent]) != root:
        print(' ! Skip %s, it resolves outside %s' % (member.name, prefix))
        return False

    if os.path.isdir(target) and not os.path.islink(target):
        print(' ! Skip %s, a directory is in the way' % (member.name))
        return False

    os.unlink(target)
    return True



def pkg_entry(member):
    """
    @brief The path and type pkg records for an extracted member.
    @param member The member.
    @return (path, type), type being 'd', 'l' or 'f'; None for the archive root.
    """
    path = os.path.normpath('/' + member.name)

    if path == '/':
        return None

    if member.isdir():
        return (path, 'd')

    if member.issym() or member.islnk():
        return (path, 'l')

    return (path, 'f')



def pkg_extract(archive, prefix):
    """
    @brief Extracts a package over the prefix, replacing the files an older version left; existing directories are kept.
    @param archive The package archive.
    @param prefix Where to install it.
    @return The package version and the (path, type) entries it installed.
    """
    z = tarfile.open(archive, 'r:xz')

    pkg_process(z, 'pre-install', prefix)

    entries = []

    for member in z.getmembers():

        if member.name == '.pkg' or member.name.startswith('.pkg/'):
            continue

        if pkg_dropped(member):
            continue

        entry = pkg_entry(member)

        if member.isdir():
            if os.path.lexists(pkg_target(prefix, member)):
                if entry:
                    entries.append(entry)
                continue

        elif pkg_clear(prefix, member) == False:
            continue

        if verbose:
            print(' - Extract %s in %s' % (member.name, prefix))

        z.extract(member, path=prefix, filter=pkg_filter)

        if entry:
            entries.append(entry)


    try:
        version = z.extractfile('.pkg/version').read().decode().strip()
    except:
        version = 'unknown'

    pkg_process(z, 'post-install', prefix)

    z.close()

    return version, entries



def pkg_db_open(path):
    """
    @brief Opens the pkg local index, creating it from the pkg schema.
    @param path The database file.
    @return The connection.
    """
    os.makedirs(os.path.dirname(path), 0o755, exist_ok = True)

    conn = sqlite3.connect(path)

    with open(schema) as fp:
        conn.executescript(fp.read())

    return conn



def pkg_record(conn, archive, p, version, entries):
    """
    @brief Records an installed package in the pkg local index as explicitly installed, taking over the files other packages owned.
    @param conn The local index.
    @param archive The package archive.
    @param p The package name.
    @param version The package version.
    @param entries The (path, type) entries it installed.
    """
    with open(archive, 'rb') as fp:
        sha256 = hashlib.sha256(fp.read()).hexdigest()

    conn.execute('DELETE FROM installed WHERE name = ?', (p,))
    conn.execute('INSERT INTO installed (name, version, repo, sha256, explicit, date) VALUES (?, ?, NULL, ?, 1, ?)', (p, version, sha256, int(time.time())))

    for path, kind in entries:
        if kind != 'd':
            conn.execute("DELETE FROM files WHERE path = ? AND type != 'd' AND package != ?", (path, p))
        conn.execute('INSERT OR IGNORE INTO files (package, path, type) VALUES (?, ?, ?)', (p, path, kind))

    conn.commit()



def pkg_install(cache, repo, repo_version, packages, prefix):
    """
    Install package
    """
    for p in packages:
        print('GET %s %s.tar.xz' % (repo, p))
        pkg_get(remote, cache, repo, repo_version, p)


    conn = pkg_db_open(db) if db else None

    for p in packages:
        print("Unpacking %s.tar.xz..." % (p))

        archive = pkg_archive(cache, repo, repo_version, p)
        version, entries = pkg_extract(archive, prefix)

        if conn:
            pkg_record(conn, archive, p, version, entries)

    if conn:
        conn.close()


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

    global db
    global repo
    global prefix
    global verbose
    global repo_version
    
    repo = argv.repo
    repo_version = argv.repo_version
    prefix = argv.prefix
    verbose = argv.verbose
    db = argv.db

    if argv.clean:
        if os.path.exists(cache):
            shutil.rmtree(cache)

    os.makedirs(cache, 0o755, exist_ok = True)
    os.makedirs('%s/%s/%s' % (cache, repo, repo_version), 0o755, exist_ok = True)


    if argv.install is not None:
        pkg_install(cache, repo, repo_version, argv.install, prefix)
    
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
    argp.add_argument('--db', type=str, default=db, help='Record installed packages in this pkg local index')
    argp.add_argument('--repo', type=str, default=repo, help="Select another repository")
    argp.add_argument('--repo-version', type=str, default=repo_version, help="Select another repository version")
    argp.add_argument('--clean', const=True, action='store_const', default=False, help='Clean cache directory')
    argp.add_argument('--verbose', const=True, action='store_const', default=verbose, help='Verbose output on stdout')
    argp.add_argument('--version', action='version', version='%(prog)s 1.0')


    main(argp.parse_args())

