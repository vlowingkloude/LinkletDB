#include "mmap_file.h"

#ifdef _WIN32
#error "linklet mmap backend is not yet implemented for Windows"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct LinkletMmapFile {
    int fd;
    uint64_t size;
};

static void set_error(LinkletError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

static void set_errno_error(LinkletError *error, const char *operation) {
    set_error(error, "%s: %s", operation, strerror(errno));
}

static LinkletMmapFile *open_file(const char *path, const int flags, LinkletError *error) {
    if (!path || !path[0]) {
        set_error(error, "file path is required");
        return NULL;
    }
    const int fd = open(path, flags, 0644);
    if (fd < 0) {
        set_errno_error(error, "open file");
        return NULL;
    }
    struct stat stat_buffer;
    if (fstat(fd, &stat_buffer) != 0) {
        set_errno_error(error, "stat file");
        close(fd);
        return NULL;
    }
    LinkletMmapFile *file = (LinkletMmapFile *)calloc(1, sizeof(*file));
    if (!file) {
        set_error(error, "out of memory");
        close(fd);
        return NULL;
    }
    file->fd = fd;
    file->size = (uint64_t)stat_buffer.st_size;
    return file;
}

LinkletMmapFile *linklet_mmap_create(const char *path, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    return open_file(path, O_RDWR | O_CREAT | O_EXCL, error);
}

LinkletMmapFile *linklet_mmap_open(const char *path, const bool writable, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    return open_file(path, writable ? O_RDWR : O_RDONLY, error);
}

void linklet_mmap_close(LinkletMmapFile *file) {
    if (!file) {
        return;
    }
    if (file->fd >= 0) {
        close(file->fd);
    }
    free(file);
}

uint64_t linklet_mmap_size(const LinkletMmapFile *file) {
    return file ? file->size : 0;
}

bool linklet_mmap_resize(LinkletMmapFile *file, const uint64_t new_size, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!file) {
        set_error(error, "file is required");
        return false;
    }
    if (new_size > (uint64_t)INT64_MAX) {
        set_error(error, "file size exceeds platform limit");
        return false;
    }
    if (ftruncate(file->fd, (off_t)new_size) != 0) {
        set_errno_error(error, "resize file");
        return false;
    }
    file->size = new_size;
    return true;
}

static bool io_at(const LinkletMmapFile *file, void *buffer, const size_t size,
                  const uint64_t offset, const bool write, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!file || (!buffer && size != 0)) {
        set_error(error, "file and buffer are required");
        return false;
    }
    size_t done = 0;
    while (done < size) {
        if (offset > (uint64_t)INT64_MAX || done > (uint64_t)INT64_MAX - offset) {
            set_error(error, "file offset exceeds platform limit");
            return false;
        }
        const off_t at = (off_t)(offset + done);
        const ssize_t result =
            write ? pwrite(file->fd, (const unsigned char *)buffer + done, size - done, at)
                  : pread(file->fd, (unsigned char *)buffer + done, size - done, at);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_errno_error(error, write ? "write file" : "read file");
            return false;
        }
        if (result == 0) {
            set_error(error, "unexpected end of file");
            return false;
        }
        done += (size_t)result;
    }
    return true;
}

bool linklet_mmap_read_at(const LinkletMmapFile *file, void *destination, const size_t size,
                          const uint64_t offset, LinkletError *error) {
    return io_at(file, destination, size, offset, false, error);
}

bool linklet_mmap_write_at(LinkletMmapFile *file, const void *source, const size_t size,
                           const uint64_t offset, LinkletError *error) {
    return io_at(file, (void *)source, size, offset, true, error);
}

bool linklet_mmap_map(LinkletMmapFile *file, const uint64_t offset, const size_t length,
                      const bool writable, LinkletMmapView *view, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!file || !view) {
        set_error(error, "file and view are required");
        return false;
    }
    if (offset > file->size || length > file->size - offset) {
        set_error(error, "map range exceeds file size");
        return false;
    }
    void *addr = mmap(NULL, length, PROT_READ | (writable ? PROT_WRITE : 0), MAP_SHARED, file->fd,
                      (off_t)offset);
    if (addr == MAP_FAILED) {
        set_errno_error(error, "mmap file");
        return false;
    }
    view->addr = addr;
    view->size = length;
    return true;
}

bool linklet_mmap_unmap(LinkletMmapView *view, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!view || !view->addr) {
        set_error(error, "mapping view is required");
        return false;
    }
    if (munmap(view->addr, view->size) != 0) {
        set_errno_error(error, "munmap file");
        return false;
    }
    view->addr = NULL;
    view->size = 0;
    return true;
}

bool linklet_mmap_sync(LinkletMmapView *view, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!view || !view->addr) {
        set_error(error, "mapping view is required");
        return false;
    }
    if (msync(view->addr, view->size, MS_SYNC) != 0) {
        set_errno_error(error, "msync file");
        return false;
    }
    return true;
}

bool linklet_mmap_flush(LinkletMmapFile *file, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!file) {
        set_error(error, "file is required");
        return false;
    }
    if (fsync(file->fd) != 0) {
        set_errno_error(error, "fsync file");
        return false;
    }
    return true;
}
