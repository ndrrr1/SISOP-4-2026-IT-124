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

Catatan:

* Folder `soal_1/amba_files` tidak ada di ZIP lama karena folder tersebut adalah bahan runtime. Pada langkah run Soal 1, folder dan file `1.txt` sampai `7.txt` dibuat ulang dengan isi yang sama.
* File `notes.csv.enc` untuk Soal 2 juga dibuat ulang melalui command `base64 -d`, sehingga orang yang baru download ZIP tetap bisa mengetes hasil decrypt FUSE.
* Folder kosong seperti `encrypted_storage`, `fuse_mount`, `data`, dan `logs` tetap dibutuhkan saat program dijalankan.

---

# Pendahuluan

Pada Modul 4 Sistem Operasi ini, saya mengerjakan tiga soal yang berhubungan dengan **FUSE**, **file system virtual**, **enkripsi file sederhana**, **Docker**, **Samba file sharing**, permission user, persistence data, dan logging.

Secara garis besar:

* **Soal 1** membuat file system virtual menggunakan FUSE. Program menampilkan file tambahan bernama `tujuan.txt` yang berisi gabungan koordinat dari file `1.txt` sampai `7.txt`.
* **Soal 2** membuat file system FUSE yang menyimpan file dalam bentuk terenkripsi menggunakan XOR. Hasil mount FUSE kemudian digunakan oleh server database yang berjalan di Docker.
* **Soal 3** membuat file sharing server menggunakan Samba di Docker. Program mengatur user, group, permission folder, persistence data, dan logging aktivitas.

Modul ini memperlihatkan bahwa sistem operasi tidak hanya menjalankan program, tetapi juga mengatur bagaimana file disimpan, bagaimana hak akses diterapkan, bagaimana service berjalan di background, dan bagaimana aktivitas sistem dicatat melalui log.

---

# Cara Menjalankan Program

Bagian ini ditulis untuk kondisi ketika seseorang baru saja mendownload ZIP final. Semua langkah dimulai dari folder utama repository.

Folder utama repository adalah folder yang berisi:

```text
soal_1  soal_2  soal_3
```

## 1. Masuk ke folder hasil extract ZIP

Jika ZIP didownload dari GitHub, biasanya nama folder hasil extract adalah:

```text
SISOP-4-2026-IT-124-main
```

Contoh masuk ke folder tersebut:

```bash
cd ~/Downloads/SISOP-4-2026-IT-124-main
```

Jika nama folder berbeda, sesuaikan dengan lokasi folder hasil extract.

## 2. Pastikan sudah berada di root repository

```bash
ls
```

Output yang diharapkan:

```text
soal_1  soal_2  soal_3
```

Jika output tersebut muncul, berarti posisi terminal sudah benar.

---

# Requirement Umum

Requirement ini dijalankan satu kali sebelum menjalankan soal.

## 1. Update package list

```bash
sudo apt update
```

## 2. Install compiler dan dependency FUSE

```bash
sudo apt install -y gcc pkg-config libfuse3-dev fuse3
```

## 3. Install Docker dan tools pendukung

```bash
sudo apt install -y docker.io smbclient cifs-utils tree zip unzip curl xxd
```

## 4. Aktifkan Docker

```bash
sudo systemctl enable --now docker
```

## 5. Aktifkan module FUSE

```bash
sudo modprobe fuse
```

## 6. Aktifkan konfigurasi `allow_other`

```bash
sudo sed -i 's/^#user_allow_other/user_allow_other/' /etc/fuse.conf
```

```bash
grep -q '^user_allow_other' /etc/fuse.conf || echo 'user_allow_other' | sudo tee -a /etc/fuse.conf
```

## 7. Cek Docker

```bash
sudo docker ps
```

Jika command berhasil dan tidak muncul error permission/service, Docker sudah siap digunakan.

## 8. Jika `docker compose` belum tersedia

Pada beberapa environment, Docker Compose belum otomatis tersedia. Jika command `sudo docker compose` gagal, install Docker Compose lama dengan:

```bash
sudo apt install -y docker-compose
```

Setelah itu, command:

```bash
sudo docker compose up -d
```

dapat diganti menjadi:

```bash
sudo docker-compose up -d
```

---

# Reporting Soal 1

## A. Deskripsi Soal

Pada soal pertama, dibuat program C bernama `kenz_rescue.c` menggunakan FUSE. Program ini melakukan mount terhadap folder sumber berisi file perjalanan Mas Amba, yaitu `1.txt` sampai `7.txt`.

Di dalam mount point, seluruh file asli tetap dapat dibaca. Selain itu, program juga menambahkan file virtual bernama `tujuan.txt`. File ini tidak benar-benar ada di folder sumber, tetapi muncul di hasil mount.

Isi dari `tujuan.txt` dibuat dengan membaca seluruh baris yang diawali dengan `KOORD:` dari file `1.txt` sampai `7.txt`, kemudian menggabungkan semua potongan koordinat tersebut menjadi tujuan akhir Mas Amba.

Output akhir yang diharapkan adalah:

```text
Tujuan Mas Amba: -7.957382728443728,112.4698688227961,23:59 WIB
```

---

## B. File yang Digunakan

File utama yang dikumpulkan:

* `soal_1/kenz_rescue.c`

File dan folder runtime yang dibuat saat menjalankan:

* `soal_1/amba_files/1.txt`
* `soal_1/amba_files/2.txt`
* `soal_1/amba_files/3.txt`
* `soal_1/amba_files/4.txt`
* `soal_1/amba_files/5.txt`
* `soal_1/amba_files/6.txt`
* `soal_1/amba_files/7.txt`
* `soal_1/mnt/`
* `soal_1/kenz_rescue`
* `soal_1/fuse.log`

---

## C. Kode Lengkap `kenz_rescue.c`

<details>
<summary>Klik untuk membuka/menutup kode lengkap kenz_rescue.c</summary>

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

</details>

---

## D. Cara Menjalankan Soal 1

Semua langkah pada bagian ini dimulai dari folder utama repository.

### 1. Masuk ke folder `soal_1`

```bash
cd soal_1
```

### 2. Lepaskan mount lama jika masih ada

```bash
fusermount3 -u mnt 2>/dev/null || true
```

### 3. Hapus folder runtime lama

```bash
rm -rf amba_files
```

### 4. Hapus mount point lama

```bash
rm -rf mnt
```

### 5. Hapus file compile lama

```bash
rm -f kenz_rescue
```

### 6. Hapus log FUSE lama

```bash
rm -f fuse.log
```

### 7. Buat folder input

```bash
mkdir -p amba_files
```

### 8. Buat mount point

```bash
mkdir -p mnt
```

### 9. Buat file bahan `1.txt`

```bash
cat > amba_files/1.txt <<'EOF'
=== HARI 1 ===

Hari pertama ekspedisi pertama. Saya berangkat dari Tembok Ratapan Keputih jam 5 pagi.
Tujuan saya: Petilasan Puncak Gunung Kawi, untuk meng-update firmware Pusaka Pesugihan v2.7 milik mendiang paman.

KOORD: -7.957

Sampai nanti, paman.
-- Amba
EOF
```

### 10. Buat file bahan `2.txt`

```bash
cat > amba_files/2.txt <<'EOF'
=== HARI 2 ===

Hari kedua. Sampai Pos 1 Gunung Kawi setelah delapan jam jalan kaki.
Kabut tipis, masih bisa lihat warung madura di pinggir jalur.
Saya mampir makan gajah baru 12 batang. Pemilik warung nanya saya mau kemana. Saya jawab 'mau naik anjing'. Dia langsung paham, tidak banyak tanya.

KOORD: 382728

Sampai nanti, paman.
-- Amba
EOF
```

### 11. Buat file bahan `3.txt`

```bash
cat > amba_files/3.txt <<'EOF'
=== HARI 3 ===

Hari ketiga. Pos 2. Sinyal HP mati total.
Saya baca lagi catatan paman: 'kalau sudah di sini, jangan menengok ke belakang sampai puncak'.
Saya patuh. Tidak menengok. Walaupun ada suara "HAI 4nt3k-4nt3k 453N9" di belakang saya seharian. Pelan, sabar, mengikuti ritme jalan saya.

KOORD: 443728, 

Sampai nanti, paman.
-- Amba
EOF
```

### 12. Buat file bahan `4.txt`

```bash
cat > amba_files/4.txt <<'EOF'
=== HARI 4 ===

Hari keempat. Pos 3 - Sumber Air Sungai Kembar.
Saya isi botol sambil baca mantra v2.7. Mantranya panjang sekali, kayak log kernel waktu boot Arch Windows.
Selesai mantra, satu daun beringin jatuh tepat ke tangan saya. Pertanda baik, kata paman dulu. kaki mulai terasa hangat (atau bisa jadi gweh ngompol).

KOORD: 112.469

Sampai nanti, paman.
-- Amba
EOF
```

### 13. Buat file bahan `5.txt`

```bash
cat > amba_files/5.txt <<'EOF'
=== HARI 5 ===

Hari kelima. Trek makin berat. Kabut tebal sekali, jarak pandang dua meter.
Saya hampir tersesat dua kali. Kompas tidak stabil di sekitar Pos 5, selalu menunjuk arah yang berbeda.
Tanah bergetar pelan, semacam denyut nadi. Lokasi sudah dekat.

KOORD: 8688227961, 

Sampai nanti, paman.
-- Amba
EOF
```

### 14. Buat file bahan `6.txt`

```bash
cat > amba_files/6.txt <<'EOF'
=== HARI 6 ===

Hari keenam. Pos 6 - Pondok Tua sebelum puncak.
Saya bermalam di sini. Tidak ada penjaga, hanya angin dan bunyi gamelan sayup-sayup dari arah puncak.
Besok subuh saya naik ke petilasan. Sesuai catatan paman, pusaka harus diaktifkan tepat tengah malam berikutnya.

KOORD: 23:

Sampai nanti, paman.
-- Amba
EOF
```

### 15. Buat file bahan `7.txt`

```bash
cat > amba_files/7.txt <<'EOF'
=== HARI 7 ===

Hari ketujuh. Petilasan Puncak Kawi.
Saya gelar pusaka di altar batu. Saat lilin pertama saya nyalakan, kabut tiba-tiba membentuk sosok pria bermuka familiar, sendirian, berdiri tepat di hadapan saya. Sang Pria Solo Penjaga Puncak!!!
Dia bicara pelan: "Didatengin tanpa janjian saya diem, didatengin tanpa suguhan saya juga diem, disalahin terus klo ada pendaki ngilang saya juga diem...TAPI KALI INI SAYA AKAN LAWAN!...firmware ini cuma bisa di-update kalau kau bawa tumbal. Bukan ayam bangkok, bukan kadal sunda" Melainkan Sebiji  Asisten praktikum yang psikopat. Yang doyan kasih soal sulit. Yang rilis soal jam 00.00 dan 03.00 untuk meneror mahasigma. Cari dia. Bawa dia. Tengah malam berikutnya, di sini.'
Saya turun gunung dengan rencana baru. Saya cari nama-nama asisten Sisop. Satu nama menonjol: Asisten Kenz.

KOORD: 59 WIB

Sampai nanti, paman.
-- Amba
EOF
```

### 16. Cek file bahan

```bash
ls amba_files
```

Output yang diharapkan:

```text
1.txt  2.txt  3.txt  4.txt  5.txt  6.txt  7.txt
```

### 17. Cek salah satu isi file bahan

```bash
cat amba_files/1.txt
```

Output harus menampilkan isi perjalanan hari pertama dan memiliki baris:

```text
KOORD: -7.957
```

### 18. Compile program

```bash
gcc kenz_rescue.c $(pkg-config fuse3 --cflags --libs) -o kenz_rescue
```

Jika tidak ada error, file executable `kenz_rescue` berhasil dibuat.

### 19. Jalankan FUSE

Agar mudah dites dalam satu terminal, FUSE dijalankan di background dan output diarahkan ke `fuse.log`.

```bash
./kenz_rescue amba_files mnt -f > fuse.log 2>&1 &
```

### 20. Tunggu proses mount

```bash
sleep 2
```

### 21. Cek isi mount point

```bash
ls mnt
```

Output yang diharapkan:

```text
1.txt  2.txt  3.txt  4.txt  5.txt  6.txt  7.txt  tujuan.txt
```

### 22. Cek isi `tujuan.txt`

```bash
cat mnt/tujuan.txt
```

Output yang diharapkan:

```text
Tujuan Mas Amba: -7.957382728443728,112.4698688227961,23:59 WIB
```

### 23. Pastikan `tujuan.txt` tidak ada di folder asli

```bash
ls amba_files
```

Output yang diharapkan:

```text
1.txt  2.txt  3.txt  4.txt  5.txt  6.txt  7.txt
```

File `tujuan.txt` hanya muncul di folder `mnt`, bukan di folder `amba_files`.

### 24. Stop FUSE Soal 1

```bash
fusermount3 -u mnt
```

### 25. Kembali ke folder utama repository

```bash
cd ..
```

---

## E. Penjelasan Kode Soal 1

### 1. Konfigurasi FUSE

Program menggunakan FUSE versi 3.

```c
#define FUSE_USE_VERSION 31
```

### 2. File virtual `tujuan.txt`

Program mendefinisikan file virtual bernama `tujuan.txt`. File ini tidak dibuat secara fisik di folder sumber, melainkan hanya dimunculkan melalui FUSE.

### 3. Pembacaan koordinat

Program membaca file `1.txt` sampai `7.txt`, mencari baris yang diawali `KOORD:`, mengambil isi setelah prefix tersebut, membersihkan newline, lalu menggabungkan semuanya menjadi satu string.

### 4. Fungsi `readdir`

Fungsi ini digunakan ketika user menjalankan `ls` pada mount point. Program menampilkan file asli dari folder sumber dan menambahkan `tujuan.txt`.

### 5. Fungsi `read`

Jika file yang dibaca adalah file biasa, program membaca file asli. Jika file yang dibaca adalah `tujuan.txt`, program membuat isi file secara dinamis dari hasil gabungan koordinat.

---

## F. Hasil Akhir Soal 1

Struktur final ZIP tetap bersih:

```text
soal_1/
└── kenz_rescue.c
```

Folder `amba_files` dan `mnt` hanya dibuat saat runtime.

---

# Reporting Soal 2

## A. Deskripsi Soal

Pada soal kedua, dibuat file system menggunakan FUSE yang menyimpan data ke folder `encrypted_storage`. Data yang dilihat user melalui folder `fuse_mount` tampil seperti file normal, tetapi data asli yang tersimpan di backend memiliki ekstensi `.enc` dan terenkripsi menggunakan XOR key `0x76`.

Selain itu, soal ini menggunakan server database yang dijalankan melalui Docker. Folder hasil mount FUSE dihubungkan ke container sebagai `/app/db`, sehingga server dapat membaca file melalui layer FUSE.

Secara konsep:

* user membaca dan menulis file melalui `fuse_mount`
* file asli disimpan di `encrypted_storage`
* file backend diberi ekstensi `.enc`
* isi file backend dienkripsi menggunakan XOR key `0x76`
* Docker menjalankan binary `server`
* client C terhubung ke server melalui port `9000`

---

## B. File yang Digunakan

File utama:

* `soal_2/fuse.c`
* `soal_2/client.c`
* `soal_2/Dockerfile`
* `soal_2/server`
* `soal_2/encrypted_storage/`
* `soal_2/fuse_mount/`

File runtime yang dibuat saat testing:

* `soal_2/encrypted_storage/tests/notes.csv.enc`
* `soal_2/fuse`
* `soal_2/client`
* `soal_2/fuse.log`
* file normal yang muncul di `fuse_mount`

---

## C. Kode Lengkap `fuse.c`

<details>
<summary>Klik untuk membuka/menutup kode lengkap fuse.c</summary>

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

</details>

---

## D. Kode Lengkap `client.c`

<details>
<summary>Klik untuk membuka/menutup kode lengkap client.c</summary>

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

</details>

---

## E. Isi `Dockerfile` Soal 2

<details>
<summary>Klik untuk membuka/menutup Dockerfile soal_2</summary>

```dockerfile
FROM ubuntu:latest

WORKDIR /app
COPY server /app/server
RUN chmod +x /app/server && mkdir -p /app/db

EXPOSE 9000
CMD ["/app/server"]
```

</details>

---

## F. Cara Menjalankan Soal 2

Semua langkah pada bagian ini dimulai dari folder utama repository.

### 1. Masuk ke folder `soal_2`

```bash
cd soal_2
```

### 2. Lepaskan mount lama jika masih ada

```bash
fusermount3 -u fuse_mount 2>/dev/null || true
```

### 3. Hapus container lama jika ada

```bash
sudo docker rm -f db_app 2>/dev/null || true
```

### 4. Hapus file compile lama

```bash
rm -f fuse
```

### 5. Hapus file client hasil compile lama

```bash
rm -f client
```

### 6. Hapus log FUSE lama

```bash
rm -f fuse.log
```

### 7. Bersihkan folder backend lama

```bash
rm -rf encrypted_storage/*
```

### 8. Bersihkan folder mount lama

```bash
rm -rf fuse_mount/*
```

### 9. Buat folder backend untuk file uji terenkripsi

```bash
mkdir -p encrypted_storage/tests
```

### 10. Buat folder mount point

```bash
mkdir -p fuse_mount
```

### 11. Buat file `notes.csv.enc` sesuai bahan soal

Command ini membuat ulang file terenkripsi dari bentuk base64 agar orang yang baru download ZIP tetap bisa mengetes hasil decrypt.

```bash
cat > encrypted_storage/tests/notes.csv.enc.b64 <<'EOF'
FwMCHhkEWhgZAhMFfBcSGx8YWiIzJSIpJSM1NTMlJXw=
EOF
```

### 12. Decode file base64 menjadi `notes.csv.enc`

```bash
base64 -d encrypted_storage/tests/notes.csv.enc.b64 > encrypted_storage/tests/notes.csv.enc
```

### 13. Hapus file base64 sementara

```bash
rm -f encrypted_storage/tests/notes.csv.enc.b64
```

### 14. Cek file encrypted bawaan soal

```bash
ls encrypted_storage/tests
```

Output yang diharapkan:

```text
notes.csv.enc
```

### 15. Compile program FUSE

```bash
gcc fuse.c $(pkg-config fuse3 --cflags --libs) -o fuse
```

### 16. Compile program client

```bash
gcc client.c -o client
```

### 17. Beri permission executable pada server

```bash
chmod +x server
```

### 18. Jalankan FUSE

Agar mudah dites dalam satu terminal, FUSE dijalankan di background.

```bash
./fuse encrypted_storage fuse_mount -o allow_other -f > fuse.log 2>&1 &
```

### 19. Tunggu proses mount

```bash
sleep 2
```

### 20. Cek file yang muncul di mount point

```bash
ls fuse_mount/tests
```

Output yang diharapkan:

```text
notes.csv
```

File asli bernama `notes.csv.enc`, tetapi di mount point tampil sebagai `notes.csv`.

### 21. Cek isi file hasil decrypt

```bash
cat fuse_mount/tests/notes.csv
```

Output yang diharapkan:

```text
author,notes
admin,TEST_SUCCESS
```

### 22. Buat file uji baru melalui `fuse_mount`

```bash
echo "halo database" > fuse_mount/test.txt
```

### 23. Baca file uji dari `fuse_mount`

```bash
cat fuse_mount/test.txt
```

Output yang diharapkan:

```text
halo database
```

### 24. Cek file asli di backend

```bash
ls encrypted_storage
```

Output yang diharapkan minimal terdapat:

```text
test.txt.enc
tests
```

Artinya file yang terlihat normal di `fuse_mount` tersimpan sebagai file terenkripsi di `encrypted_storage`.

### 25. Cek isi file terenkripsi

```bash
xxd encrypted_storage/test.txt.enc
```

Isi file seharusnya tidak terbaca sebagai teks normal.

### 26. Build Docker image

```bash
sudo docker build -t soal-2-modul-4-sisop .
```

### 27. Jalankan container server

```bash
sudo docker run -d --name db_app -p 9000:9000 -v "$(pwd)/fuse_mount:/app/db" soal-2-modul-4-sisop
```

### 28. Cek container

```bash
sudo docker ps
```

Output yang diharapkan terdapat container:

```text
db_app
```

### 29. Cek log server

```bash
sudo docker logs db_app
```

### 30. Jalankan client

```bash
./client 127.0.0.1 9000
```

Output awal yang diharapkan:

```text
Connected to DB Server on 127.0.0.1:9000
Type EXIT to quit.
db>
```

### 31. Coba command pada client

Ketik:

```text
HELP
```

### 32. Keluar dari client

Ketik:

```text
EXIT
```

### 33. Stop container Soal 2

```bash
sudo docker rm -f db_app
```

### 34. Stop FUSE Soal 2

```bash
fusermount3 -u fuse_mount
```

### 35. Kembali ke folder utama repository

```bash
cd ..
```

---

## G. Penjelasan Kode Soal 2

### 1. XOR key

Program menggunakan:

```c
#define XOR_KEY 0x76
```

Key ini digunakan untuk mengenkripsi dan mendekripsi isi file.

### 2. Ekstensi `.enc`

File backend diberi ekstensi `.enc`. Misalnya `test.txt` di `fuse_mount` akan tersimpan sebagai `test.txt.enc` di `encrypted_storage`.

### 3. Fungsi `xor_buffer()`

Fungsi ini melakukan XOR terhadap setiap byte pada buffer file. Karena XOR bersifat reversible, fungsi yang sama dapat digunakan untuk enkripsi dan dekripsi.

### 4. Fungsi `readdir`

Fungsi ini mengatur tampilan folder pada `fuse_mount`. File yang tersimpan sebagai `.enc` di backend ditampilkan tanpa ekstensi `.enc`.

### 5. Fungsi `read`

Saat file dibaca dari `fuse_mount`, program membaca file `.enc` dari backend, melakukan XOR, lalu mengembalikan isi file dalam bentuk normal.

### 6. Fungsi `write`

Saat user menulis file melalui `fuse_mount`, data dienkripsi menggunakan XOR sebelum disimpan ke backend.

### 7. Program `client.c`

`client.c` membuat koneksi TCP ke server. Program membaca input user, mengirim command ke server, lalu menampilkan response.

### 8. Dockerfile

Dockerfile menyalin binary `server` ke `/app/server`, membuat folder `/app/db`, membuka port `9000`, dan menjalankan server.

---

## H. Hasil Akhir Soal 2

Struktur final ZIP tetap bersih:

```text
soal_2/
├── client.c
├── Dockerfile
├── encrypted_storage/
├── fuse.c
├── fuse_mount/
└── server
```

File `notes.csv.enc` dibuat saat runtime melalui langkah run.

---

# Reporting Soal 3

## A. Deskripsi Soal

Pada soal ketiga, dibuat sistem file sharing bernama **LibraryIT** menggunakan Samba di dalam Docker. Sistem ini memiliki beberapa share:

* `docs`
* `ebooks`
* `papers`
* `sourcecode`

Setiap user memiliki hak akses berbeda:

* `member` hanya dapat membaca share tertentu.
* `contributor` dapat menulis ke `ebooks`, `papers`, dan `sourcecode`, tetapi tidak dapat menulis ke `docs`.
* `librarian` dapat menulis ke `docs` dan termasuk user staff.
* anonymous access tidak diperbolehkan.
* `sourcecode` tidak dapat diakses oleh `member`.

Data Samba disimpan ke folder host `data/`, sehingga data tetap ada meskipun container dimatikan. Selain itu, terdapat logger yang membaca audit log Samba dan menuliskannya ke `logs/libraryit.log`.

---

## B. File yang Digunakan

File utama:

* `soal_3/Dockerfile`
* `soal_3/docker-compose.yml`
* `soal_3/smb.conf`
* `soal_3/entrypoint.sh`
* `soal_3/data/`
* `soal_3/logs/libraryit.log`

Folder runtime:

* `soal_3/data/docs/`
* `soal_3/data/ebooks/`
* `soal_3/data/papers/`
* `soal_3/data/sourcecode/`
* `soal_3/logs/`

---

## C. Isi `Dockerfile` Soal 3

<details>
<summary>Klik untuk membuka/menutup Dockerfile soal_3</summary>

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

</details>

---

## D. Isi `docker-compose.yml`

<details>
<summary>Klik untuk membuka/menutup docker-compose.yml</summary>

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

</details>

---

## E. Isi `smb.conf`

<details>
<summary>Klik untuk membuka/menutup smb.conf</summary>

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

</details>

---

## F. Isi `entrypoint.sh`

<details>
<summary>Klik untuk membuka/menutup entrypoint.sh</summary>

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

</details>

---

## G. Cara Menjalankan Soal 3

Semua langkah pada bagian ini dimulai dari folder utama repository.

### 1. Masuk ke folder `soal_3`

```bash
cd soal_3
```

### 2. Buat folder `docs`

```bash
mkdir -p data/docs
```

### 3. Buat folder `ebooks`

```bash
mkdir -p data/ebooks
```

### 4. Buat folder `papers`

```bash
mkdir -p data/papers
```

### 5. Buat folder `sourcecode`

```bash
mkdir -p data/sourcecode
```

### 6. Buat folder `logs`

```bash
mkdir -p logs
```

### 7. Buat file log utama

```bash
touch logs/libraryit.log
```

### 8. Beri permission executable pada `entrypoint.sh`

```bash
chmod +x entrypoint.sh
```

### 9. Matikan Samba bawaan host jika ada

Langkah ini dilakukan agar port `445` tidak bentrok dengan container Samba.

```bash
sudo systemctl stop smbd nmbd 2>/dev/null || true
```

### 10. Hapus container server lama jika ada

```bash
sudo docker rm -f libraryit-server 2>/dev/null || true
```

### 11. Hapus container logger lama jika ada

```bash
sudo docker rm -f libraryit-logger 2>/dev/null || true
```

### 12. Matikan compose lama jika ada

```bash
sudo docker compose down --remove-orphans 2>/dev/null || true
```

### 13. Build Docker Compose

```bash
sudo docker compose build --no-cache
```

### 14. Jalankan Docker Compose

```bash
sudo docker compose up -d
```

### 15. Tunggu container siap

```bash
sleep 5
```

### 16. Cek container

```bash
sudo docker ps
```

Output yang diharapkan terdapat container:

```text
libraryit-server
libraryit-logger
```

### 17. Tes daftar share sebagai `member`

```bash
smbclient -L //127.0.0.1 -U member --password='member123' -m SMB3
```

Output yang diharapkan:

```text
docs
ebooks
papers
IPC$
```

Share `sourcecode` tidak muncul untuk `member` karena aksesnya hanya untuk group `staff`.

Jika muncul pesan:

```text
Reconnect with SMB1 for workgroup listing
Unable to connect with SMB1
```

pesan tersebut dapat diabaikan karena share SMB3 sudah berhasil tampil.

### 18. Tes anonymous access

```bash
smbclient -L //127.0.0.1 -N -m SMB3
```

Hasil yang diharapkan:

* anonymous access gagal
* user tanpa login tidak dapat mengakses share

### 19. Tes member membaca `docs`

```bash
smbclient //127.0.0.1/docs -U member --password='member123' -m SMB3 -c "ls"
```

Hasil yang diharapkan:

* user `member` dapat membaca isi `docs`

### 20. Buat file testing untuk `ebooks`

```bash
echo "test ebook" > ~/test_ebook.txt
```

### 21. Upload file ke `ebooks` sebagai contributor

```bash
smbclient //127.0.0.1/ebooks -U contributor --password='contrib456' -m SMB3 -c "put $HOME/test_ebook.txt test_ebook.txt; ls"
```

Hasil yang diharapkan:

* upload berhasil
* file `test_ebook.txt` muncul di share `ebooks`

### 22. Buat file testing untuk `papers`

```bash
echo "test paper" > ~/test_paper.txt
```

### 23. Upload file ke `papers` sebagai contributor

```bash
smbclient //127.0.0.1/papers -U contributor --password='contrib456' -m SMB3 -c "put $HOME/test_paper.txt test_paper.txt; ls"
```

Hasil yang diharapkan:

* upload berhasil
* file `test_paper.txt` muncul di share `papers`

### 24. Buat file testing untuk `docs`

```bash
echo "dokumen librarian" > ~/test_docs.txt
```

### 25. Upload file ke `docs` sebagai librarian

```bash
smbclient //127.0.0.1/docs -U librarian --password='lib789' -m SMB3 -c "put $HOME/test_docs.txt test_docs.txt; ls"
```

Hasil yang diharapkan:

* upload berhasil karena `librarian` memiliki hak tulis ke `docs`

### 26. Buat file testing contributor untuk `docs`

```bash
echo "dokumen contributor" > ~/contributor_docs.txt
```

### 27. Tes contributor menulis ke `docs`

```bash
smbclient //127.0.0.1/docs -U contributor --password='contrib456' -m SMB3 -c "put $HOME/contributor_docs.txt contributor_docs.txt; ls"
```

Hasil yang diharapkan:

```text
NT_STATUS_ACCESS_DENIED
```

atau upload gagal.

### 28. Tes member tidak bisa akses `sourcecode`

```bash
smbclient //127.0.0.1/sourcecode -U member --password='member123' -m SMB3 -c "ls"
```

Output yang diharapkan:

```text
NT_STATUS_ACCESS_DENIED
```

### 29. Buat file testing untuk `sourcecode`

```bash
echo "print('hello sourcecode')" > ~/main.py
```

### 30. Upload file ke `sourcecode` sebagai contributor

```bash
smbclient //127.0.0.1/sourcecode -U contributor --password='contrib456' -m SMB3 -c "put $HOME/main.py main.py; ls"
```

Hasil yang diharapkan:

* upload berhasil
* file `main.py` muncul di share `sourcecode`

### 31. Cek persistence data

```bash
sudo find data -type f
```

Output yang diharapkan setelah testing:

```text
data/ebooks/test_ebook.txt
data/papers/test_paper.txt
data/docs/test_docs.txt
data/sourcecode/main.py
```

Artinya data berhasil tersimpan di folder host `data/`, bukan hanya di dalam container.

### 32. Cek log final

```bash
cat logs/libraryit.log | tail -30
```

### 33. Cek log container logger

```bash
sudo docker logs libraryit-logger --tail=30
```

Format log yang diharapkan:

```text
[YYYY-MM-DD HH:MM:SS] [INFO] [username] [AKSI] [file/share]
[YYYY-MM-DD HH:MM:SS] [WARNING] [username] [DENIED] [file/share]
```

Contoh:

```text
[2026-05-13 11:56:36] [INFO] [contributor] [CLOSE] [/libraryit/ebooks]
[2026-05-13 11:56:36] [WARNING] [member] [DENIED] [/libraryit/sourcecode]
```

### 34. Stop Soal 3

```bash
sudo docker compose down
```

### 35. Kembali ke folder utama repository

```bash
cd ..
```

---

## H. Penjelasan Konfigurasi Soal 3

### 1. `docker-compose.yml`

File ini menjalankan dua service:

* `libraryit-server`, yaitu container Samba server.
* `libraryit-logger`, yaitu container logger.

Folder `data` di host di-mount ke `/libraryit`, sedangkan folder `logs` di-mount ke `/var/log/libraryit`.

### 2. Dockerfile

Dockerfile menggunakan image Ubuntu, menginstall Samba, modul VFS Samba, rsyslog, dan Python. Setelah itu Dockerfile menyalin `smb.conf` dan `entrypoint.sh` ke dalam container.

### 3. User dan group

Pada `entrypoint.sh`, dibuat dua group:

```text
readonly
staff
```

Kemudian dibuat tiga user:

```text
member       -> readonly
contributor  -> staff
librarian    -> staff
```

Password Samba yang digunakan:

```text
member       : member123
contributor  : contrib456
librarian    : lib789
```

### 4. Share `docs`

Share `docs` bersifat read-only secara default. Namun user `librarian` masuk ke `write list`, sehingga hanya `librarian` yang dapat menulis.

### 5. Share `ebooks` dan `papers`

Share `ebooks` dan `papers` dapat dibaca oleh group `readonly` dan `staff`, tetapi hanya group `staff` yang dapat menulis.

### 6. Share `sourcecode`

Share `sourcecode` hanya dapat diakses oleh group `staff`. Karena `member` bukan bagian dari `staff`, maka akses ke `sourcecode` akan ditolak.

### 7. Logging

Samba menggunakan modul `full_audit` untuk mencatat aktivitas. Log mentah dikirim ke `LOCAL7`, lalu `rsyslog` menulisnya ke `audit_raw.log`. Service logger membaca log mentah tersebut, memformat ulang, lalu menyimpannya ke `libraryit.log`.

Format akhir log:

```text
[YYYY-MM-DD HH:MM:SS] [LEVEL] [USERNAME] [AKSI] [NAMA FILE/SHARE]
```

---

## I. Hasil Akhir Soal 3

```text
soal_3/
├── data/
│   ├── docs/
│   ├── ebooks/
│   ├── papers/
│   └── sourcecode/
├── docker-compose.yml
├── Dockerfile
├── entrypoint.sh
├── logs/
│   └── libraryit.log
└── smb.conf
```

---

# Cleanup Setelah Testing

Command ini digunakan untuk mengembalikan repository ke kondisi bersih setelah testing. Jalankan dari folder utama repository.

## 1. Unmount Soal 1 jika masih aktif

```bash
fusermount3 -u soal_1/mnt 2>/dev/null || true
```

## 2. Unmount Soal 2 jika masih aktif

```bash
fusermount3 -u soal_2/fuse_mount 2>/dev/null || true
```

## 3. Masuk ke folder Soal 3

```bash
cd soal_3
```

## 4. Matikan Docker Compose Soal 3

```bash
sudo docker compose down --remove-orphans 2>/dev/null || true
```

## 5. Kembali ke root repository

```bash
cd ..
```

## 6. Hapus container Soal 2 jika masih ada

```bash
sudo docker rm -f db_app 2>/dev/null || true
```

## 7. Bersihkan runtime Soal 1

```bash
rm -rf soal_1/amba_files
```

```bash
rm -rf soal_1/mnt
```

```bash
rm -f soal_1/kenz_rescue
```

```bash
rm -f soal_1/fuse.log
```

## 8. Bersihkan runtime Soal 2

```bash
rm -f soal_2/fuse
```

```bash
rm -f soal_2/client
```

```bash
rm -f soal_2/fuse.log
```

```bash
sudo rm -rf soal_2/encrypted_storage/*
```

```bash
sudo rm -rf soal_2/fuse_mount/*
```

## 9. Bersihkan data testing Soal 3

```bash
sudo rm -rf soal_3/data/docs/*
```

```bash
sudo rm -rf soal_3/data/ebooks/*
```

```bash
sudo rm -rf soal_3/data/papers/*
```

```bash
sudo rm -rf soal_3/data/sourcecode/*
```

```bash
sudo rm -rf soal_3/logs/*
```

## 10. Buat ulang folder penting Soal 2

```bash
mkdir -p soal_2/encrypted_storage
```

```bash
mkdir -p soal_2/fuse_mount
```

## 11. Buat ulang folder penting Soal 3

```bash
mkdir -p soal_3/data/docs
```

```bash
mkdir -p soal_3/data/ebooks
```

```bash
mkdir -p soal_3/data/papers
```

```bash
mkdir -p soal_3/data/sourcecode
```

```bash
mkdir -p soal_3/logs
```

## 12. Buat ulang file log utama

```bash
touch soal_3/logs/libraryit.log
```

## 13. Perbaiki permission folder

```bash
sudo chown -R "$USER:$USER" soal_3/data soal_3/logs
```

```bash
chmod 755 soal_3/data
```

```bash
chmod 755 soal_3/data/docs
```

```bash
chmod 755 soal_3/data/ebooks
```

```bash
chmod 755 soal_3/data/papers
```

```bash
chmod 755 soal_3/data/sourcecode
```

```bash
chmod 755 soal_3/logs
```

```bash
chmod 644 soal_3/logs/libraryit.log
```

## 14. Cek struktur akhir

```bash
tree
```

Struktur yang diharapkan:

```text
.
├── soal_1
│   └── kenz_rescue.c
├── soal_2
│   ├── client.c
│   ├── Dockerfile
│   ├── encrypted_storage/
│   ├── fuse.c
│   ├── fuse_mount/
│   └── server
└── soal_3
    ├── data/
    │   ├── docs/
    │   ├── ebooks/
    │   ├── papers/
    │   └── sourcecode/
    ├── docker-compose.yml
    ├── Dockerfile
    ├── entrypoint.sh
    ├── logs/
    │   └── libraryit.log
    └── smb.conf
```

---

# Kesimpulan

Dari ketiga soal pada modul ini, saya memahami beberapa konsep penting dalam Sistem Operasi.

* **Soal 1** memperlihatkan cara membuat file virtual menggunakan FUSE dan membaca data dari beberapa file sumber.
* **Soal 2** memperlihatkan cara membuat file system terenkripsi sederhana menggunakan FUSE serta menghubungkannya dengan server melalui Docker.
* **Soal 3** memperlihatkan cara mengatur Samba file sharing di Docker, membatasi permission berdasarkan user/group, menjaga persistence data, dan membuat sistem logging.

Secara keseluruhan, seluruh soal sudah dapat dijalankan sesuai kebutuhan, mulai dari mounting FUSE, enkripsi file, komunikasi client-server, file sharing Samba, hingga pencatatan log aktivitas.
