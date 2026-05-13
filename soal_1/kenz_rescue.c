#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>

#define VIRTUAL_FILE "tujuan.txt"
#define KOORD_PREFIX "KOORD:"
#define OUT_PREFIX "Tujuan Mas Amba: "

static char source_dir[PATH_MAX];

static int join_path(char *out, size_t outsz, const char *base, const char *path) {
    if (out == NULL || base == NULL || path == NULL) return -EINVAL;
    if (strstr(path, "..") != NULL) return -EACCES;
    int n = snprintf(out, outsz, "%s%s", base, path);
    if (n < 0 || (size_t)n >= outsz) return -ENAMETOOLONG;
    return 0;
}

static void trim_left(char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
}

static void trim_right(char *s) {
    size_t len = strlen(s);
    while (len > 0) {
        char c = s[len - 1];
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t') break;
        s[--len] = '\0';
    }
}

static int append_text(char **buf, size_t *len, size_t *cap, const char *text) {
    size_t add = strlen(text);
    if (*len + add + 1 > *cap) {
        size_t newcap = (*cap == 0) ? 256 : *cap;
        while (*len + add + 1 > newcap) {
            if (newcap > ((size_t)-1) / 2) return -ENOMEM;
            newcap *= 2;
        }
        char *tmp = realloc(*buf, newcap);
        if (!tmp) return -ENOMEM;
        *buf = tmp;
        *cap = newcap;
    }
    memcpy(*buf + *len, text, add);
    *len += add;
    (*buf)[*len] = '\0';
    return 0;
}

static int build_tujuan(char **out, size_t *out_len) {
    char *fragments = NULL;
    size_t flen = 0, fcap = 0;

    for (int i = 1; i <= 7; i++) {
        char filename[PATH_MAX];
        int n = snprintf(filename, sizeof(filename), "%s/%d.txt", source_dir, i);
        if (n < 0 || (size_t)n >= sizeof(filename)) {
            free(fragments);
            return -ENAMETOOLONG;
        }

        FILE *fp = fopen(filename, "r");
        if (!fp) {
            int err = -errno;
            free(fragments);
            return err;
        }

        char line[4096];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, KOORD_PREFIX, strlen(KOORD_PREFIX)) == 0) {
                char *frag = line + strlen(KOORD_PREFIX);
                trim_left(&frag);
                trim_right(frag);
                int ret = append_text(&fragments, &flen, &fcap, frag);
                if (ret < 0) {
                    fclose(fp);
                    free(fragments);
                    return ret;
                }
            }
        }

        if (ferror(fp)) {
            fclose(fp);
            free(fragments);
            return -EIO;
        }
        fclose(fp);
    }

    size_t total = strlen(OUT_PREFIX) + flen + 1;
    char *result = malloc(total + 1);
    if (!result) {
        free(fragments);
        return -ENOMEM;
    }

    snprintf(result, total + 1, "%s%s\n", OUT_PREFIX, fragments ? fragments : "");
    free(fragments);
    *out = result;
    *out_len = strlen(result);
    return 0;
}

static int is_tujuan(const char *path) {
    return strcmp(path, "/" VIRTUAL_FILE) == 0;
}

static int kenz_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(*stbuf));

    if (is_tujuan(path)) {
        char *content = NULL;
        size_t len = 0;
        int ret = build_tujuan(&content, &len);
        if (ret < 0) return ret;
        free(content);
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = (off_t)len;
        return 0;
    }

    char real[PATH_MAX];
    int ret = join_path(real, sizeof(real), source_dir, path);
    if (ret < 0) return ret;
    if (lstat(real, stbuf) == -1) return -errno;
    return 0;
}

static int kenz_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    (void)flags;
    (void)offset;
    (void)fi;

    char real[PATH_MAX];
    int ret = join_path(real, sizeof(real), source_dir, path);
    if (ret < 0) return ret;

    DIR *dp = opendir(real);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }
    closedir(dp);

    if (strcmp(path, "/") == 0) {
        char *content = NULL;
        size_t len = 0;
        ret = build_tujuan(&content, &len);
        if (ret < 0) return ret;

        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        st.st_nlink = 1;
        st.st_size = (off_t)len;
        free(content);
        filler(buf, VIRTUAL_FILE, &st, 0, 0);
    }

    return 0;
}

static int kenz_open(const char *path, struct fuse_file_info *fi) {
    if (is_tujuan(path)) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
        return 0;
    }

    char real[PATH_MAX];
    int ret = join_path(real, sizeof(real), source_dir, path);
    if (ret < 0) return ret;

    int fd = open(real, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = (uint64_t)fd;
    return 0;
}

static int kenz_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    if (is_tujuan(path)) {
        char *content = NULL;
        size_t len = 0;
        int ret = build_tujuan(&content, &len);
        if (ret < 0) return ret;

        if (offset < 0) {
            free(content);
            return -EINVAL;
        }
        if ((size_t)offset < len) {
            if (offset + (off_t)size > (off_t)len) size = len - (size_t)offset;
            memcpy(buf, content + offset, size);
        } else {
            size = 0;
        }
        free(content);
        return (int)size;
    }

    int fd = (fi && fi->fh) ? (int)fi->fh : -1;
    int need_close = 0;
    if (fd < 0) {
        char real[PATH_MAX];
        int ret = join_path(real, sizeof(real), source_dir, path);
        if (ret < 0) return ret;
        fd = open(real, O_RDONLY);
        if (fd == -1) return -errno;
        need_close = 1;
    }

    ssize_t res = pread(fd, buf, size, offset);
    int saved = errno;
    if (need_close) close(fd);
    if (res == -1) return -saved;
    return (int)res;
}

static int kenz_release(const char *path, struct fuse_file_info *fi) {
    (void)path;
    if (fi && fi->fh) {
        close((int)fi->fh);
        fi->fh = 0;
    }
    return 0;
}

static struct fuse_operations kenz_ops = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open = kenz_open,
    .read = kenz_read,
    .release = kenz_release,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_directory> <mount_directory> [FUSE options]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char resolved[PATH_MAX];
    if (!realpath(argv[1], resolved)) {
        perror("source_directory");
        return EXIT_FAILURE;
    }

    struct stat st;
    if (stat(resolved, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: source_directory harus berupa direktori yang ada.\n");
        return EXIT_FAILURE;
    }

    strncpy(source_dir, resolved, sizeof(source_dir) - 1);
    source_dir[sizeof(source_dir) - 1] = '\0';

    char **fuse_argv = calloc((size_t)argc, sizeof(char *));
    if (!fuse_argv) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    int fuse_argc = 0;
    fuse_argv[fuse_argc++] = argv[0];
    for (int i = 2; i < argc; i++) fuse_argv[fuse_argc++] = argv[i];

    int ret = fuse_main(fuse_argc, fuse_argv, &kenz_ops, NULL);
    free(fuse_argv);
    return ret;
}
