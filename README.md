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
│   │   └── .gitkeep
│   ├── fuse.c
│   ├── fuse_mount
│   │   └── .gitkeep
│   └── server
└── soal_3
    ├── data
    │   ├── docs
    │   │   └── .gitkeep
    │   ├── ebooks
    │   │   └── .gitkeep
    │   ├── papers
    │   │   └── .gitkeep
    │   └── sourcecode
    │       └── .gitkeep
    ├── docker-compose.yml
    ├── Dockerfile
    ├── entrypoint.sh
    ├── logs
    │   └── libraryit.log
    └── smb.conf
```

---

# Pendahuluan

Pada modul ini saya mengerjakan tiga soal yang berkaitan dengan konsep Sistem Operasi, khususnya penggunaan **FUSE**, **file system virtual**, **enkripsi sederhana**, **Docker**, **Samba file sharing**, permission user, persistence data, dan logging.

Secara garis besar:

* **Soal 1** membahas pembuatan file system virtual menggunakan FUSE untuk menampilkan file tambahan bernama `tujuan.txt`.
* **Soal 2** membahas FUSE untuk menyimpan file dalam bentuk terenkripsi menggunakan XOR dan menghubungkannya dengan server database di dalam Docker.
* **Soal 3** membahas pembuatan server file sharing menggunakan Samba di Docker, pembatasan akses berdasarkan user/group, persistence data, dan logging aktivitas.

Dengan mengerjakan modul ini, saya memahami bagaimana file system dapat dibuat secara virtual, bagaimana data dapat disimpan dalam bentuk terenkripsi, serta bagaimana service berbasis container dapat dikonfigurasi agar memiliki permission dan logging yang sesuai kebutuhan.

---

# Requirement Umum

Sebelum menjalankan program, install package yang dibutuhkan.

```bash
sudo apt update
sudo apt install -y gcc pkg-config libfuse3-dev fuse3 tree docker.io smbclient cifs-utils python3 curl zip unzip
sudo systemctl enable --now docker
sudo modprobe fuse
```

Untuk FUSE yang menggunakan opsi `allow_other`, aktifkan konfigurasi berikut.

```bash
sudo sed -i 's/^#user_allow_other/user_allow_other/' /etc/fuse.conf
grep -q '^user_allow_other' /etc/fuse.conf || echo 'user_allow_other' | sudo tee -a /etc/fuse.conf
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

* `soal_1/kenz_rescue.c`
* folder runtime `amba_files/`
* folder runtime `mnt/`

Pada repository final, folder `amba_files` dan `mnt` tidak ikut dikumpulkan karena hanya digunakan untuk testing.

---

## C. Konsep Penyelesaian

Program `kenz_rescue.c` menggunakan FUSE untuk membuat file system virtual. Program membaca folder asli, lalu menampilkan isinya pada mount point. Untuk file biasa, program meneruskan operasi baca ke file asli. Untuk file `tujuan.txt`, program membuat isinya secara dinamis.

Beberapa fungsi utama yang digunakan:

* `getattr()` untuk mengambil atribut file.
* `readdir()` untuk menampilkan isi direktori.
* `open()` untuk membuka file.
* `read()` untuk membaca file.
* `release()` untuk menutup file.
* fungsi khusus untuk membaca `KOORD:` dari file `1.txt` sampai `7.txt`.

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

Setiap file memiliki baris yang diawali dengan `KOORD:`.

### 3. Compile program

```bash
gcc kenz_rescue.c $(pkg-config fuse3 --cflags --libs) -o kenz_rescue
```

Hasil yang diharapkan:

* file executable `kenz_rescue` berhasil dibuat
* tidak ada error saat compile

### 4. Jalankan FUSE

```bash
./kenz_rescue amba_files mnt -f
```

Terminal akan terlihat diam karena FUSE berjalan di foreground.

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

### 7. Cek folder asli

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

File `tujuan.txt` hanya muncul di `mnt`, bukan di `amba_files`.

### 8. Unmount FUSE

```bash
fusermount3 -u mnt
```

---

## E. Penjelasan Kode per Bagian

### 1. Deklarasi FUSE

Program menggunakan:

```c
#define FUSE_USE_VERSION 31
```

Bagian ini menunjukkan bahwa program menggunakan FUSE versi 3.

### 2. File virtual `tujuan.txt`

Program mendefinisikan nama file virtual sebagai `tujuan.txt`. File ini tidak dibuat secara fisik di folder sumber, melainkan hanya dimunculkan melalui FUSE.

### 3. Pembacaan koordinat

Program membaca file `1.txt` sampai `7.txt`, mencari baris dengan awalan `KOORD:`, lalu mengambil isi setelah prefix tersebut. Setiap potongan koordinat dibersihkan dari spasi dan newline, kemudian digabung menjadi satu string.

### 4. Fungsi `readdir`

Fungsi ini menampilkan isi direktori asli dan menambahkan `tujuan.txt` ke daftar file yang muncul pada mount point.

### 5. Fungsi `read`

Jika file yang dibaca adalah file biasa, maka program membaca file asli dari folder sumber. Jika file yang dibaca adalah `tujuan.txt`, maka program membuat isi file secara dinamis dari hasil gabungan koordinat.

---

## F. Hasil Akhir Soal 1

Struktur final yang dikumpulkan:

```text
soal_1/
└── kenz_rescue.c
```

---

# Reporting Soal 2

## A. Deskripsi Soal

Pada soal kedua, dibuat file system menggunakan FUSE yang menyimpan data ke folder `encrypted_storage`. Data yang dilihat user melalui folder `fuse_mount` tampil seperti file normal, tetapi data asli yang tersimpan di backend memiliki ekstensi `.enc` dan dienkripsi menggunakan XOR.

Selain itu, soal ini menggunakan server database yang dijalankan melalui Docker. Folder hasil mount FUSE dihubungkan ke container sebagai `/app/db`, sehingga server dapat membaca file yang sudah didekripsi oleh FUSE.

Secara konsep:

* user membaca dan menulis file melalui `fuse_mount`
* file asli disimpan di `encrypted_storage`
* file backend disimpan dengan ekstensi `.enc`
* isi file backend dienkripsi menggunakan XOR key `0x76`
* Docker menjalankan binary `server`
* client C terhubung ke server melalui port `9000`

---

## B. File yang Digunakan

* `soal_2/fuse.c`
* `soal_2/client.c`
* `soal_2/Dockerfile`
* `soal_2/server`
* `soal_2/encrypted_storage/`
* `soal_2/fuse_mount/`

---

## C. Konsep Penyelesaian

Program `fuse.c` berfungsi sebagai penghubung antara folder yang dilihat user dan folder penyimpanan asli. Saat user membuat atau menulis file di `fuse_mount`, data akan dienkripsi terlebih dahulu lalu disimpan ke `encrypted_storage` dengan ekstensi `.enc`.

Sebaliknya, saat user membaca file dari `fuse_mount`, program akan membaca file `.enc` dari backend, mendekripsinya menggunakan XOR, lalu menampilkannya sebagai teks normal.

Program `client.c` digunakan untuk menghubungkan user ke server melalui TCP. Sementara itu, Docker digunakan untuk menjalankan binary `server` pada port `9000`.

---

## D. Cara Menjalankan dan Hasil per Tahap

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

* file executable `fuse` berhasil dibuat
* file executable `client` berhasil dibuat
* file `server` memiliki permission executable

### 4. Jalankan FUSE

```bash
./fuse encrypted_storage fuse_mount -o allow_other -f
```

Terminal akan terlihat diam karena FUSE berjalan di foreground.

### 5. Buka terminal baru dan uji tulis file

```bash
cd soal_2
echo "halo database" > fuse_mount/test.txt
cat fuse_mount/test.txt
ls encrypted_storage
```

Hasil yang diharapkan saat membaca dari `fuse_mount`:

```text
halo database
```

Sedangkan di `encrypted_storage`, file tersimpan sebagai:

```text
test.txt.enc
```

### 6. Cek isi terenkripsi di backend

```bash
xxd encrypted_storage/test.txt.enc
```

Hasil yang diharapkan:

* isi file tidak terbaca sebagai teks normal karena sudah dienkripsi menggunakan XOR

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

Untuk melihat command yang tersedia:

```text
HELP
```

Untuk keluar:

```text
EXIT
```

### 11. Stop service

```bash
sudo docker rm -f db_app
fusermount3 -u fuse_mount
```

---

## E. Penjelasan Kode per Bagian

### 1. XOR key

Program menggunakan XOR key:

```c
#define XOR_KEY 0x76
```

Key ini digunakan untuk mengenkripsi dan mendekripsi isi file.

### 2. Ekstensi `.enc`

File yang disimpan di backend diberi ekstensi `.enc`. Misalnya file yang terlihat sebagai `test.txt` di `fuse_mount` akan tersimpan sebagai `test.txt.enc` di `encrypted_storage`.

### 3. Fungsi XOR

Fungsi XOR berjalan pada setiap byte data. Karena XOR bersifat reversible, fungsi yang sama dapat digunakan untuk enkripsi dan dekripsi.

### 4. Fungsi `readdir`

Saat user menjalankan `ls` di `fuse_mount`, program menampilkan nama file tanpa ekstensi `.enc`.

### 5. Fungsi `read`

Saat file dibaca dari `fuse_mount`, program membaca file `.enc` dari backend, melakukan XOR, lalu mengembalikan isi file dalam bentuk normal.

### 6. Fungsi `write`

Saat user menulis file melalui `fuse_mount`, data dienkripsi dengan XOR terlebih dahulu, kemudian disimpan ke backend.

### 7. Dockerfile

Dockerfile soal 2 menjalankan binary `server` di dalam container. Folder `fuse_mount` di-mount ke `/app/db`, sehingga server membaca file melalui layer FUSE.

---

## F. Hasil Akhir Soal 2

Struktur final yang dikumpulkan:

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

* `soal_3/Dockerfile`
* `soal_3/docker-compose.yml`
* `soal_3/smb.conf`
* `soal_3/entrypoint.sh`
* `soal_3/data/`
* `soal_3/logs/libraryit.log`

---

## C. Konsep Penyelesaian

Soal 3 menggunakan Docker Compose untuk menjalankan dua service:

* `libraryit-server`, yaitu container Samba server.
* `libraryit-logger`, yaitu container logger yang membaca log audit dan menulis hasil formatnya ke `libraryit.log`.

Folder `data/` pada host dihubungkan ke `/libraryit` dalam container agar data bersifat persistent. Folder `logs/` pada host dihubungkan ke `/var/log/libraryit` agar log tetap tersimpan di host.

User dan group dibuat di dalam container melalui `entrypoint.sh`.

---

## D. User dan Permission

User yang digunakan:

```text
member       : member123
contributor  : contrib456
librarian    : lib789
```

Group yang digunakan:

```text
readonly
staff
```

Pembagian akses:

```text
member       -> readonly
contributor  -> staff
librarian    -> staff
```

Aturan share:

* `docs`: dapat dibaca oleh semua user, tetapi hanya `librarian` yang dapat menulis.
* `ebooks`: dapat dibaca oleh `readonly` dan `staff`, tetapi hanya `staff` yang dapat menulis.
* `papers`: dapat dibaca oleh `readonly` dan `staff`, tetapi hanya `staff` yang dapat menulis.
* `sourcecode`: hanya dapat diakses oleh `staff`.

---

## E. Cara Menjalankan dan Hasil per Tahap

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

### 3. Matikan Samba bawaan host jika ada

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

Share `sourcecode` tidak muncul untuk `member`.

Pesan berikut dapat diabaikan karena hanya fallback SMB1 dari `smbclient`.

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

* upload gagal atau access denied

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

## F. Penjelasan Konfigurasi per Bagian

### 1. `docker-compose.yml`

File ini menjalankan dua service, yaitu `libraryit-server` dan `libraryit-logger`.

* `libraryit-server` menjalankan Samba dan membuka port `445`.
* `libraryit-logger` membaca log audit.
* Folder `data` di host di-mount ke `/libraryit`.
* Folder `logs` di host di-mount ke `/var/log/libraryit`.

### 2. Dockerfile

Dockerfile menggunakan image Ubuntu, lalu menginstall Samba, modul VFS Samba, rsyslog, dan Python. Setelah itu Dockerfile menyalin `smb.conf` serta `entrypoint.sh` ke dalam container.

### 3. `entrypoint.sh`

File ini memiliki dua mode:

* mode server untuk membuat user, group, permission folder, menjalankan rsyslog, dan menjalankan Samba.
* mode logger untuk membaca `audit_raw.log`, memformat log, dan menuliskannya ke `libraryit.log`.

### 4. Share `docs`

Share `docs` bersifat read-only secara default. Namun, user `librarian` masuk ke `write list`, sehingga hanya `librarian` yang dapat menulis ke folder ini.

### 5. Share `ebooks` dan `papers`

Share `ebooks` dan `papers` dapat dibaca oleh group `readonly` dan `staff`. Namun, hanya group `staff` yang dapat menulis.

### 6. Share `sourcecode`

Share `sourcecode` hanya dapat diakses oleh group `staff`. Karena `member` bukan bagian dari group `staff`, maka akses `member` ke share ini ditolak.

### 7. Logging

Samba menggunakan modul `full_audit` untuk mencatat aktivitas file. Log mentah dikirim ke fasilitas `LOCAL7`, kemudian `rsyslog` menuliskannya ke `audit_raw.log`. Service logger membaca file tersebut, memformat ulang log, dan menyimpannya ke `libraryit.log`.

Format akhir log:

```text
[YYYY-MM-DD HH:MM:SS] [LEVEL] [USERNAME] [AKSI] [NAMA FILE/SHARE]
```

---

## G. Hasil Akhir Soal 3

Struktur final yang dikumpulkan:

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

Setelah dibersihkan, cek struktur repository:

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

# ZIP Final

Untuk membuat file zip final:

```bash
cd ~
zip -r SISOP-4-2026-IT-124.zip "Modul 4"
```

File yang dikumpulkan:

```text
SISOP-4-2026-IT-124.zip
```

---

# Link Repository

Repository GitHub:

```text
https://github.com/ndrr1/SISOP-4-2026-IT-124
```

---

# Kesimpulan

Dari ketiga soal pada modul ini, saya dapat memahami beberapa konsep penting dalam Sistem Operasi.

* **Soal 1** memperlihatkan cara membuat file virtual menggunakan FUSE dan membaca data dari beberapa file sumber.
* **Soal 2** memperlihatkan cara membuat file system terenkripsi sederhana menggunakan FUSE serta menghubungkannya dengan server melalui Docker.
* **Soal 3** memperlihatkan cara mengatur Samba file sharing di Docker, membatasi permission berdasarkan user/group, menjaga persistence data, dan membuat sistem logging.

Secara keseluruhan, seluruh soal sudah dapat dijalankan sesuai kebutuhan, mulai dari mounting FUSE, enkripsi file, komunikasi client-server, file sharing Samba, hingga pencatatan log aktivitas.
