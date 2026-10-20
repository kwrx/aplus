/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief Tests for the set*id/getres*id/setgroups family, for chmod()/chown() on tmpfs, ext2 and a pty slave, and for open()'s permission check. Run as root.
 */

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsuid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define SCRATCH "/tmp/cred-test.d"
#define ON_EXT2 "/cred-test.ext2"

#define USER  1000
#define OTHER 2000
#define GROUP 3000


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                        \
    {                                                                      \
        total++;                                                           \
        if (cond) {                                                        \
            printf("cred-test: PASS  %s\n", (name));                       \
        } else {                                                           \
            failures++;                                                    \
            printf("cred-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                  \
    }


/**
 * @brief Tells whether the real, effective and saved user IDs are the given ones.
 *
 * @param r The real ID.
 * @param e The effective ID.
 * @param s The saved ID.
 * @return true if all three match.
 */
static bool resuid_is(uid_t r, uid_t e, uid_t s) {

    uid_t a, b, c;

    if (getresuid(&a, &b, &c) < 0)
        return false;

    return a == r && b == e && c == s;
}

/**
 * @brief Tells whether the real, effective and saved group IDs are the given ones.
 *
 * @param r The real ID.
 * @param e The effective ID.
 * @param s The saved ID.
 * @return true if all three match.
 */
static bool resgid_is(gid_t r, gid_t e, gid_t s) {

    gid_t a, b, c;

    if (getresgid(&a, &b, &c) < 0)
        return false;

    return a == r && b == e && c == s;
}

/**
 * @brief Creates a file holding a few bytes, owned by the caller.
 *
 * @param path The file.
 * @param mode Its permission bits.
 * @return 0 on success, or -1.
 */
static int make_file(const char* path, mode_t mode) {

    unlink(path);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);

    if (fd < 0)
        return -1;

    write(fd, "cred", 4);
    close(fd);

    return chmod(path, mode);
}

/**
 * @brief Runs a check in a child, so that it may give up root without affecting the rest of the test.
 *
 * @param name What the child checks.
 * @param fn The check; it returns 0 when everything held.
 */
static void in_child(const char* name, int (*fn)(void)) {

    pid_t pid = fork();

    if (pid == 0)
        _exit(fn());

    int status = 0;

    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, name, "child failed at step %d", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}


/**
 * @brief The dropbear pattern: switch the effective ID away and back, and check that it really drops privilege while away.
 *
 * @return 0, or the number of the step that failed.
 */
static int child_seteuid_roundtrip(void) {

    if (make_file(SCRATCH "/secret", 0600) < 0)
        return 1;

    if (setegid(USER) < 0 || seteuid(USER) < 0)
        return 2;

    if (!resuid_is(0, USER, 0) || !resgid_is(0, USER, 0))
        return 3;

    if (setfsuid(-1) != USER)
        return 4;

    errno = 0;

    if (open(SCRATCH "/secret", O_RDONLY) >= 0 || errno != EACCES)
        return 5;

    if (setgroups(0, NULL) == 0 || errno != EPERM)
        return 6;

    if (setuid(OTHER) == 0 || errno != EPERM)
        return 7;

    if (seteuid(0) < 0 || setegid(0) < 0)
        return 8;

    if (!resuid_is(0, 0, 0) || !resgid_is(0, 0, 0))
        return 9;

    int fd = open(SCRATCH "/secret", O_RDONLY);

    if (fd < 0)
        return 10;

    close(fd);

    return 0;
}

/**
 * @brief setuid() as root sets all three IDs, after which root cannot be regained.
 *
 * @return 0, or the number of the step that failed.
 */
static int child_setuid_drops(void) {

    if (setgid(USER) < 0 || !resgid_is(USER, USER, USER))
        return 1;

    if (setuid(USER) < 0 || !resuid_is(USER, USER, USER))
        return 2;

    if (seteuid(0) == 0 || errno != EPERM)
        return 3;

    if (setreuid(-1, 0) == 0 || errno != EPERM)
        return 4;

    if (setresuid(-1, -1, USER) < 0)
        return 5;

    return 0;
}

/**
 * @brief setreuid() swaps the real and effective IDs, and setting the real ID updates the saved one.
 *
 * @return 0, or the number of the step that failed.
 */
static int child_setreuid(void) {

    if (setreuid(USER, OTHER) < 0 || !resuid_is(USER, OTHER, OTHER))
        return 1;

    if (setreuid(OTHER, USER) < 0 || !resuid_is(OTHER, USER, USER))
        return 2;

    if (setreuid(-1, 0) == 0 || errno != EPERM)
        return 3;

    return 0;
}

/**
 * @brief An unprivileged owner may change a file's group only to one it belongs to, and never its owner.
 *
 * @return 0, or the number of the step that failed.
 */
static int child_chown_unprivileged(void) {

    if (make_file(SCRATCH "/mine", 0644) < 0 || chown(SCRATCH "/mine", USER, USER) < 0)
        return 1;

    gid_t groups[] = {GROUP};

    if (setgroups(1, groups) < 0)
        return 2;

    if (setresgid(USER, USER, USER) < 0 || setresuid(USER, USER, USER) < 0)
        return 3;

    if (chown(SCRATCH "/mine", 0, -1) == 0 || errno != EPERM)
        return 4;

    if (chown(SCRATCH "/mine", -1, OTHER) == 0 || errno != EPERM)
        return 5;

    if (chown(SCRATCH "/mine", -1, GROUP) < 0)
        return 6;

    if (chmod(SCRATCH "/mine", 0600) < 0)
        return 7;

    if (chmod(SCRATCH "/secret", 0666) == 0 || errno != EPERM)
        return 8;

    return 0;
}


/**
 * @brief Tells whether open() fails with EACCES.
 *
 * @param path The file.
 * @param flags The open() flags.
 * @return true if it was refused for permission.
 */
static bool refused(const char* path, int flags) {

    errno = 0;

    int fd = open(path, flags);

    if (fd >= 0) {
        close(fd);
        return false;
    }

    return errno == EACCES;
}

/**
 * @brief Tells whether open() succeeds.
 *
 * @param path The file.
 * @param flags The open() flags.
 * @return true if a descriptor came back.
 */
static bool allowed(const char* path, int flags) {

    int fd = open(path, flags);

    if (fd < 0)
        return false;

    close(fd);

    return true;
}

/**
 * @brief open() checks the owner, group or other bits that apply, except for O_PATH and a file it creates itself; stat() and chdir() need no read permission.
 *
 * @return 0, or the number of the step that failed.
 */
static int child_open_permissions(void) {

    struct stat st;

    if (make_file(SCRATCH "/o-0600", 0600) < 0 || make_file(SCRATCH "/o-0644", 0644) < 0)
        return 1;

    if (make_file(SCRATCH "/o-0640", 0640) < 0 || chown(SCRATCH "/o-0640", 0, GROUP) < 0)
        return 2;

    if (make_file(SCRATCH "/o-0420", 0420) < 0 || chown(SCRATCH "/o-0420", USER, GROUP) < 0)
        return 3;

    unlink(SCRATCH "/o-new");
    rmdir(SCRATCH "/d-0711");

    if (mkdir(SCRATCH "/d-0711", 0711) < 0 || chmod(SCRATCH "/d-0711", 0711) < 0)
        return 4;

    gid_t groups[] = {GROUP};

    if (setgroups(1, groups) < 0 || setresgid(USER, USER, USER) < 0 || setresuid(USER, USER, USER) < 0)
        return 5;

    if (!refused(SCRATCH "/o-0600", O_RDONLY) || !refused(SCRATCH "/o-0600", O_WRONLY))
        return 6;

    if (!allowed(SCRATCH "/o-0644", O_RDONLY) || !refused(SCRATCH "/o-0644", O_WRONLY) || !refused(SCRATCH "/o-0644", O_RDWR))
        return 7;

    if (!allowed(SCRATCH "/o-0640", O_RDONLY) || !refused(SCRATCH "/o-0640", O_WRONLY))
        return 8;

    if (!refused(SCRATCH "/o-0420", O_WRONLY) || !allowed(SCRATCH "/o-0420", O_RDONLY))
        return 9;

    if (!allowed(SCRATCH "/o-0600", O_PATH))
        return 10;

    int fd = open(SCRATCH "/o-new", O_CREAT | O_EXCL | O_WRONLY, 0444);

    if (fd < 0)
        return 11;

    close(fd);

    if (!refused(SCRATCH "/o-new", O_WRONLY))
        return 12;

    if (stat(SCRATCH "/o-0600", &st) < 0)
        return 13;

    if (!refused(SCRATCH "/d-0711", O_RDONLY | O_DIRECTORY) || chdir(SCRATCH "/d-0711") < 0)
        return 14;

    return 0;
}


/**
 * @brief Prints the IDs after an execve(), for the parent to compare.
 *
 * @return 0.
 */
static int report_resuid(void) {

    uid_t r, e, s;

    getresuid(&r, &e, &s);
    printf("%d %d %d\n", (int)r, (int)e, (int)s);

    return 0;
}

/**
 * @brief execve() copies the effective user ID into the saved one.
 *
 * @param self The path to this program.
 */
static void test_exec(const char* self) {

    int p[2];

    if (pipe(p) < 0)
        return;

    pid_t pid = fork();

    if (pid == 0) {

        dup2(p[1], STDOUT_FILENO);
        close(p[0]);
        close(p[1]);

        setresuid(0, USER, 0);
        execlp(self, self, "--resuid", (char*)NULL);
        _exit(127);
    }

    close(p[1]);

    char buf[64] = {0};
    read(p[0], buf, sizeof(buf) - 1);
    close(p[0]);

    waitpid(pid, NULL, 0);

    CHECK(strcmp(buf, "0 1000 1000\n") == 0, "execve sets saved uid to euid", "got '%s'", buf);
}

/**
 * @brief setgroups() and getgroups(), and their inheritance across fork().
 */
static void test_groups(void) {

    gid_t set[] = {GROUP, USER, 0};
    gid_t got[8];

    CHECK(setgroups(3, set) == 0, "setgroups as root", "%s", strerror(errno));
    CHECK(getgroups(0, NULL) == 3, "getgroups size query", "%d", getgroups(0, NULL));

    int n = getgroups(8, got);
    CHECK(n == 3 && got[0] == GROUP && got[1] == USER && got[2] == 0, "getgroups contents", "n=%d", n);

    errno = 0;
    CHECK(getgroups(1, got) < 0 && errno == EINVAL, "getgroups short buffer", "%s", strerror(errno));

    errno = 0;
    CHECK(setgroups(64, set) < 0 && errno == EINVAL, "setgroups over NGROUPS_MAX", "%s", strerror(errno));

    pid_t pid = fork();

    if (pid == 0)
        _exit(getgroups(8, got) == 3 && got[0] == GROUP ? 0 : 1);

    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "groups inherited by fork", "status %d", status);

    char line[256] = {0};
    FILE* f        = fopen("/proc/self/status", "r");

    if (f) {
        while (fgets(line, sizeof(line), f) && strncmp(line, "Groups:", 7) != 0)
            ;
        fclose(f);
    }

    CHECK(strcmp(line, "Groups:\t3000 1000 0 \n") == 0, "/proc/self/status groups", "got '%s'", line);

    CHECK(setgroups(0, NULL) == 0 && getgroups(0, NULL) == 0, "setgroups clears", "%d", getgroups(0, NULL));
}

/**
 * @brief chmod() and the chown() family on one file, as root.
 *
 * @param where Which filesystem the file is on.
 * @param path The file.
 */
static void test_attrs(const char* where, const char* path) {

    char name[128];
    struct stat st;

    umask(022);

    snprintf(name, sizeof(name), "%s: create", where);
    CHECK(make_file(path, 0644) == 0, name, "%s", strerror(errno));

    snprintf(name, sizeof(name), "%s: chmod ignores umask", where);
    CHECK(chmod(path, 0666) == 0 && stat(path, &st) == 0 && (st.st_mode & 07777) == 0666, name, "mode %o", st.st_mode & 07777);

    snprintf(name, sizeof(name), "%s: chown", where);
    CHECK(chown(path, USER, OTHER) == 0 && stat(path, &st) == 0 && st.st_uid == USER && st.st_gid == OTHER, name, "uid %d gid %d", st.st_uid, st.st_gid);

    snprintf(name, sizeof(name), "%s: chown -1 keeps", where);
    CHECK(chown(path, -1, -1) == 0 && stat(path, &st) == 0 && st.st_uid == USER && st.st_gid == OTHER, name, "uid %d gid %d", st.st_uid, st.st_gid);

    snprintf(name, sizeof(name), "%s: chown clears setuid", where);
    CHECK(chmod(path, 06755) == 0 && chown(path, 0, 0) == 0 && stat(path, &st) == 0 && (st.st_mode & 07777) == 0755, name, "mode %o", st.st_mode & 07777);

    int fd = open(path, O_RDONLY);

    snprintf(name, sizeof(name), "%s: fchown/fchmod", where);
    CHECK(fd >= 0 && fchown(fd, USER, USER) == 0 && fchmod(fd, 0600) == 0 && fstat(fd, &st) == 0 && st.st_uid == USER && (st.st_mode & 07777) == 0600, name, "%s", strerror(errno));

    snprintf(name, sizeof(name), "%s: fchownat AT_EMPTY_PATH", where);
    CHECK(fd >= 0 && fchownat(fd, "", 0, 0, AT_EMPTY_PATH) == 0 && fstat(fd, &st) == 0 && st.st_uid == 0 && st.st_gid == 0, name, "%s", strerror(errno));

    errno = 0;
    snprintf(name, sizeof(name), "%s: fchownat empty path without flag", where);
    CHECK(fd >= 0 && fchownat(fd, "", 0, 0, 0) < 0 && errno == ENOENT, name, "%s", strerror(errno));

    if (fd >= 0)
        close(fd);

    errno = 0;
    snprintf(name, sizeof(name), "%s: chown missing file", where);
    CHECK(chown(SCRATCH "/missing", 0, 0) < 0 && errno == ENOENT, name, "%s", strerror(errno));
}

/**
 * @brief chmod() and chown() on a pty slave, the way an ssh server prepares a session's terminal.
 */
static void test_pty(void) {

    int m = posix_openpt(O_RDWR | O_NOCTTY);

    CHECK(m >= 0 && grantpt(m) == 0 && unlockpt(m) == 0, "open a pty", "%s", strerror(errno));

    if (m < 0)
        return;

    const char* slave = ptsname(m);
    struct stat st;

    CHECK(slave && chmod(slave, 0620) == 0 && stat(slave, &st) == 0 && (st.st_mode & 07777) == 0620 && S_ISCHR(st.st_mode), "pty: chmod", "mode %o", st.st_mode);
    CHECK(slave && chown(slave, USER, OTHER) == 0 && stat(slave, &st) == 0 && st.st_uid == USER && st.st_gid == OTHER, "pty: chown", "uid %d gid %d", st.st_uid, st.st_gid);

    close(m);
}


int main(int argc, char** argv) {

    if (argc > 1 && strcmp(argv[1], "--resuid") == 0)
        return report_resuid();


    setvbuf(stdout, NULL, _IONBF, 0);

    system("rm -rf " SCRATCH);

    if (mkdir(SCRATCH, 0777) < 0) {
        printf("cred-test: mkdir(%s): %s\n", SCRATCH, strerror(errno));
        return 1;
    }

    printf("cred-test: starting\n");

    CHECK(resuid_is(0, 0, 0) && resgid_is(0, 0, 0), "starts as root", "%s", "ids are not all 0");

    test_groups();
    test_attrs("tmpfs", SCRATCH "/attrs");
    test_attrs("ext2", ON_EXT2);
    test_pty();

    in_child("seteuid round trip restricts open", child_seteuid_roundtrip);
    in_child("setuid drops root for good", child_setuid_drops);
    in_child("setreuid", child_setreuid);
    in_child("unprivileged chown/chmod", child_chown_unprivileged);
    in_child("open permissions", child_open_permissions);

    test_exec(argv[0]);

    unlink(ON_EXT2);
    system("rm -rf " SCRATCH);

    printf("cred-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
