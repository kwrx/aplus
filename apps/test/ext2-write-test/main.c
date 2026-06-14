#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


#define TEST_PATH    "/ext2-write-test.bin"
#define PAYLOAD_SIZE (5 * 1024 * 1024 + 37)
#define GAP_SIZE     8193


static int fail(const char* operation) {
    perror(operation);
    unlink(TEST_PATH);
    return 1;
}


static void fill_pattern(uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        data[i] = (i * 31U + 17U) & 0xff;
}


int main(void) {
    uint8_t* expected = malloc(PAYLOAD_SIZE + GAP_SIZE + 16);
    uint8_t* actual   = malloc(PAYLOAD_SIZE + GAP_SIZE + 16);

    if (!expected || !actual)
        return fail("malloc");

    fill_pattern(expected, PAYLOAD_SIZE);
    unlink(TEST_PATH);

    int fd = open(TEST_PATH, O_CREAT | O_EXCL | O_RDWR, 0644);
    if (fd < 0)
        return fail("open(create)");

    size_t written = 0;
    while (written < PAYLOAD_SIZE) {
        size_t chunk = 7777;
        if (chunk > PAYLOAD_SIZE - written)
            chunk = PAYLOAD_SIZE - written;

        ssize_t result = write(fd, expected + written, chunk);
        if (result <= 0)
            return fail("write(payload)");

        written += result;
    }

    if (lseek(fd, 13, SEEK_SET) != 13)
        return fail("lseek(overwrite)");

    static const char overwrite[] = "unaligned-overwrite";
    memcpy(expected + 13, overwrite, sizeof(overwrite));

    if (write(fd, overwrite, sizeof(overwrite)) != sizeof(overwrite))
        return fail("write(overwrite)");

    if (lseek(fd, PAYLOAD_SIZE + GAP_SIZE, SEEK_SET) != PAYLOAD_SIZE + GAP_SIZE)
        return fail("lseek(gap)");

    static const char marker[] = "gap-marker";
    memset(expected + PAYLOAD_SIZE, 0, GAP_SIZE);
    memcpy(expected + PAYLOAD_SIZE + GAP_SIZE, marker, sizeof(marker));

    if (write(fd, marker, sizeof(marker)) != sizeof(marker))
        return fail("write(gap)");

    size_t expanded_size = PAYLOAD_SIZE + GAP_SIZE + sizeof(marker);

    if (lseek(fd, 0, SEEK_SET) != 0)
        return fail("lseek(readback)");

    size_t read_bytes = 0;
    while (read_bytes < expanded_size) {
        ssize_t result = read(fd, actual + read_bytes, expanded_size - read_bytes);
        if (result <= 0)
            return fail("read(readback)");

        read_bytes += result;
    }

    if (memcmp(expected, actual, expanded_size) != 0) {
        errno = EIO;
        return fail("compare(readback)");
    }

    size_t shrunk_size = 12345;
    if (ftruncate(fd, shrunk_size) < 0)
        return fail("ftruncate(shrink)");

    if (ftruncate(fd, expanded_size) < 0)
        return fail("ftruncate(grow)");

    if (lseek(fd, shrunk_size, SEEK_SET) != (off_t)shrunk_size)
        return fail("lseek(grown-range)");

    memset(actual, 0xff, expanded_size - shrunk_size);
    if (read(fd, actual, expanded_size - shrunk_size) != (ssize_t)(expanded_size - shrunk_size))
        return fail("read(grown-range)");

    for (size_t i = 0; i < expanded_size - shrunk_size; i++) {
        if (actual[i] != 0) {
            errno = EIO;
            return fail("compare(grown-range)");
        }
    }

    if (close(fd) < 0)
        return fail("close");

    fd = open(TEST_PATH, O_WRONLY | O_TRUNC);
    if (fd < 0)
        return fail("open(truncate)");

    if (close(fd) < 0)
        return fail("close(truncate)");

    struct stat st;
    if (stat(TEST_PATH, &st) < 0 || st.st_size != 0) {
        errno = EIO;
        return fail("stat(truncate)");
    }

    if (unlink(TEST_PATH) < 0)
        return fail("unlink");

    if (stat(TEST_PATH, &st) == 0 || errno != ENOENT) {
        errno = EIO;
        return fail("stat(unlinked)");
    }

    free(actual);
    free(expected);

    printf("ext2-write-test: PASS\n");
    return 0;
}
