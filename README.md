# SISOP-4-2026-IT-124

* **Nama**  : Ndaru Satria Tama
* **NRP**   : 5027251124
* **Kelas** : C

---

## Struktur Repository

```text
.
├── README.md
├── soal_1
│   └── kenz_rescue.c
├── soal_2
│   ├── client.c
│   ├── Dockerfile
│   ├── encrypted_storage
│   ├── fuse.c
│   ├── fuse_mount
│   └── server
└── soal_3
    ├── data
    │   ├── docs
    │   ├── ebooks
    │   ├── papers
    │   └── sourcecode
    ├── docker-compose.yml
    ├── Dockerfile
    ├── entrypoint.sh
    ├── logs
    │   └── libraryit.log
    └── smb.conf
```

---

# Pendahuluan

Pada modul ini saya mengerjakan tiga soal yang berhubungan dengan konsep Sistem Operasi, khususnya penggunaan **FUSE**, **file system virtual**, **enkripsi sederhana**, **Docker**, dan **Samba file sharing**. Setiap soal memiliki fokus yang berbeda, tetapi secara umum semuanya berkaitan dengan bagaimana sistem operasi mengelola file, proses, service, permission, dan komunikasi antarprogram.

Secara garis besar:

* **Soal 1** membahas pembuatan file system virtual menggunakan FUSE untuk menampilkan file khusus `tujuan.txt`.
* **Soal 2** membahas file system berbasis FUSE yang menyimpan file dalam bentuk terenkripsi menggunakan XOR dan menghubungkannya dengan server database melalui Docker.
* **Soal 3** membahas pembuatan server file sharing menggunakan Samba di Docker, pengaturan permission user, persistence data, dan sistem logging.

Dengan mengerjakan modul ini, saya dapat memahami bagaimana FUSE dapat digunakan untuk membuat tampilan file system yang berbeda dari penyimpanan aslinya, bagaimana file dapat dienkripsi saat disimpan, serta bagaimana Samba dapat dikonfigurasi untuk membatasi hak akses berdasarkan user.

---

# Requirement Umum

Sebelum menjalankan program, beberapa package yang dibutuhkan dapat diinstall dengan command berikut.

```bash
sudo apt update
sudo apt install -y gcc pkg-config libfuse3-dev fuse3 tree docker.io smbclient cifs-utils python3 curl zip unzip
sudo systemctl enable --now docker
sudo modprobe fuse
```

Untuk FUSE yang memakai opsi `allow_other`, pastikan konfigurasi berikut aktif.

```bash
sudo sed -i 's/^#user_allow_other/user_allow_other/' /etc/fuse.conf
grep -q '^user_allow_other' /etc/fuse.conf || echo 'user_allow_other' | sudo tee -a /etc/fuse.conf
```

---

# Reporting Soal 1

## A. Deskripsi Soal

Pada soal pertama, dibuat program C bernama `kenz_rescue.c` menggunakan FUSE. Program ini melakukan mount terhadap folder sumber berisi file perjalanan Mas Amba, yaitu `1.txt` sampai `7.txt`.

Di dalam mount point, seluruh file asli tetap dapat dibaca. Selain itu, program juga menambahkan file virtual bernama `tujuan.txt`. File ini tidak benar-benar ada di folder sumber, tetapi muncul di hasil mount. Isi dari `tujuan.txt` dibuat dengan membaca seluruh baris yang diawali dengan `KOORD:` dari file `1.txt` sampai `7.txt`, lalu menggabungkannya menjadi tujuan akhir Mas Amba.

Output akhir yang diharapkan dari file virtual tersebut adalah:

```text
Tujuan Mas Amba: -7.957382728443728,112.4698688227961,23:59 WIB
```

---

## B. File yang Digunakan

* `soal_1/kenz_rescue.c`
* folder input runtime `amba_files/`
* mount point runtime `mnt/`

Pada repository final, folder `amba_files` dan `mnt` tidak ikut dikumpulkan karena keduanya merupakan folder runtime/testing.

---

## C. Kode Lengkap `kenz_rescue.c`

```c
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

```

---

## D. Cara Menjalankan dan Hasil per Tahap

### 1. Masuk ke folder soal 1

```bash
cd soal_1
```

### 2. Siapkan folder input dan mount point

```bash
mkdir -p amba_files
mkdir -p mnt
```

Folder `amba_files` diisi dengan file:

```text
1.txt
2.txt
3.txt
4.txt
5.txt
6.txt
7.txt
```

Masing-masing file harus memiliki baris yang diawali dengan `KOORD:`.

### 3. Compile program

```bash
gcc kenz_rescue.c $(pkg-config fuse3 --cflags --libs) -o kenz_rescue
```

Hasil yang diharapkan:

* file executable `kenz_rescue` berhasil dibuat
* tidak ada error saat proses compile

### 4. Jalankan FUSE

```bash
./kenz_rescue amba_files mnt -f
```

Terminal akan terlihat diam karena FUSE berjalan di foreground. Hal tersebut normal.

### 5. Buka terminal baru dan cek isi mount point

```bash
cd soal_1
ls mnt
```

Hasil yang diharapkan:

```text
1.txt
2.txt
3.txt
4.txt
5.txt
6.txt
7.txt
tujuan.txt
```

### 6. Cek isi `tujuan.txt`

```bash
cat mnt/tujuan.txt
```

Hasil yang diharapkan:

```text
Tujuan Mas Amba: -7.957382728443728,112.4698688227961,23:59 WIB
```

### 7. Cek bahwa file virtual tidak ada di folder asli

```bash
ls amba_files
```

Hasil yang diharapkan:

```text
1.txt
2.txt
3.txt
4.txt
5.txt
6.txt
7.txt
```

File `tujuan.txt` hanya muncul di mount point, bukan di folder asli.

### 8. Unmount FUSE

```bash
fusermount3 -u mnt
```

---

## E. Penjelasan Kode per Bagian

### 1. Konfigurasi FUSE

```c
#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
```

Bagian ini digunakan agar program kompatibel dengan FUSE versi 3 dan dapat menangani operasi file dengan ukuran offset yang sesuai.

### 2. File virtual `tujuan.txt`

```c
#define VIRTUAL_FILE "tujuan.txt"
#define KOORD_PREFIX "KOORD:"
#define OUT_PREFIX "Tujuan Mas Amba: "
```

Tiga konstanta ini digunakan untuk menentukan nama file virtual, prefix baris koordinat yang dicari, dan format awal output.

### 3. Fungsi `build_tujuan()`

Fungsi ini merupakan inti dari soal pertama. Program membuka file `1.txt` sampai `7.txt`, mencari baris yang diawali dengan `KOORD:`, membersihkan spasi dan newline, lalu menggabungkan potongan koordinat tersebut menjadi satu string.

### 4. Fungsi `kenz_readdir()`

Fungsi ini digunakan saat user menjalankan `ls` pada mount point. Program akan menampilkan isi folder asli, lalu menambahkan `tujuan.txt` sebagai file virtual.

### 5. Fungsi `kenz_getattr()`

Fungsi ini digunakan untuk memberikan atribut file. Jika path yang diminta adalah `/tujuan.txt`, maka program menghitung ukuran isi virtual file dan menandainya sebagai file read-only.

### 6. Fungsi `kenz_read()`

Fungsi ini menangani pembacaan file. Jika file yang dibaca adalah `tujuan.txt`, maka isi file dibuat secara dinamis dari hasil `build_tujuan()`. Jika file biasa yang dibaca, maka program membaca file asli dari folder sumber.

---

## F. Hasil Akhir Soal 1

Struktur final yang dikumpulkan untuk soal 1 adalah:

```text
soal_1/
└── kenz_rescue.c
```

---

# Reporting Soal 2

## A. Deskripsi Soal

Pada soal kedua, dibuat file system menggunakan FUSE yang menyimpan data ke folder `encrypted_storage`. Data yang dilihat user melalui folder `fuse_mount` tampil seperti file normal, tetapi data asli yang tersimpan di backend memiliki ekstensi `.enc` dan dienkripsi menggunakan XOR key `0x76`.

Selain itu, soal ini juga menggunakan server database yang dijalankan melalui Docker. Folder hasil mount FUSE dihubungkan ke container sebagai `/app/db`, sehingga server dapat membaca data yang sudah didekripsi oleh FUSE.

Secara konsep:

* user membaca/menulis file melalui `fuse_mount`
* file asli disimpan di `encrypted_storage`
* setiap file biasa disimpan dengan ekstensi `.enc`
* isi file di backend dienkripsi menggunakan XOR
* Docker menjalankan binary `server`
* client C digunakan untuk terhubung ke server melalui port `9000`

---

## B. File yang Digunakan

* `soal_2/fuse.c`
* `soal_2/client.c`
* `soal_2/Dockerfile`
* `soal_2/server`
* `soal_2/encrypted_storage/`
* `soal_2/fuse_mount/`

---

## C. Kode Lengkap `fuse.c`

```c
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

```

---

## D. Kode Lengkap `client.c`

```c
#define _POSIX_C_SOURCE 200112L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "9000"
#define BUFFER_SIZE 4096

static int send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(host, port, &hints, &res);
    if (gai != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
        return -1;
    }

    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

static void read_response(int fd) {
    char buf[BUFFER_SIZE + 1];
    int got_any = 0;

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval tv;
        tv.tv_sec = got_any ? 0 : 2;
        tv.tv_usec = got_any ? 250000 : 0;

        int ready = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            return;
        }
        if (ready == 0) break;

        ssize_t n = recv(fd, buf, BUFFER_SIZE, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            return;
        }
        if (n == 0) {
            printf("\n[server closed connection]\n");
            exit(EXIT_SUCCESS);
        }
        buf[n] = '\0';
        fputs(buf, stdout);
        got_any = 1;
    }

    if (!got_any) printf("[no response]\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    const char *host = (argc > 1) ? argv[1] : DEFAULT_HOST;
    const char *port = (argc > 2) ? argv[2] : DEFAULT_PORT;

    int fd = connect_to_server(host, port);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%s\n", host, port);
        return EXIT_FAILURE;
    }

    printf("Connected to DB Server on %s:%s\n", host, port);
    printf("Type EXIT to quit.\n");

    char line[BUFFER_SIZE];
    while (1) {
        printf("db> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';

        if (strcmp(line, "EXIT") == 0 || strcmp(line, "exit") == 0 ||
            strcmp(line, "QUIT") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        if (line[0] == '\0') continue;

        char request[BUFFER_SIZE + 2];
        int n = snprintf(request, sizeof(request), "%s\n", line);
        if (n < 0 || (size_t)n >= sizeof(request)) {
            fprintf(stderr, "Command too long\n");
            continue;
        }

        if (send_all(fd, request, (size_t)n) == -1) {
            perror("send");
            close(fd);
            return EXIT_FAILURE;
        }
        read_response(fd);
    }

    close(fd);
    return EXIT_SUCCESS;
}

```

---

## E. Isi `Dockerfile`

```dockerfile
FROM ubuntu:latest

WORKDIR /app
COPY server /app/server
RUN chmod +x /app/server && mkdir -p /app/db

EXPOSE 9000
CMD ["/app/server"]

```

---

## F. Cara Menjalankan dan Hasil per Tahap

### 1. Masuk ke folder soal 2

```bash
cd soal_2
```

### 2. Siapkan folder penyimpanan dan mount point

```bash
mkdir -p encrypted_storage
mkdir -p fuse_mount
```

### 3. Compile program FUSE dan client

```bash
gcc fuse.c $(pkg-config fuse3 --cflags --libs) -o fuse
gcc client.c -o client
chmod +x server
```

Hasil yang diharapkan:

* executable `fuse` berhasil dibuat
* executable `client` berhasil dibuat
* binary `server` memiliki permission executable

### 4. Jalankan FUSE

```bash
./fuse encrypted_storage fuse_mount -o allow_other -f
```

Terminal akan diam karena FUSE berjalan di foreground.

### 5. Buka terminal baru dan uji tulis file

```bash
cd soal_2
echo "halo database" > fuse_mount/test.txt
cat fuse_mount/test.txt
ls encrypted_storage
```

Hasil yang diharapkan saat melihat dari `fuse_mount`:

```text
halo database
```

Sedangkan di `encrypted_storage`, file akan tersimpan sebagai:

```text
test.txt.enc
```

### 6. Cek isi terenkripsi di backend

```bash
xxd encrypted_storage/test.txt.enc
```

Hasil yang diharapkan:

* isi file tidak terbaca sebagai teks normal karena sudah melalui proses XOR

### 7. Build Docker image

```bash
sudo docker build -t soal-2-modul-4-sisop .
```

### 8. Jalankan container server

```bash
sudo docker run -d \
  --name db_app \
  -p 9000:9000 \
  -v "$(pwd)/fuse_mount:/app/db" \
  soal-2-modul-4-sisop
```

### 9. Cek container

```bash
sudo docker ps
sudo docker logs db_app
```

Hasil yang diharapkan:

* container `db_app` berjalan
* server siap menerima koneksi pada port `9000`

### 10. Jalankan client

```bash
./client 127.0.0.1 9000
```

Hasil awal yang diharapkan:

```text
Connected to DB Server on 127.0.0.1:9000
Type EXIT to quit.
db>
```

Untuk keluar dari client:

```text
EXIT
```

### 11. Stop service

```bash
sudo docker rm -f db_app
fusermount3 -u fuse_mount
```

---

## G. Penjelasan Kode per Bagian

### 1. XOR key dan ekstensi file

```c
#define XOR_KEY 0x76
#define ENC_EXT ".enc"
```

`XOR_KEY` adalah kunci enkripsi sederhana yang digunakan untuk mengenkripsi dan mendekripsi isi file. `ENC_EXT` digunakan agar file backend tersimpan dengan ekstensi `.enc`.

### 2. Fungsi `xor_buffer()`

Fungsi ini melakukan operasi XOR terhadap setiap byte file. Karena XOR bersifat reversible, fungsi yang sama dapat dipakai untuk proses enkripsi dan dekripsi.

### 3. Fungsi `enc_readdir()`

Fungsi ini mengatur tampilan saat user menjalankan `ls` pada `fuse_mount`. File yang di backend bernama `test.txt.enc` akan ditampilkan sebagai `test.txt`.

### 4. Fungsi `enc_create()`

Saat user membuat file melalui `fuse_mount`, program akan membuat file backend dengan tambahan ekstensi `.enc`.

### 5. Fungsi `enc_read()`

Saat file dibaca dari `fuse_mount`, program membaca file terenkripsi dari backend lalu melakukan XOR agar isi yang tampil kepada user kembali normal.

### 6. Fungsi `enc_write()`

Saat user menulis file melalui `fuse_mount`, isi file dienkripsi terlebih dahulu menggunakan XOR, kemudian disimpan ke backend sebagai file `.enc`.

### 7. Program `client.c`

`client.c` digunakan untuk membuat koneksi TCP ke server. Program menerima input dari user, mengirim command ke server, lalu menampilkan response yang dikirim server.

### 8. Dockerfile

Dockerfile soal 2 membuat image berbasis Ubuntu, menyalin binary `server` ke `/app/server`, membuat folder `/app/db`, membuka port `9000`, dan menjalankan server.

---

## H. Hasil Akhir Soal 2

Struktur final yang dikumpulkan untuk soal 2 adalah:

```text
soal_2/
├── client.c
├── Dockerfile
├── encrypted_storage
├── fuse.c
├── fuse_mount
└── server
```

---

# Reporting Soal 3

## A. Deskripsi Soal

Pada soal ketiga, dibuat sistem file sharing bernama **LibraryIT** menggunakan Samba di dalam Docker. Sistem ini memiliki beberapa share, yaitu `docs`, `ebooks`, `papers`, dan `sourcecode`.

Setiap user memiliki hak akses berbeda:

* `member` hanya dapat membaca share tertentu.
* `contributor` dapat menulis ke `ebooks`, `papers`, dan `sourcecode`, tetapi tidak dapat menulis ke `docs`.
* `librarian` dapat menulis ke `docs` serta termasuk user staff.
* anonymous access tidak diperbolehkan.
* `sourcecode` tidak dapat diakses oleh `member`.

Data pada Samba disimpan ke folder host `data/`, sehingga file tetap ada meskipun container dimatikan. Selain itu, terdapat container logger yang membaca audit log Samba lalu menulis hasilnya ke `logs/libraryit.log` dengan format tertentu.

---

## B. File yang Digunakan

* `soal_3/Dockerfile`
* `soal_3/docker-compose.yml`
* `soal_3/smb.conf`
* `soal_3/entrypoint.sh`
* `soal_3/data/`
* `soal_3/logs/libraryit.log`

---

## C. Isi `Dockerfile`

```dockerfile
FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    samba \
    samba-common-bin \
    samba-vfs-modules \
    rsyslog \
    python3 \
    && rm -rf /var/lib/apt/lists/*

COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh

EXPOSE 445

ENTRYPOINT ["/entrypoint.sh"]

```

---

## D. Isi `docker-compose.yml`

```yaml
services:
  libraryit-server:
    build: .
    container_name: libraryit-server
    ports:
      - "445:445"
    volumes:
      - ./data:/libraryit
      - ./logs:/var/log/libraryit
    command: server
    restart: unless-stopped

  libraryit-logger:
    build: .
    container_name: libraryit-logger
    volumes:
      - ./logs:/var/log/libraryit
    command: logger
    depends_on:
      - libraryit-server
    restart: unless-stopped

```

---

## E. Isi `smb.conf`

```ini
[global]
   workgroup = WORKGROUP
   server string = LibraryIT Server
   server role = standalone server
   security = user

   passdb backend = smbpasswd
   smb passwd file = /etc/samba/smbpasswd

   map to guest = Never
   guest ok = no
   usershare allow guests = no

   server min protocol = SMB2
   client min protocol = SMB2
   ntlm auth = yes

   smb ports = 445
   disable netbios = yes

   load printers = no
   printing = bsd
   printcap name = /dev/null
   disable spoolss = yes

   access based share enum = yes
   hide unreadable = yes

   logging = syslog@1 file@0
   log level = 0

[docs]
   path = /libraryit/docs
   browseable = yes
   read only = yes
   valid users = member contributor librarian
   write list = librarian
   create mask = 0664
   directory mask = 0775
   vfs objects = full_audit
   full_audit:prefix = %u|%S
   full_audit:success = all
   full_audit:failure = all
   full_audit:facility = LOCAL7
   full_audit:priority = NOTICE

[ebooks]
   path = /libraryit/ebooks
   browseable = yes
   read only = yes
   valid users = @readonly @staff
   write list = @staff
   create mask = 0664
   directory mask = 0775
   vfs objects = full_audit
   full_audit:prefix = %u|%S
   full_audit:success = all
   full_audit:failure = all
   full_audit:facility = LOCAL7
   full_audit:priority = NOTICE

[papers]
   path = /libraryit/papers
   browseable = yes
   read only = yes
   valid users = @readonly @staff
   write list = @staff
   create mask = 0664
   directory mask = 0775
   vfs objects = full_audit
   full_audit:prefix = %u|%S
   full_audit:success = all
   full_audit:failure = all
   full_audit:facility = LOCAL7
   full_audit:priority = NOTICE

[sourcecode]
   path = /libraryit/sourcecode
   browseable = yes
   read only = no
   valid users = @staff
   create mask = 0664
   directory mask = 0770
   vfs objects = full_audit
   full_audit:prefix = %u|%S
   full_audit:success = all
   full_audit:failure = all
   full_audit:facility = LOCAL7
   full_audit:priority = NOTICE

```

---

## F. Isi `entrypoint.sh`

```bash
#!/bin/bash
set -e

MODE="${1:-server}"

LOG_DIR="/var/log/libraryit"
RAW_LOG="$LOG_DIR/audit_raw.log"
FINAL_LOG="$LOG_DIR/libraryit.log"

mkdir -p "$LOG_DIR"
touch "$RAW_LOG" "$FINAL_LOG"
chmod 666 "$RAW_LOG" "$FINAL_LOG"

if [ "$MODE" = "logger" ]; then
    tail -n +1 -F "$RAW_LOG" | while read -r line; do
        case "$line" in
            *"smbd_audit:"*)
                payload="${line#*smbd_audit: }"

                username="$(echo "$payload" | cut -d'|' -f1)"
                share="$(echo "$payload" | cut -d'|' -f2)"
                action="$(echo "$payload" | cut -d'|' -f3)"
                result="$(echo "$payload" | cut -d'|' -f4)"
                target="$(echo "$payload" | cut -d'|' -f5-)"

                [ -z "$username" ] && username="-"
                [ -z "$share" ] && share="-"
                [ -z "$action" ] && action="-"
                [ -z "$target" ] && target="$share"

                upper_action="$(echo "$action" | tr '[:lower:]' '[:upper:]')"

                if echo "$result" | grep -qi "fail"; then
                    level="WARNING"
                    upper_action="DENIED"
                else
                    level="INFO"
                fi

                timestamp="$(date '+%Y-%m-%d %H:%M:%S')"
                formatted="[$timestamp] [$level] [$username] [$upper_action] [$target]"

                echo "$formatted" | tee -a "$FINAL_LOG"
                ;;
        esac
    done
    exit 0
fi

groupadd -f readonly
groupadd -f staff

id member >/dev/null 2>&1 || useradd -M -s /usr/sbin/nologin -g readonly member
id contributor >/dev/null 2>&1 || useradd -M -s /usr/sbin/nologin -g staff contributor
id librarian >/dev/null 2>&1 || useradd -M -s /usr/sbin/nologin -g staff librarian

rm -f /etc/samba/smbpasswd
touch /etc/samba/smbpasswd
chmod 600 /etc/samba/smbpasswd

printf "member123\nmember123\n" | smbpasswd -s -a member
printf "contrib456\ncontrib456\n" | smbpasswd -s -a contributor
printf "lib789\nlib789\n" | smbpasswd -s -a librarian

smbpasswd -e member
smbpasswd -e contributor
smbpasswd -e librarian

mkdir -p /libraryit/docs /libraryit/ebooks /libraryit/papers /libraryit/sourcecode

chown -R librarian:staff /libraryit/docs
chmod -R 775 /libraryit/docs

chown -R root:staff /libraryit/ebooks
chmod -R 2775 /libraryit/ebooks

chown -R root:staff /libraryit/papers
chmod -R 2775 /libraryit/papers

chown -R root:staff /libraryit/sourcecode
chmod -R 2770 /libraryit/sourcecode

cat > /etc/rsyslog.conf <<RSYSLOG
module(load="imuxsock")
local7.*    $RAW_LOG
*.*         /dev/null
RSYSLOG

rm -f /run/rsyslogd.pid
rsyslogd

logger -p local7.notice "smbd_audit: system|startup|connect|ok|libraryit"

testparm -s /etc/samba/smb.conf

exec smbd --foreground --no-process-group --debug-stdout

```

---

## G. Cara Menjalankan dan Hasil per Tahap

### 1. Masuk ke folder soal 3

```bash
cd soal_3
```

### 2. Siapkan folder dan permission

```bash
chmod +x entrypoint.sh
mkdir -p data/docs data/ebooks data/papers data/sourcecode logs
touch logs/libraryit.log
```

### 3. Matikan service Samba bawaan host jika ada

```bash
sudo systemctl stop smbd nmbd 2>/dev/null || true
```

Langkah ini dilakukan agar port `445` tidak bentrok dengan container.

### 4. Build dan jalankan Docker Compose

```bash
sudo docker compose down --remove-orphans 2>/dev/null || true
sudo docker compose build --no-cache
sudo docker compose up -d
sleep 5
sudo docker ps
```

Hasil yang diharapkan:

```text
libraryit-server
libraryit-logger
```

### 5. Tes daftar share sebagai `member`

```bash
smbclient -L //127.0.0.1 -U member --password='member123' -m SMB3
```

Hasil yang diharapkan:

```text
docs
ebooks
papers
IPC$
```

Share `sourcecode` tidak muncul untuk `member` karena hak aksesnya dibatasi untuk staff.

Pesan seperti berikut dapat diabaikan karena hanya fallback SMB1 dari `smbclient`.

```text
Reconnect with SMB1 for workgroup listing
Unable to connect with SMB1
```

### 6. Tes anonymous access

```bash
smbclient -L //127.0.0.1 -N -m SMB3
```

Hasil yang diharapkan:

* koneksi anonymous gagal

### 7. Tes akses baca `docs` oleh member

```bash
smbclient //127.0.0.1/docs -U member --password='member123' -m SMB3 -c "ls"
```

Hasil yang diharapkan:

* user `member` dapat melihat isi folder `docs`

### 8. Tes upload ke `ebooks` oleh contributor

```bash
echo "test ebook" > ~/test_ebook.txt
smbclient //127.0.0.1/ebooks -U contributor --password='contrib456' -m SMB3 -c "put $HOME/test_ebook.txt test_ebook.txt; ls"
```

Hasil yang diharapkan:

* upload berhasil
* file muncul di share `ebooks`

### 9. Tes upload ke `papers` oleh contributor

```bash
echo "test paper" > ~/test_paper.txt
smbclient //127.0.0.1/papers -U contributor --password='contrib456' -m SMB3 -c "put $HOME/test_paper.txt test_paper.txt; ls"
```

Hasil yang diharapkan:

* upload berhasil
* file muncul di share `papers`

### 10. Tes upload ke `docs` oleh librarian

```bash
echo "dokumen librarian" > ~/test_docs.txt
smbclient //127.0.0.1/docs -U librarian --password='lib789' -m SMB3 -c "put $HOME/test_docs.txt test_docs.txt; ls"
```

Hasil yang diharapkan:

* upload berhasil karena `librarian` masuk write list untuk `docs`

### 11. Tes contributor menulis ke `docs`

```bash
echo "dokumen contributor" > ~/contributor_docs.txt
smbclient //127.0.0.1/docs -U contributor --password='contrib456' -m SMB3 -c "put $HOME/contributor_docs.txt contributor_docs.txt; ls"
```

Hasil yang diharapkan:

* upload gagal atau access denied karena `docs` hanya dapat ditulis oleh `librarian`

### 12. Tes member mengakses `sourcecode`

```bash
smbclient //127.0.0.1/sourcecode -U member --password='member123' -m SMB3 -c "ls"
```

Hasil yang diharapkan:

```text
NT_STATUS_ACCESS_DENIED
```

### 13. Tes contributor menulis ke `sourcecode`

```bash
echo "print('hello sourcecode')" > ~/main.py
smbclient //127.0.0.1/sourcecode -U contributor --password='contrib456' -m SMB3 -c "put $HOME/main.py main.py; ls"
```

Hasil yang diharapkan:

* upload berhasil karena `contributor` termasuk group staff

### 14. Cek persistence data

```bash
sudo find data -type f
```

Hasil yang diharapkan setelah testing:

```text
data/ebooks/test_ebook.txt
data/papers/test_paper.txt
data/docs/test_docs.txt
data/sourcecode/main.py
```

Artinya file tersimpan di host melalui volume `./data:/libraryit`.

### 15. Cek log

```bash
cat logs/libraryit.log | tail -30
sudo docker logs libraryit-logger --tail=30
```

Format yang diharapkan:

```text
[YYYY-MM-DD HH:MM:SS] [INFO] [username] [AKSI] [file/share]
[YYYY-MM-DD HH:MM:SS] [WARNING] [username] [DENIED] [file/share]
```

Contoh output:

```text
[2026-05-13 11:56:36] [INFO] [contributor] [CLOSE] [/libraryit/ebooks]
[2026-05-13 11:56:36] [WARNING] [member] [DENIED] [/libraryit/sourcecode]
```

### 16. Stop container

```bash
sudo docker compose down
```

---

## H. Penjelasan Konfigurasi per Bagian

### 1. `docker-compose.yml`

File ini menjalankan dua service, yaitu `libraryit-server` dan `libraryit-logger`.

* `libraryit-server` menjalankan Samba dan membuka port `445`.
* `libraryit-logger` membaca log audit dari folder `logs`.
* Folder `data` di host di-mount ke `/libraryit`.
* Folder `logs` di host di-mount ke `/var/log/libraryit`.

### 2. Dockerfile

Dockerfile menggunakan image `ubuntu:latest`, lalu menginstall Samba, modul VFS Samba, rsyslog, dan Python. Setelah itu Dockerfile menyalin `smb.conf` serta `entrypoint.sh` ke dalam container.

### 3. User dan group

Pada `entrypoint.sh`, dibuat dua group:

```text
readonly
staff
```

Lalu dibuat tiga user:

```text
member       -> group readonly
contributor  -> group staff
librarian    -> group staff
```

Password user Samba:

```text
member       : member123
contributor  : contrib456
librarian    : lib789
```

### 4. Share `docs`

Share `docs` bersifat read-only secara default, tetapi user `librarian` masuk ke `write list`. Artinya `member` dan `contributor` dapat membaca, tetapi hanya `librarian` yang dapat menulis.

### 5. Share `ebooks` dan `papers`

Share `ebooks` dan `papers` dapat dibaca oleh group `readonly` dan `staff`, tetapi hanya group `staff` yang dapat menulis. Karena itu, `contributor` dan `librarian` dapat menulis, sedangkan `member` hanya membaca.

### 6. Share `sourcecode`

Share `sourcecode` hanya dapat diakses oleh group `staff`. Karena `member` tidak termasuk group staff, maka akses `member` ke share ini akan ditolak.

### 7. Logging

Samba menggunakan modul `full_audit` untuk mencatat aktivitas file. Log mentah dikirim ke fasilitas `LOCAL7`, kemudian `rsyslog` menuliskannya ke `audit_raw.log`. Service `libraryit-logger` membaca file tersebut, memformat ulang log, dan menyimpannya ke `libraryit.log`.

Format akhirnya adalah:

```text
[YYYY-MM-DD HH:MM:SS] [LEVEL] [USERNAME] [AKSI] [NAMA FILE/SHARE]
```

---

## I. Hasil Akhir Soal 3

Struktur final yang dikumpulkan untuk soal 3 adalah:

```text
soal_3/
├── data
│   ├── docs
│   ├── ebooks
│   ├── papers
│   └── sourcecode
├── docker-compose.yml
├── Dockerfile
├── entrypoint.sh
├── logs
│   └── libraryit.log
└── smb.conf
```

---

# Cara Membersihkan Hasil Testing Sebelum Pengumpulan

Sebelum dikumpulkan, file hasil testing dapat dibersihkan dengan command berikut.

```bash
cd ~/"Modul 4"

fusermount3 -u soal_1/mnt 2>/dev/null || true
fusermount3 -u soal_2/fuse_mount 2>/dev/null || true

cd soal_3
sudo docker compose down --remove-orphans 2>/dev/null || true
cd ..

rm -rf soal_1/amba_files
rm -rf soal_1/mnt
rm -f soal_1/kenz_rescue soal_1/kenz.log

sudo docker rm -f db_app 2>/dev/null || true
rm -f soal_2/fuse soal_2/client soal_2/fuse.log
sudo rm -rf soal_2/encrypted_storage/*
sudo rm -rf soal_2/fuse_mount/*

sudo rm -rf soal_3/data/docs/*
sudo rm -rf soal_3/data/ebooks/*
sudo rm -rf soal_3/data/papers/*
sudo rm -rf soal_3/data/sourcecode/*

sudo rm -rf soal_3/logs/*
mkdir -p soal_3/logs
touch soal_3/logs/libraryit.log

sudo chown -R "$USER:$USER" soal_3/data soal_3/logs
mkdir -p soal_2/encrypted_storage soal_2/fuse_mount
```

---

# Kesimpulan

Dari ketiga soal pada modul ini, saya dapat memahami beberapa konsep penting dalam Sistem Operasi.

* **Soal 1** memperlihatkan cara membuat file virtual menggunakan FUSE dan membaca data dari beberapa file sumber.
* **Soal 2** memperlihatkan cara membuat file system terenkripsi sederhana menggunakan FUSE serta menghubungkannya dengan server melalui Docker.
* **Soal 3** memperlihatkan cara mengatur Samba file sharing di Docker, membatasi permission berdasarkan user/group, menjaga persistence data, dan membuat sistem logging.

Secara keseluruhan, seluruh soal sudah dapat dijalankan sesuai kebutuhan, mulai dari mounting FUSE, enkripsi file, komunikasi client-server, file sharing Samba, hingga pencatatan log aktivitas.
