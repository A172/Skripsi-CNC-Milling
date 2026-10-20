# Changelog

## 2026-10-21

- Mengubah Work Origin menjadi menu dua tingkat: pilih slot `G54`-`G59.3`, lalu pilih `Pilih Origin` atau `Perbarui Origin`.
- Menjaga pola dialog seragam: Enter=Yes, Back=No, rotary memindahkan pilihan, dan klik rotary mengeksekusi pilihan yang disorot; default dialog tetap No.
- Memastikan `Perbarui Origin` belum menimpa workspace sebelum operator menyetujui konfirmasi penyimpanan.
- Menampilkan kode workspace pada judul konfirmasi pembaruan, misalnya `G54 ORIGIN`, dengan pesan ringkas `Perbarui origin?` agar tidak terpotong pada LCD.
- Menambahkan `examples/gcode/CEK_ORIGIN_XY.gcode` untuk pemeriksaan origin aktif tanpa menurunkan Z, dengan feedrate konservatif, jeda inspeksi 10 detik, dan Home All setelah pemeriksaan.

## 2026-10-20

- Mengubah fallback area kerja X pada interface dari `1300` menjadi `1250` mm agar konsisten dengan batas kerja Marlin terbaru.
- Memperbarui contoh payload MQTT area kerja menjadi X `1250`, Y `2600`, dan Z `140` mm.
- Menambahkan menu Work Origin untuk memilih workspace Marlin `G54`-`G59.3` yang tersimpan.
- Mengubah workflow Set Origin menjadi Set/Save Origin yang mengirim `G92 X0 Y0 Z0` dan `M500` pada workspace aktif.
- Menambahkan interlock agar Home All dan workspace harus dipilih ulang setelah homing, job selesai, stop, error, SD dilepas, atau koneksi Marlin terputus.
- Menghapus prompt repeat otomatis setelah safe park `G28`; pengulangan job kini harus melalui Home All dan pemilihan workspace tersimpan.
- Menambahkan `work_origin` dan `work_origin_valid` pada payload retained `cnc/machine` untuk menampilkan workspace aktif di dashboard Node-RED.
- Mempertahankan status Home dan Work Origin saat memilih file sehingga workflow dapat dilakukan dengan urutan `Home All -> Use G54 -> Select Job -> Start`.

## 2026-08-22

- Mengaktifkan log diagnostik UART Marlin `[MARLIN][TX]` dan `[MARLIN][RX]` untuk mengecek respons `M203`, `M115`, `M119`, `M211`, dan polling `M114`.
- Memperbarui dokumentasi integrasi Marlin sesuai kondisi uji hardware saat ini: koneksi Marlin aktif, RX GPIO44, dan TX GPIO43.
- Menambahkan build flag `CORE_DEBUG_LEVEL=0` dan memperlambat retry mount MicroSD menjadi 30 detik saat kartu dilepas agar log internal Arduino core seperti `sd_diskio.cpp` tidak memenuhi Serial Monitor.
- Mengubah pola alarm buzzer Marlin menjadi tiga beep tajam dengan restart langsung tanpa jeda panjang agar mendekati efek alarm/warning dan terdengar lebih urgent saat koneksi Marlin terputus.
- Menambahkan fallback Area X/Y/Z 1300/2600/140 mm pada Machine Ctrl&Status saat `M115` geometry report belum terbaca, sambil tetap mengganti nilai dari data Marlin jika respons valid masuk.
- Menginisialisasi fallback Area X/Y/Z sejak koneksi Marlin dimulai agar dashboard tidak menunggu menu Machine Ctrl&Status untuk menampilkan area kerja.
- Menambahkan Area X/Y/Z ke payload retained `cnc/machine` melalui field `area_x`, `area_y`, `area_z`, `area_source`, dan objek `work_area` untuk Data Mesin dashboard.
- Menambahkan prefetch status mesin round-robin setelah Marlin merespons agar `M203`, `M115`, `M119`, dan `M211` dikirim sebelum user masuk menu Machine Ctrl&Status tanpa tertahan oleh satu respons yang belum bisa diparse.
- Menghapus refresh otomatis saat membuka menu Machine Ctrl&Status agar menu langsung tampil memakai nilai prefetch boot; refresh ulang tetap lewat item `Refresh Status`.
- Menambahkan field `active`, `popup`, `source`, dan `code` pada payload retained `cnc/alarm` agar Node-RED dapat memunculkan pop-up alarm/warning dari payload terstruktur.
- Menambahkan warning `HOME_SENSOR_UNKNOWN` pada `cnc/alarm` ketika Marlin tersambung tetapi status sensor home/endstop belum terbaca valid.
- Mempertahankan `cnc/error` untuk error keras dan menambahkan field `source` serta `code` agar kompatibel dengan flow/dashboard lama.
- Memperbaiki format JSON payload `cnc/error` agar field `code` dan `message` valid.
- Memperbarui tanggal firmware terakhir menjadi `22/08/2026`.

## 2026-08-21

- Menambahkan field `home`, `home_valid`, dan `home_note` pada payload retained `cnc/machine` untuk dashboard sensor home/endstop.
- Mempertahankan field kompatibel `x_home`, `y_home`, dan `z_home` agar mapping dashboard lama tetap aman.
- Membuat status sensor MQTT memakai nilai `OPEN`, `TRIGGERED`, atau `UNKNOWN` dengan note `ok`, `not_connected`, atau `not_loaded`.
- Menambahkan polling `M119` berkala saat monitoring Marlin aktif dan Marlin sudah merespons, tanpa mengirim command gerakan.

- Menambahkan editor IP broker MQTT dari menu Network > Broker.
- Editor broker memakai format fixed digit `000.000.000.000`; contoh `192.168.250.9` diisi sebagai `192.168.250.009`.
- Putar rotary mengubah digit `0-9`, klik rotary atau Enter maju ke kanan, dan Back mundur ke kiri atau batal pada digit pertama.
- Menambahkan validasi oktet maksimal `255` dan dialog konfirmasi sebelum broker disimpan ke Preferences.
- Mengaktifkan ulang workflow LCD normal dengan membuat `CNC_HEADLESS_WIFI_MQTT_DEV` default nonaktif.
- Mematikan log tombol/rotary per tick secara default melalui `AppConfig::ENABLE_INPUT_EVENT_LOG`.
- Membatasi redraw LCD editor broker melalui `AppConfig::BROKER_EDITOR_REDRAW_MS` agar rotary lebih responsif dan LCD tidak berkedip berlebihan.
- Menurunkan interval redraw editor broker menjadi 30 ms dan menjeda connect/reconnect MQTT selama editor broker aktif.
- Mengubah highlight digit aktif editor broker menjadi negatif seperti highlight menu.
- Menambahkan jeda retry mount MicroSD 5 detik setelah mount gagal agar tombol dan rotary tetap responsif saat modul SD belum terpasang.
- Memperbarui tanggal firmware terakhir menjadi `21/08/2026`.

## 2026-08-09

- Menambahkan toggle sementara `CNC_HEADLESS_WIFI_MQTT_DEV` untuk pengembangan WiFi/MQTT saat LCD bermasalah.
- Mode headless melewati inisialisasi LCD dan input tombol/rotary, tetapi tetap menjalankan Serial Monitor, buzzer, MicroSD, WiFi, dan MQTT.
- Menambahkan guard di `AppController` agar fungsi tampilan tidak menulis ke LCD ketika mode headless aktif.
- Memperbarui tanggal firmware terakhir menjadi `09/08/2026`.

## 2026-08-08

- Menambahkan hot-swap MicroSD non-blocking dengan state `absent`, `mounting`, `ready`, dan `error`.
- Menambahkan topic `cnc/sd/status/request` dan `cnc/sd/status` untuk status MicroSD aktual.
- Menambahkan topic `cnc/file/delete` dan `cnc/file/delete/ack` untuk hapus file job MicroSD dari dashboard.
- Menolak `cnc/file/select` dari MQTT dengan reason `remote_selection_disabled` agar dashboard tidak dapat memilih job CNC.
- Menambahkan proteksi delete untuk file yang sedang dipakai, sedang menunggu konfirmasi lokal, target folder, path tidak valid, dan file `.part`.
- Menambahkan parameter upload opsional `overwrite`; target yang sudah ada ditolak dengan `file_exists` kecuali `overwrite:true`.
- Menambahkan mekanisme backup `.bak` saat overwrite agar file lama dipertahankan sampai file baru selesai diverifikasi.
- Mengubah protokol `cnc/file/start`, `cnc/file/chunk`, dan `cnc/file/finish` agar benar-benar menyimpan upload G-code ke MicroSD.
- Menambahkan penulisan file sementara `.part`, validasi ukuran, flush/close, rename ke file final, dan cleanup saat transfer gagal.
- Mengubah ACK FINISH berhasil menjadi `{"type":"finish","status":"saved",...}` setelah file final tersimpan.
- Menambahkan ACK error upload dengan reason `sd_not_ready`, `invalid_payload`, `invalid_filename`, `unsupported_extension`, `transfer_busy`, `no_active_transfer`, `open_failed`, `invalid_chunk`, `unexpected_chunk`, `write_failed`, `incomplete_transfer`, `size_mismatch`, dan `rename_failed`.
- Mempertahankan upload MQTT hanya sebagai penyimpanan file; file belum dipilih otomatis, belum dikirim ke Marlin, dan belum dieksekusi.
- Memperbarui tanggal firmware terakhir menjadi `08/08/2026`.

## 2026-08-07

- Mengaktifkan `CNC_ENABLE_NETWORK` di `src/main.cpp` untuk pengujian integrasi Node-RED dengan ESP32-S3.
- Menambahkan suffix topic MQTT `file/start`, `file/chunk`, `file/finish`, dan `file/ack`.
- Menambahkan subscribe `cnc/file/start`, `cnc/file/chunk`, dan `cnc/file/finish` tanpa menghapus `cnc/command` dan `cnc/gcode`.
- Menambahkan state validasi transfer file MQTT `IDLE -> RECEIVING -> COMPLETE`.
- Menambahkan ACK `cnc/file/ack` untuk START, CHUNK berurutan, dan FINISH.
- Membuat retry CHUNK terakhir dan FINISH setelah COMPLETE bersifat idempotent.
- Mempertahankan data G-code MQTT sebagai validasi komunikasi saja; belum disimpan ke SD card, belum dikirim ke Marlin, dan belum dieksekusi.
- Memindahkan inisialisasi `Serial1` ke `beginMarlinConnection()` agar Serial Monitor tidak terganggu saat `CNC_ENABLE_MARLIN_CONNECTION` masih nonaktif.
- Menambahkan request daftar file job MicroSD melalui `cnc/file/list/request`.
- Menambahkan publish daftar file job ke `cnc/file/list` dalam paket begin, item, end, dan error SD.
- Menambahkan pemindaian file job rekursif di `SdCardReader` dari root `/` dengan batas 8 tingkat folder dan maksimal 50 file.
- Menambahkan topic `cnc/file/select` dan `cnc/file/select/ack` untuk validasi pilihan file MicroSD dari dropdown Node-RED.
- Menambahkan validasi path file job di `SdCardReader` dengan reason error `invalid_payload`, `invalid_path`, `unsupported_extension`, `file_not_found`, `not_a_file`, `open_failed`, `sd_not_ready`, dan `request_busy`.
- Menyimpan pilihan file MQTT sebagai hasil validasi internal tanpa memilih job LCD, tanpa membuka jalur eksekusi, dan tanpa mengaktifkan Marlin.
- Memperbarui tanggal firmware terakhir menjadi `07/08/2026`.

## 2026-08-05

- Menambahkan bunyi loading pendek berulang saat firmware mencoba auto-connect memakai credential WiFi yang sudah tersimpan.
- Membuat Reset WiFi langsung membuka portal tanpa bunyi loading agar tidak mengganggu user saat mengisi ulang jaringan.
- Menambahkan dokumentasi lokasi pengaturan nada, durasi, dan interval buzzer.
- Menghentikan buzzer sebelum portal WiFiManager dibuka agar tone tombol tidak tertahan selama proses setup WiFi.
- Mengubah feedback WiFi/MQTT/Reset WiFi menjadi bunyi hasil sekali, bukan loading saat user masih perlu menyambungkan koneksi.
- Mengubah pola alarm buzzer agar memakai frekuensi nada Enter dengan format alarm panjang yang sama.
- Memperbaiki jumlah step pola alarm buzzer agar tidak membaca data di luar array.
- Memperbarui tanggal firmware terakhir menjadi `05/08/2026`.

## 2026-08-04

- Mengubah payload `cnc/status.state` menjadi `ONLINE` dan `OFFLINE` agar status MQTT tampil kapital di dashboard.
- Mengubah fallback `cnc/progress.file` menjadi `Belum dipilih` saat user belum memilih file job.
- Mengubah tampilan waktu LCD menjadi `HH:MM` dan menampilkan kalender lengkap `DD/MM/YYYY` pada menu Network.
- Menambahkan pengaman renderer standby agar input waktu `HH:MM:SS` tetap tampil sebagai `HH:MM` di LCD.
- Merapikan baris atas standby menjadi format `CNC:[OK/X] | aktivitas`.
- Memisahkan `cnc/machine.state` sebagai status koneksi Marlin dan `cnc/machine.activity` sebagai aktivitas kerja mesin.
- Menambahkan alamat dashboard Node-RED skripsi ke README.
- Mengirim `cnc/network.mqtt` sebagai `DISCONNECTED` saat MQTT diputus secara normal agar retained network tidak terus menampilkan `CONNECTED`.
- Menjelaskan bahwa Node-RED harus memakai `cnc/status` sebagai sumber utama saat interface offline karena Last Will hanya menjamin update pada `cnc/status`.
- Memperbarui dokumentasi contoh payload `cnc/status`.

## 2026-08-03

- Menambahkan workflow GitHub Actions `.github/workflows/platformio-build.yml` agar build firmware ikut dicek saat ada perubahan ke GitHub.
- Workflow berjalan pada push dan pull request ke branch `main`.
- Workflow memasang PlatformIO lewat `pip`, lalu menjalankan `platformio run -e esp32-s3-devkitm-1`.
- Menambahkan catatan bahwa CI hanya memeriksa kompilasi, bukan upload firmware atau pengujian hardware.

## 2026-08-01

- Menambahkan item `Alarm: OK/ERROR` pada menu Machine Ctrl&Status.
- Menambahkan item `Test Buzzer` pada Machine Ctrl&Status untuk memutar pola alarm buzzer satu kali tanpa mengirim command ke Marlin.
- Menyamakan sumber status alarm menu dengan kondisi `LOST`, `ERROR`, dan job sender `ERROR`.
- Mengganti teks bawah halaman kedua About dari `AYB Interface` menjadi `PT AUDIO YUDISTIRO BUANA`.
- Mengubah pola alarm buzzer menjadi nada panjang berulang.
- Menambahkan pola loading pendek untuk aktivasi WiFi/MQTT.
- Mengubah nada Back menjadi lebih pendek dan lebih tinggi agar tidak terdengar serak.
- Memperbarui tanggal firmware terakhir menjadi `01/08/2026`.
- Memperbarui README dan PROJECT_STATUS agar fungsi alarm/test buzzer terdokumentasi.

## 2026-06-21

- Menambahkan `GcodeJobController` untuk analisis file SD, flow control `ok`, progress, timeout, dan state job.
- Menambahkan estimasi waktu tersisa dari G0/G1/G2/G3/G4 dengan dukungan unit serta mode posisi modal.
- Mengaktifkan `X+/PLAY` untuk Start/Resume, `X-/PAUSE` untuk Pause, dan long-press `Z-/STOP` untuk controlled stop.
- Menambahkan preflight file, koneksi Marlin, Home All, Set Origin, dan soft endstop sebelum Start.
- Menambahkan field kompatibel `activity` pada `cnc/machine` dan state aktual pada `cnc/progress`.
- Menambahkan alarm/error MQTT ketika job sender masuk state `ERROR`.
- Menambahkan heartbeat `M114` saat pause dan memblokir polling lain ketika job menguasai UART.
- Memperbarui tanggal firmware terakhir menjadi `21/06/2026`.
- Menambahkan garis pemisah antara status WiFi/MQTT dan informasi job pada standby ketika file dipilih.
- Merapikan posisi vertikal elemen standby agar garis, nama job, dan estimasi tetap muat pada LCD 128x64.
- Mengaudit seluruh topic MQTT dan mempertahankan `cnc/status`, `cnc/network`, `cnc/position`, `cnc/progress`, `cnc/time`, `cnc/alarm`, `cnc/error`, `cnc/command`, `cnc/gcode`, dan `cnc/machine`.
- Menambahkan field `file` pada payload retained `cnc/progress` dari file SD card yang benar-benar dipilih.
- Menggunakan fallback progress aman `Belum dipilih`, `0`, dan `IDLE` saat job belum tersedia atau belum berjalan.
- Mengubah placeholder detail mesin yang belum tersedia dari `?` menjadi `UNKNOWN` tanpa mengubah nama field `cnc/machine`.
- Menormalkan kondisi alarm tanpa error menjadi `{"level":"INFO","message":"OK"}`.
- Mendokumentasikan pemetaan topic dan field untuk Node-RED Dashboard 2.0.

## 2026-06-20

- Mengganti scrolling piksel About menjadi tiga halaman yang berganti setiap 3 detik agar mudah dibaca.
- Mengubah indikator standby menjadi label lengkap `WiFi:[OK]/WiFi:[X]` dan `MQTT:[OK]/MQTT:[X]`.
- Menempatkan indikator WiFi/MQTT pada satu baris khusus dan mempertahankan font koordinat `6x10`.
- Mengurutkan Machine Ctrl&Status menjadi CNC, Home X/Y/Z, SoftEnd, Spindle, Feed XY/Z, Area X/Y/Z, lalu Refresh Status.
- Menyesuaikan indeks handler agar navigasi dan aksi setiap item tetap benar setelah perubahan urutan.
- Melengkapi README dengan fungsi seluruh menu/submenu yang sudah tersedia.
- Menjelaskan bahwa Refresh Status membaca ulang `M203`, `M115`, `M119`, dan `M211` tanpa menggerakkan mesin.
- Mendokumentasikan bahwa edit Area X/Y/Z masih lokal dan belum diterapkan ke Marlin.
- Menambahkan topic retained `cnc/machine` untuk state CNC, spindle, soft endstop, feedrate, dan sensor home.
- Menandai detail mesin sebagai `?` atau `null` saat CNC belum `CONNECTED` agar status lama tidak dianggap data nyata.
- Menambahkan validitas dan sumber data pada `cnc/position`.
- Menampilkan posisi tidak tersedia sebagai nilai `null` dengan display `?`, bukan angka nol palsu.
- Menyiapkan simulasi posisi MQTT dengan default nonaktif.
- Mengganti menu `Settings` menjadi `About` pada urutan terakhir workflow menu utama.
- Menambahkan layar About dengan logo, nama firmware, versi, Alfath Yusuf Biyono, NIM 2141170132, dan tanggal update.
- Menambahkan scrolling About non-blocking serta keluar melalui Enter, Back, atau klik rotary.
- Menetapkan nama firmware `AYB Interface` secara terpusat di `AppConfig.h`.
- Menggunakan identitas firmware pada boot screen, Serial Monitor, menu About, dan dokumentasi.
- Menambahkan toggle `CNC_ENABLE_MARLIN_CONNECTION` di `main.cpp`; default dinonaktifkan selama SKR belum tersambung.
- Mengganti status standby ambigu `M:--/OK/NO` menjadi `CNC:OFF/WAIT/DISC/OK/LOST/ERR`.
- Menambahkan state komunikasi Marlin `OFF`, `WAITING`, `DISCONNECTED`, `CONNECTED`, `LOST`, dan `ERROR`.
- Memblokir pengiriman perintah mesin sampai Marlin berstatus `CONNECTED`.
- Mengubah alarm buzzer dan MQTT agar hanya aktif untuk koneksi yang terputus setelah pernah tersambung atau respons error Marlin.

## 2026-06-19

- Menambahkan passive buzzer pada GPIO 38 menggunakan PWM LEDC.
- Menambahkan `BuzzerHandler` non-blocking dengan prioritas pola bunyi.
- Menambahkan feedback untuk Enter, Back, klik rotary, warning SD/job, dan job selesai.
- Menambahkan alarm buzzer berulang setiap 5 detik saat Marlin tidak merespons.
- Memperbaiki awal perhitungan timeout Marlin agar kondisi belum pernah merespons dapat berubah dari `WAIT` menjadi `NO RESP`.
- Menambahkan indikator status WiFi dan MQTT pada area kosong standby screen.
- Menggunakan `V` untuk tersambung, `X` untuk mati/belum tersambung, dan `?` untuk error.
- Mengubah label ringkas `W`/`M` menjadi `WiFi`/`MQTT` dan menyejajarkan `WiFi` tepat di bawah koordinat Y.
- Memperbaiki Reset WiFi agar portal WiFiManager langsung dibuka tanpa toggle OFF/ON manual.
- Memperbaiki default broker dari `192.168.250.13` ke IPv4 laptop aktif `192.168.250.3`.
- Menambahkan migrasi otomatis Preferences dari alamat broker lama.
- Memperbaiki status koneksi agar kegagalan TCP probe tetap tampil `TIMEOUT`, bukan tertimpa `DISCONNECTED`.
- Memindahkan konfigurasi broker/topic MQTT dari `AppConfig.h` ke `src/config/MqttConfig.h`.
- Menambahkan publish retained `cnc/status` dengan Last Will `OFFLINE`.
- Menambahkan subscribe `cnc/command` dan `cnc/gcode` menggunakan PubSubClient.
- Menampilkan payload command dan G-code di Serial Monitor tanpa mengeksekusi atau meneruskan ke UART.
- Menambahkan publish `cnc/progress` dan `cnc/error` tanpa menghapus topic monitoring lama.
- Menjaga konfigurasi MQTT yang sudah bekerja tanpa mengubah topic monitoring.
- Mengaktifkan polling `M114` periodik untuk monitoring posisi Marlin.
- Memastikan posisi X/Y/Z dari respons Marlin dipakai oleh snapshot MQTT `cnc/position`.
- Mengubah alarm MQTT agar fokus pada status respons Marlin:
  - `ERROR / Marlin no response` saat Marlin timeout.
  - `INFO / OK` saat Marlin kembali normal.
- Menambahkan dokumentasi status project sesuai aturan `AGENTS.md`.
- Memperluas README dengan dokumentasi UI LCD: boot, standby, main menu, submenu, Select Job, Set Origin, Machine Ctrl&Status, Network, dan dialog konfirmasi.
- Memperbarui PROJECT_STATUS agar mencatat workflow UI yang sudah selesai.
- Menambahkan bagian Hardware ke README.
- Memperbaiki ARCHITECTURE.md dengan diagram ASCII yang valid dan sinkron dengan hardware/software project.

## v0.4.0

- Menambahkan MQTT monitoring lokal.
- Menambahkan dukungan Mosquitto broker lokal.
- Menambahkan monitoring lewat MQTT Explorer.
- Menambahkan sinkronisasi waktu NTP WIB.

## v0.3.0

- Menambahkan SD card browser.
- Menambahkan navigasi folder.

## v0.2.0

- Menambahkan LCD menu system.
- Menambahkan rotary encoder.
- Menambahkan push button input.

## v0.1.0

- Initial project structure.
