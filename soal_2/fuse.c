#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif

#define XOR_KEY 0x76
#define ENC_EXT ".enc"

static char storage_dir[PATH_MAX];

static int contains_parent_ref(const char *path) {
    return path && strstr(path, "..") != NULL;
}

static int ends_with(const char *s, const char *suffix) {
    size_t sl = strlen(s), sufl = strlen(suffix);
    return sl >= sufl && strcmp(s + sl - sufl, suffix) == 0;
}

static int join_raw(char *out, size_t outsz, const char *path) {
    if (!out || !path) return -EINVAL;
    if (contains_parent_ref(path)) return -EACCES;
    int n = snprintf(out, outsz, "%s%s", storage_dir, path);
    if (n < 0 || (size_t)n >= outsz) return -ENAMETOOLONG;
    return 0;
}

static int join_enc(char *out, size_t outsz, const char *path) {
    if (!out || !path) return -EINVAL;
    if (contains_parent_ref(path)) return -EACCES;
    int n = snprintf(out, outsz, "%s%s%s", storage_dir, path, ENC_EXT);
    if (n < 0 || (size_t)n >= outsz) return -ENAMETOOLONG;
    return 0;
}

static int resolve_path(char *out, size_t outsz, const char *path, struct stat *st) {
    int ret = join_raw(out, outsz, path);
    if (ret < 0) return ret;
    if (lstat(out, st) == 0) return 0;

    int raw_errno = errno;
    ret = join_enc(out, outsz, path);
    if (ret < 0) return ret;
    if (lstat(out, st) == 0) return 0;

    return -raw_errno;
}

static void xor_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) buf[i] ^= XOR_KEY;
}

static int enc_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(*stbuf));

    if (strcmp(path, "/") == 0) {
        if (lstat(storage_dir, stbuf) == -1) return -errno;
        return 0;
    }

    char real[PATH_MAX];
    return resolve_path(real, sizeof(real), path, stbuf);
}

static int enc_access(const char *path, int mask) {
    if (strcmp(path, "/") == 0) {
        return access(storage_dir, mask) == -1 ? -errno : 0;
    }

    struct stat st;
    char real[PATH_MAX];
    int ret = resolve_path(real, sizeof(real), path, &st);
    if (ret < 0) return ret;
    if (access(real, mask) == -1) return -errno;
    return 0;
}

static int enc_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
    (void)flags;
    (void)offset;
    (void)fi;

    char dirpath[PATH_MAX];
    int ret = join_raw(dirpath, sizeof(dirpath), path);
    if (ret < 0) return ret;

    DIR *dp = opendir(dirpath);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        char name[NAME_MAX + 1];
        strncpy(name, de->d_name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';

        char child[PATH_MAX];
        int n;
        if (strcmp(path, "/") == 0) n = snprintf(child, sizeof(child), "%s/%s", storage_dir, de->d_name);
        else n = snprintf(child, sizeof(child), "%s%s/%s", storage_dir, path, de->d_name);
        if (n < 0 || (size_t)n >= sizeof(child)) continue;

        struct stat st;
        memset(&st, 0, sizeof(st));
        if (lstat(child, &st) == -1) continue;

        if (S_ISREG(st.st_mode) && ends_with(name, ENC_EXT)) {
            name[strlen(name) - strlen(ENC_EXT)] = '\0';
        }
        if (filler(buf, name, &st, 0, 0)) break;
    }

    closedir(dp);
    return 0;
}

static int enc_mkdir(const char *path, mode_t mode) {
    char real[PATH_MAX];
    int ret = join_raw(real, sizeof(real), path);
    if (ret < 0) return ret;
    if (mkdir(real, mode) == -1) return -errno;
    return 0;
}

static int enc_rmdir(const char *path) {
    char real[PATH_MAX];
    int ret = join_raw(real, sizeof(real), path);
    if (ret < 0) return ret;
    if (rmdir(real) == -1) return -errno;
    return 0;
}

static int enc_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real[PATH_MAX];
    int ret = join_enc(real, sizeof(real), path);
    if (ret < 0) return ret;

    int fd = open(real, (fi->flags | O_CREAT), mode);
    if (fd == -1) return -errno;
    fi->fh = (uint64_t)fd;
    return 0;
}

static int enc_open(const char *path, struct fuse_file_info *fi) {
    char real[PATH_MAX];
    int ret = join_enc(real, sizeof(real), path);
    if (ret < 0) return ret;

    int fd = open(real, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = (uint64_t)fd;
    return 0;
}

static int enc_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void)path;
    if (!fi || !fi->fh) return -EBADF;
    int fd = (int)fi->fh;

    ssize_t res = pread(fd, buf, size, offset);
    if (res == -1) return -errno;
    xor_buffer(buf, (size_t)res);
    return (int)res;
}

static int enc_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    (void)path;
    if (!fi || !fi->fh) return -EBADF;
    int fd = (int)fi->fh;

    char *tmp = malloc(size ? size : 1);
    if (!tmp) return -ENOMEM;
    memcpy(tmp, buf, size);
    xor_buffer(tmp, size);

    ssize_t res = pwrite(fd, tmp, size, offset);
    int saved = errno;
    free(tmp);
    if (res == -1) return -saved;
    return (int)res;
}

static int enc_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void)fi;
    char real[PATH_MAX];
    int ret = join_enc(real, sizeof(real), path);
    if (ret < 0) return ret;
    if (truncate(real, size) == -1) return -errno;
    return 0;
}

static int enc_unlink(const char *path) {
    char real[PATH_MAX];
    int ret = join_enc(real, sizeof(real), path);
    if (ret < 0) return ret;
    if (unlink(real) == -1) return -errno;
    return 0;
}

static int enc_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void)fi;
    char real[PATH_MAX];
    struct stat st;
    int ret = resolve_path(real, sizeof(real), path, &st);
    if (ret < 0) return ret;
    if (utimensat(AT_FDCWD, real, tv, 0) == -1) return -errno;
    return 0;
}

static int enc_release(const char *path, struct fuse_file_info *fi) {
    (void)path;
    if (fi && fi->fh) {
        close((int)fi->fh);
        fi->fh = 0;
    }
    return 0;
}

static struct fuse_operations enc_ops = {
    .getattr = enc_getattr,
    .readdir = enc_readdir,
    .mkdir = enc_mkdir,
    .rmdir = enc_rmdir,
    .create = enc_create,
    .open = enc_open,
    .read = enc_read,
    .write = enc_write,
    .truncate = enc_truncate,
    .unlink = enc_unlink,
    .access = enc_access,
    .utimens = enc_utimens,
    .release = enc_release,
};

static int ensure_dir(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR;
        return -1;
    }
    return mkdir(dir, 0755);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [encrypted_storage] <fuse_mount> [FUSE options]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *storage_arg;
    int mount_index;
    if (argc >= 3 && argv[1][0] != '-') {
        storage_arg = argv[1];
        mount_index = 2;
    } else {
        storage_arg = "encrypted_storage";
        mount_index = 1;
    }

    if (ensure_dir(storage_arg) == -1) {
        perror("encrypted_storage");
        return EXIT_FAILURE;
    }

    char resolved[PATH_MAX];
    if (!realpath(storage_arg, resolved)) {
        perror("realpath encrypted_storage");
        return EXIT_FAILURE;
    }
    strncpy(storage_dir, resolved, sizeof(storage_dir) - 1);
    storage_dir[sizeof(storage_dir) - 1] = '\0';

    char **fuse_argv = calloc((size_t)argc + 1, sizeof(char *));
    if (!fuse_argv) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    int fuse_argc = 0;
    fuse_argv[fuse_argc++] = argv[0];
    for (int i = mount_index; i < argc; i++) fuse_argv[fuse_argc++] = argv[i];

    int ret = fuse_main(fuse_argc, fuse_argv, &enc_ops, NULL);
    free(fuse_argv);
    return ret;
}
