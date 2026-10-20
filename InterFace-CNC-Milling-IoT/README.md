# AYB Interface

**AYB Interface** adalah firmware antarmuka CNC milling berbasis ESP32-S3 untuk menghubungkan user, LCD 128x64, SD card, jaringan lokal, MQTT, dan controller CNC berbasis Marlin pada SKR V1.4 Turbo.

Firmware ini dikembangkan secara mandiri sebagai bagian dari proyek akhir D4 Teknik Elektronika Politeknik Negeri Malang.

- Pengembang: Alfath Yusuf Biyono
- NIM: 2141170132
- Instansi pada About: PT AUDIO YUDISTIRO BUANA
- Pembaruan firmware terakhir: 22/08/2026

Identitas firmware disimpan terpusat di `src/config/AppConfig.h` melalui `FIRMWARE_NAME`, `FIRMWARE_VERSION`, `FIRMWARE_DESCRIPTION`, dan `FIRMWARE_ORGANIZATION`. Identitas tersebut digunakan pada boot screen, Serial Monitor, dan menu About.

## Ringkasan Sistem

Alur kerja utama saat ini:

```text
Fusion 360 / CAM
      |
      v
File G-code di SD Card
      |
      v
ESP32-S3 CNC Interface
      |
      +-- LCD 128x64 + tombol + rotary encoder
      |
      +-- MQTT lokal ke Mosquitto / MQTT Explorer
      |
      +-- UART ke SKR V1.4 Turbo / Marlin
```

## Hardware

### Controller Interface

- ESP32-S3 DevKitM-1

### CNC Controller

- BTT SKR V1.4 Turbo
- Marlin Firmware

### Motion System

- DM542 Stepper Driver
- NEMA23 Stepper Motor

### User Interface

- LCD 128x64
- Rotary Encoder
- Push Button
- Passive buzzer pada GPIO 38

### Storage

- MicroSD Card

### Network

- WiFi
- MQTT Broker

## Fitur yang Sudah Ada

- Tampilan booting, standby, main menu, submenu, dan dialog konfirmasi.
- Navigasi menggunakan push button dan rotary encoder.
- Browser SD card untuk memilih file job.
- Tampilan job terpilih dan estimasi waktu di standby.
- Menu Work Origin untuk memilih sembilan workspace Marlin `G54`-`G59.3` dan menandai workspace aktif.
- Submenu `Perbarui Origin` dengan jog X/Y/Z, `G92 X0 Y0 Z0`, dan `M500` untuk menyimpan offset workspace aktif ke EEPROM/flash Marlin.
- Soft endstop Marlin dapat diaktifkan setelah pembaruan origin dengan `M211 S1`.
- Menu Machine Ctrl&Status untuk status alarm, test buzzer, spindle, feedrate, area kerja, soft endstop, dan sensor homing.
- Menu Network untuk WiFi, MQTT, broker, AP setup, waktu singkat, tanggal lengkap, status sinkronisasi, dan reset WiFi.
- WiFiManager untuk konfigurasi WiFi dan parameter MQTT lokal.
- Editor IP broker MQTT melalui menu Network > Broker dengan input digit `000.000.000.000`.
- Mode sementara `CNC_HEADLESS_WIFI_MQTT_DEV` untuk menjalankan WiFi/MQTT tanpa inisialisasi LCD saat pengembangan.
- Sinkronisasi waktu NTP WIB dan penggunaan waktu internal ESP32 setelah sinkron.
- MQTT monitoring lokal ke Mosquitto.
- Monitoring state mesin melalui topic retained `cnc/machine`.
- G-code sender file SD non-blocking dengan flow control respons `ok` Marlin.
- Estimasi waktu tersisa dari analisis gerakan G-code.
- Layar About tiga halaman dengan logo dan identitas pengembang.
- Feedback buzzer non-blocking untuk navigasi, job selesai, warning, dan alarm Marlin.

### Passive Buzzer

Passive buzzer menggunakan GPIO 38 dan PWM LEDC. Fungsinya:

- Enter dan klik rotary menghasilkan beep pendek bernada tinggi.
- Back menghasilkan beep pendek bernada tinggi agar lebih bersih pada passive buzzer.
- Aktivasi WiFi/MQTT dan Reset WiFi menghasilkan bunyi hasil sekali: sukses jika tersambung, warning jika gagal atau timeout.
- Saat WiFi aktif dan credential tersimpan, buzzer memberi bunyi loading pendek berulang seperti `_ _ _` selama mencoba auto-connect.
- Selama portal WiFiManager menunggu user menyambungkan perangkat ke AP setup, buzzer diam agar tidak mengganggu.
- Setelah Reset WiFi, portal langsung dibuka tanpa bunyi loading karena user memang perlu mengisi ulang jaringan.
- Job selesai menghasilkan pola tiga nada.
- SD card atau job yang tidak siap menghasilkan pola warning.
- Koneksi Marlin yang sebelumnya aktif lalu terputus, atau Marlin yang mengirim error, menghasilkan pola alarm tiga beep tajam dengan jeda sangat pendek yang langsung berulang selama error aktif.
- Item `Test Buzzer` di Machine Ctrl&Status memutar pola alarm satu kali untuk memastikan buzzer terdengar.
- Putaran rotary dan tombol jog tidak menghasilkan bunyi agar navigasi dan jog tetap responsif.

Pola bunyi dijalankan tanpa `delay()`, sehingga tidak menghentikan pembacaan tombol, rotary, UART Marlin, maupun MQTT. Gunakan transistor driver jika arus buzzer melebihi kemampuan aman GPIO ESP32-S3.

Nada dan tempo tiap pola dapat diatur di `src/output/BuzzerHandler.cpp` pada tabel `SELECT_STEPS`, `BACK_STEPS`, `SUCCESS_STEPS`, `LOADING_STEPS`, `WARNING_STEPS`, `JOB_FINISHED_STEPS`, dan `ALARM_STEPS`. Nilai `frequency` adalah frekuensi PWM dalam Hz, nilai `durationMs` adalah durasi step, dan `frequency` `0` berarti jeda/senyap. Jarak antar bunyi loading WiFi diatur dari `AppConfig::WIFI_LOADING_BEEP_INTERVAL_MS`; pengulangan alarm Marlin diatur dari `AppConfig::BUZZER_ALARM_REPEAT_MS`.

## User Interface LCD

UI utama menggunakan LCD 128x64 berbasis U8g2. Alur tampilan dibuat bertahap agar user dapat mengoperasikan mesin tanpa PC setelah file G-code tersedia di SD card.

### Mode Sementara Tanpa LCD

Jika LCD sedang bermasalah dan pengujian hanya fokus ke WiFi/MQTT, firmware dapat dijalankan dengan toggle `CNC_HEADLESS_WIFI_MQTT_DEV` di `src/main.cpp`. Saat toggle ini aktif, firmware tetap membuka Serial Monitor, buzzer, MicroSD, WiFi, dan MQTT, tetapi tidak menjalankan inisialisasi LCD serta tidak mengaktifkan input tombol/rotary encoder.

Mode ini hanya untuk prosedur pengembangan jaringan. Untuk kembali ke workflow interface normal, komentari kembali:

```cpp
// #define CNC_HEADLESS_WIFI_MQTT_DEV
```

Pada mode ini, konfigurasi WiFiManager masih bisa memakai credential dan parameter broker MQTT yang sudah tersimpan. Jika credential belum ada atau salah, portal WiFiManager tetap dibuka oleh firmware, tetapi instruksi AP tidak tampil di LCD sehingga pantau Serial Monitor.

### Boot Screen

Saat sistem menyala, LCD menampilkan boot screen berisi identitas interface dan area logo. Tampilan ini dipakai sebagai indikator awal bahwa ESP32, LCD, dan firmware sudah mulai berjalan.

### Standby Screen

Mode standby adalah tampilan utama setelah boot. Informasi yang ditampilkan:

- Status mesin.
- Status koneksi CNC dan aktivitas kerja pada baris atas, misalnya `CNC:[X] | OFF`, `CNC:[OK] | IDLE`, atau `CNC:[OK] | RUN`.
- Waktu lokal singkat `HH:MM` dari sinkronisasi NTP/internal ESP32.
- Koordinat X, Y, dan Z.
- Nama job yang sedang dipilih dari SD card.
- Estimasi waktu kerja jika tersedia.
- Status komunikasi Marlin secara ringkas.
- Indikator lengkap `WiFi` dan `MQTT` di area bawah koordinat Y.

Format indikator koneksi:

- `WiFi:[OK]` berarti WiFi terhubung dan `WiFi:[X]` berarti tidak terhubung/nonaktif.
- `MQTT:[OK]` berarti MQTT terhubung ke broker dan `MQTT:[X]` berarti tidak terhubung/nonaktif.

Indikator tampil pada satu baris khusus di bawah koordinat dengan format `WiFi:[...] MQTT:[...]`. Font tetap `6x10`, sama seperti koordinat, dan indikator tetap terlihat saat progress job tampil. Saat file job sudah dipilih, garis horizontal memisahkan baris koneksi dari nama job dan estimasi agar informasi lebih mudah dibaca.

Nilai koordinat X/Y/Z hanya dianggap valid setelah respons `M114` diterima saat CNC berstatus `CONNECTED`. Jika mesin belum tersambung, standby menampilkan `?` untuk setiap koordinat.

Status baris atas standby sengaja dipisah menjadi dua bagian. Bagian kiri `CNC:[OK]` atau `CNC:[X]` menjelaskan apakah interface benar-benar mendapat respons dari Marlin/SKR. Bagian kanan menjelaskan aktivitas kerja mesin. Saat Marlin belum tersambung aktivitas tampil `OFF`; saat Marlin tersambung tetapi tidak menjalankan job tampil `IDLE`; saat job berjalan tampil `RUN`; saat pause tampil `PAUS`; dan saat error tampil `ERR`. Pemisahan ini mencegah status koneksi disamakan dengan status pekerjaan.

### Main Menu

Menu utama berisi:

- `Move & Jog`
- `Select Job`
- `Machine Ctrl&Status`
- `Network`
- `About`

Navigasi menu menggunakan rotary encoder. Klik rotary atau tombol Enter menjalankan item yang dipilih.

### Submenu Move & Jog

Submenu ini mengikuti urutan persiapan bidang kerja:

1. `Home All (G28)` untuk mencari referensi home mesin.
2. `Work Origin` untuk memilih origin tersimpan `G54`-`G59.3` atau menyimpan origin baru.

### Home All

`Home All (G28)` membuka dialog konfirmasi sebelum mengirim `G28`. Perintah hanya dikirim jika komunikasi CNC berstatus `CONNECTED` dan fitur Home aktif. Home All menggerakkan mesin menuju sensor home dan tidak menetapkan origin material.

### Work Origin

Menu Work Origin menyediakan sembilan slot workspace: `G54`, `G55`, `G56`, `G57`, `G58`, `G59`, `G59.1`, `G59.2`, dan `G59.3`. Setiap slot membuka submenu `Pilih Origin` dan `Perbarui Origin`.

- `Pilih Origin` membuka konfirmasi `Gunakan G54?` dan hanya mengaktifkan offset tersimpan tanpa menggerakkan spindle.
- `Perbarui Origin` mengaktifkan slot tersebut lalu membuka layar jog untuk menetapkan dan menyimpan ulang origin.

Pemilihan maupun pembaruan workspace hanya diizinkan setelah `Home All`. Membuka layar `Perbarui Origin` belum menimpa data. Penyimpanan baru terjadi ketika operator menekan Enter dan menyetujui dialog bertajuk workspace aktif, misalnya `G54 ORIGIN`, dengan pesan `Perbarui origin?`.

Kontrol yang digunakan:

- Tombol X/Y/Z short press mengubah nilai `+/-1`.
- Tombol X/Y/Z long press mengubah nilai `+/-10`.
- Rotary encoder memilih axis.
- Putar rotary mengubah nilai axis terpilih.
- Tekan dan putar rotary mengubah nilai lebih besar.
- Tombol Enter membuka konfirmasi penyimpanan origin.

Saat konfirmasi `Yes`, firmware mengirim perintah:

```gcode
M400
G92 X0 Y0 Z0
M500
M211 S1
```

`G92` memperbarui offset slot workspace aktif, lalu `M500` menyimpan tabel `G54`-`G59.3` secara permanen pada Marlin. `M211 S1` dikirim jika fitur soft endstop setelah pembaruan origin aktif di konfigurasi. Penyimpanan hanya dilakukan ketika job tidak aktif.

Setelah mesin dinyalakan ulang atau setelah `G28`, jalankan `Home All`, buka slot yang sama, lalu pilih `Pilih Origin`. Interface mengirim `G53` lalu `G54` agar offset tersimpan dimuat ulang dengan pasti. Jika posisi fixture atau panjang tool berubah, gunakan `Perbarui Origin` sebelum menjalankan job.

Semua dialog konfirmasi mengikuti aturan UI yang sama: tombol Enter menjalankan `Yes`, tombol Back menjalankan `No`, putaran rotary memindahkan kursor `Yes/No`, dan klik rotary menjalankan pilihan yang sedang disorot. Pilihan awal tetap `No` sebagai kondisi aman.

### Select Job

Menu Select Job langsung membuka browser SD card. Fitur yang tersedia:

- Menampilkan label SD card jika terbaca.
- Menampilkan path folder saat ini.
- Folder ditampilkan di atas file.
- Folder dan file diurutkan berdasarkan abjad.
- File yang dipilih disimpan sebagai job aktif.
- Enter atau klik rotary membuka folder atau mengonfirmasi file.
- Back kembali ke folder induk, lalu kembali ke menu utama dari root SD card.

Job aktif ditampilkan kembali di standby screen sebagai `Job: nama_file`.

Setelah file dipilih, firmware menganalisis G-code secara bertahap agar tombol, LCD, MQTT, dan UART tetap responsif. Workflow menjalankan job:

1. Jalankan `Home All`.
2. Buka slot workspace, misalnya `G54`, lalu gunakan `Pilih Origin` atau `Perbarui Origin`.
3. Pilih file melalui `Select Job`.
4. Tunggu status job `READY` dan estimasi selesai dihitung.
5. Tahan tombol `X+ / PLAY` selama 1 detik, lalu konfirmasi Start.

Pemilihan file hanya membaca dan menganalisis G-code sehingga tidak membatalkan status Home maupun Work Origin. Dengan demikian operator dapat memakai urutan `Home All -> G54 -> Pilih Origin -> Select Job -> Start`. Interlock tetap dihapus setelah homing ulang, job selesai, controlled stop, error, SD dilepas, reset, atau koneksi Marlin terputus.

Untuk memeriksa apakah origin tersimpan menunjuk lokasi yang benar, salin `examples/gcode/CEK_ORIGIN_XY.gcode` ke MicroSD. Jalankan hanya tanpa mata pahat dan tanpa benda yang menghalangi lintasan. File tidak memilih atau mengubah workspace: operator tetap harus menjalankan `Home All`, memilih slot melalui `Pilih Origin`, memilih file cek, lalu Start. Program mematikan spindle, mempertahankan Z di posisi home, bergerak pelan ke `X0 Y0` origin aktif, menunggu 10 detik untuk pemeriksaan visual, lalu menjalankan `G28` dan `M5`.

Kontrol job saat berjalan:

- `X+ / PLAY`: tahan untuk Start saat `READY`; tekan singkat untuk Resume saat `PAUSED`.
- `X- / PAUSE`: meminta pause setelah command aktif mendapat respons `ok`.
- `Z- / STOP`: tahan 1,5 detik untuk controlled stop dan mengirim `M5`.

Stop tersebut bukan emergency stop. Emergency stop harus tetap menggunakan rangkaian/tombol hardware khusus.

State job yang tersedia adalah `IDLE`, `ANALYZING`, `READY`, `STARTING`, `RUNNING`, `PAUSING`, `PAUSED`, `STOPPING`, `COMPLETING`, `COMPLETED`, `STOPPED`, dan `ERROR`.

Estimasi LCD adalah waktu tersisa dengan format `HH:MM:SS`. Perhitungan membaca jarak dan feedrate `G0/G1`, arc XY `G2/G3`, dwell `G4`, unit `G20/G21`, mode `G90/G91`, dan perubahan koordinat `G92`. Estimasi belum memasukkan akselerasi, deselerasi, pergantian tool, waktu spindle mencapai RPM, atau perilaku khusus macro sehingga harus dianggap perkiraan.

### Machine Ctrl&Status

Menu ini digunakan untuk melihat dan mengatur status mesin yang berhubungan dengan Marlin. Urutan item aktual dan fungsinya:

1. `CNC`: menampilkan state komunikasi dan menjalankan pembacaan ulang status ketika dipilih.
2. `Alarm`: menampilkan kondisi alarm sistem `OK` atau `ERROR`.
3. `Test Buzzer`: memutar pola alarm buzzer satu kali tanpa mengirim command ke Marlin.
4. `Home X/Y/Z`: membaca sensor endstop melalui `M119`.
5. `SoftEnd`: membaca status soft endstop melalui `M211`.
6. `Spindle`: membuka konfirmasi untuk mengirim `M3` atau `M5`.
7. `Feed XY`: membaca/mengubah feedrate maksimum X/Y melalui `M203`.
8. `Feed Z`: membaca/mengubah feedrate maksimum Z melalui `M203`.
9. `Area X/Y/Z`: menampilkan geometry report yang dibaca melalui `M115`, dengan fallback konfigurasi lokal jika geometry report belum terbaca.
10. `Refresh Status`: membaca ulang seluruh parameter status yang didukung.

Urutan tersebut memprioritaskan pemeriksaan komunikasi, alarm, dan sensor keselamatan sebelum kontrol spindle serta parameter gerak.

Item `Alarm` memakai sumber status yang sama dengan buzzer dan MQTT alarm. Status menjadi `ERROR` jika koneksi Marlin yang sebelumnya aktif berubah `LOST`, Marlin mengirim `ERROR`, atau job sender masuk state `ERROR`. Status `OFF` atau `DISCONNECTED` sebelum Marlin pernah tersambung tetap dibaca melalui item `CNC`, bukan dianggap alarm.

`Refresh Status` terlebih dahulu menandai feedrate, area kerja, soft endstop, dan sensor home sebagai belum diketahui. Jika CNC berstatus `CONNECTED`, interface kemudian meminta:

- `M203` untuk feedrate maksimum X/Y/Z.
- `M115` untuk firmware/geometry report.
- `M119` untuk sensor home/endstop.
- `M211` untuk soft endstop.

Membuka menu Machine Ctrl&Status tidak lagi memaksa refresh status karena parameter sudah diprefetch setelah boot. Item `Refresh Status` tetap tersedia untuk meminta ulang parameter secara manual. Refresh tidak menjalankan homing, tidak menggerakkan axis, tidak mengubah origin, dan tidak me-restart Marlin. Posisi `M114` tetap diperbarui oleh polling periodik terpisah. Jika CNC belum terhubung, nilai yang belum diketahui tetap tampil `?`.

Saat koneksi Marlin diaktifkan, interface langsung mengisi fallback area dari `AppConfig::MACHINE_WORK_AREA_DEFAULT_X/Y/Z_MM` agar LCD dan dashboard tidak kosong setelah boot. Jika respons `M115` kemudian memuat geometry report, nilai fallback diganti dengan data Marlin. Perubahan nilai `Area X/Y/Z` saat ini masih hanya mengubah nilai lokal pada interface; belum ada command Marlin yang menerapkan perubahan area kerja tersebut.

Status sensor homing memakai format seperti `OPEN`, `TRIGGER`, atau `?` jika belum diketahui.

### Network

Menu Network digunakan untuk konfigurasi dan pemantauan koneksi:

- `WiFi`: mengaktifkan atau menonaktifkan WiFi melalui konfirmasi.
- `SSID`: menampilkan nama jaringan yang terhubung.
- `IP`: menampilkan alamat IP ESP32.
- `MQTT`: mengaktifkan atau menonaktifkan MQTT; WiFi harus aktif.
- `Link`: menampilkan kondisi koneksi MQTT dan broker aktif.
- `Broker`: membuka editor alamat IP broker MQTT.
- `AP`: menampilkan nama access point konfigurasi WiFiManager.
- `Waktu`: menampilkan jam internal ESP32 dengan format `HH:MM`.
- `Tanggal`: menampilkan kalender lengkap internal ESP32 dengan format `DD/MM/YYYY`.
- `Sync`: meminta sinkronisasi NTP jika WiFi terhubung.
- `Reset WiFi`: menghapus credential lama dan membuka kembali portal konfigurasi setelah konfirmasi.

Saat WiFi setup aktif, LCD menampilkan instruksi untuk menyambungkan perangkat ke AP `CNC-Interface-Setup`.

Item `Broker` menyediakan input IP broker langsung dari LCD dengan format tetap `000.000.000.000`. Contoh broker `192.168.250.9` diisi sebagai `192.168.250.009`.
Digit aktif ditampilkan dengan highlight negatif seperti item menu agar posisi input lebih mudah terlihat.

Kontrol editor broker:

- Putar rotary encoder untuk memilih angka `0` sampai `9` pada digit aktif.
- Klik rotary encoder atau tekan Enter untuk maju ke digit kanan.
- Tekan Back untuk mundur ke digit kiri.
- Jika Back ditekan pada digit pertama, editor batal dan kembali ke menu Network.
- Saat digit terakhir sudah diisi, klik rotary atau tekan Enter membuka konfirmasi penggantian broker.

Firmware memvalidasi setiap oktet agar tidak lebih dari `255`. Setelah konfirmasi `Yes`, broker disimpan ke Preferences dalam bentuk IP normal tanpa padding nol, misalnya `192.168.250.9`, lalu konfigurasi MQTT diterapkan ulang. Jika WiFi dan MQTT sedang aktif, koneksi MQTT akan diputus dan dibuka kembali memakai broker baru.

Redraw editor broker dibatasi oleh `AppConfig::BROKER_EDITOR_REDRAW_MS` agar putaran rotary tetap responsif dan LCD tidak berkedip berlebihan saat digit diputar cepat. Log event tombol/rotary per tick juga default nonaktif melalui `AppConfig::ENABLE_INPUT_EVENT_LOG`. Saat editor broker aktif, proses connect/reconnect MQTT dijeda sementara agar broker lama atau broker yang belum benar tidak menahan loop input.

Saat `Reset WiFi` dikonfirmasi, credential lama dihapus dan portal `CNC-Interface-Setup` langsung dibuka kembali. User tidak perlu mematikan dan menyalakan toggle WiFi secara manual.

### About

Menu About menggunakan tiga halaman yang berganti otomatis setiap 3 detik:

1. Logo firmware, nama `AYB Interface`, dan versi firmware.
2. Nama pembuat `Alfath Yusuf Biyono`, NIM `2141170132`, dan `PT AUDIO YUDISTIRO BUANA`.
3. Tanggal pembaruan firmware terakhir dan versi firmware.

Setiap halaman menampilkan nomor halaman agar urutannya jelas. Layar About dapat ditutup menggunakan tombol Enter, tombol Back, atau klik rotary encoder. Pergantian halaman menggunakan `millis()` dan tidak menghentikan loop utama.

### Dialog Konfirmasi

Dialog konfirmasi menggunakan aturan konsisten:

- Tombol Enter fisik selalu `Yes`.
- Tombol Back fisik selalu `No`.
- Putar rotary memilih `Yes` atau `No`.
- Klik rotary menjalankan pilihan yang disorot.

## MQTT Lokal

Konfigurasi default broker dan topic berada di `src/config/MqttConfig.h`. Nilai broker yang sudah disimpan melalui Preferences/WiFiManager tetap dapat menggantikan nilai default saat runtime.

Default broker lokal saat ini adalah `192.168.250.7:1883`, sesuai IPv4 laptop saat pengujian. Sebaiknya laptop diberi DHCP reservation/static IP atau broker diperbarui melalui WiFiManager jika alamat laptop berubah.

Topic utama yang digunakan:

- `cnc/status` - publish status koneksi ESP32 ke broker.
- `cnc/command` - subscribe command sederhana dan tampilkan payload di Serial Monitor.
- `cnc/gcode` - subscribe baris G-code dan tampilkan payload di Serial Monitor.
- `cnc/progress` - publish nama file, progress byte yang sudah diakui Marlin, dan state job.
- `cnc/error` - publish status error aktif.
- `cnc/position` - publish posisi X/Y/Z dari Marlin.
- `cnc/machine` - publish state CNC, spindle, Work Origin, soft endstop, feedrate, dan sensor home.
- `cnc/file/start` - subscribe awal transfer file G-code dari Node-RED.
- `cnc/file/chunk` - subscribe potongan file G-code berurutan dari Node-RED.
- `cnc/file/finish` - subscribe akhir transfer file G-code dari Node-RED.
- `cnc/file/ack` - publish ACK untuk START, CHUNK, dan FINISH.
- `cnc/file/list/request` - subscribe permintaan daftar file job MicroSD dari Node-RED.
- `cnc/file/list` - publish daftar file job MicroSD dalam paket begin, item, end, atau error.
- `cnc/file/select` - subscribe permintaan validasi pilihan file MicroSD dari Node-RED.
- `cnc/file/select/ack` - publish hasil validasi pilihan file MicroSD.
- `cnc/file/delete` - subscribe permintaan hapus file MicroSD dari Node-RED.
- `cnc/file/delete/ack` - publish hasil hapus file MicroSD.
- `cnc/sd/status/request` - subscribe permintaan status MicroSD.
- `cnc/sd/status` - publish status MicroSD aktual.

Topic monitoring lama tetap dipertahankan agar dashboard yang sudah ada tidak rusak:

- `cnc/network`
- `cnc/time`
- `cnc/alarm`

Payload dikirim dalam format JSON.

Contoh `cnc/status` saat terhubung:

```json
{
  "connected": true,
  "state": "ONLINE"
}
```

Jika koneksi MQTT terputus atau Last Will aktif, field `state` pada `cnc/status` dikirim sebagai `OFFLINE`. Nilai status koneksi pada topic ini sengaja memakai huruf kapital agar lebih mudah dibaca di dashboard.

Contoh `cnc/position`:

```json
{
  "x": 0.0,
  "y": 0.0,
  "z": 0.0,
  "display": {"x": "0.00", "y": "0.00", "z": "0.00"},
  "unit": "mm",
  "valid": true,
  "source": "marlin"
}
```

Saat Marlin belum memberikan posisi nyata:

```json
{
  "x": null,
  "y": null,
  "z": null,
  "display": {"x": "?", "y": "?", "z": "?"},
  "unit": "mm",
  "valid": false,
  "source": "unavailable"
}
```

Contoh `cnc/machine` ketika komunikasi CNC dinonaktifkan:

```json
{
  "state": "OFF",
  "connected": false,
  "activity": "OFF",
  "spindle": "UNKNOWN",
  "soft_endstop": "UNKNOWN",
  "feed_xy": null,
  "feed_z": null,
  "area_x": 1250,
  "area_y": 2600,
  "area_z": 140,
  "area_source": "fallback",
  "work_origin": "NONE",
  "work_origin_valid": false,
  "x_home": "UNKNOWN",
  "y_home": "UNKNOWN",
  "z_home": "UNKNOWN",
  "work_area": {"x": 1250, "y": 2600, "z": 140, "valid": true, "source": "fallback"},
  "home": {"x": "UNKNOWN", "y": "UNKNOWN", "z": "UNKNOWN"},
  "home_valid": {"x": false, "y": false, "z": false},
  "home_note": {"x": "not_connected", "y": "not_connected", "z": "not_connected"}
}
```

Field `cnc/machine.state` berisi status koneksi Marlin seperti `OFF`, `WAITING`, `DISCONNECTED`, `CONNECTED`, `LOST`, atau `ERROR`. Field `cnc/machine.connected` bernilai `true` hanya saat `state` adalah `CONNECTED`. Field `cnc/machine.activity` berisi kondisi kerja mesin: `OFF` ketika CNC belum tersambung, `IDLE` ketika CNC tersambung tetapi tidak menjalankan job, `RUNNING` ketika job aktif, `PAUSED` ketika job dijeda, `STOPPING` ketika controlled stop berjalan, dan `ERROR` saat job atau koneksi Marlin error.

Status sensor home/endstop dikirim pada `cnc/machine` melalui field kompatibel lama `x_home`, `y_home`, dan `z_home`, serta field terstruktur `home`, `home_valid`, dan `home_note` untuk dashboard. Nilai sensor yang valid adalah `OPEN` dan `TRIGGERED`. Nilai `UNKNOWN` berarti data belum valid, misalnya Marlin belum tersambung, respons `M119` belum diterima, atau jalur sensor perlu dicek. Field `home_note` berisi `ok`, `not_connected`, atau `not_loaded` agar dashboard dapat membedakan sensor open dengan data yang belum terbaca.

Area kerja mesin dikirim pada `cnc/machine` melalui field kompatibel `area_x`, `area_y`, `area_z`, dan `area_source`, serta field terstruktur `work_area`. Nilai `area_source`/`work_area.source` bernilai `fallback` jika berasal dari konfigurasi lokal interface dan berubah menjadi `marlin` setelah geometry report `M115` berhasil diparse.

Work Origin aktif dikirim melalui `work_origin`, misalnya `G54`, dan `work_origin_valid`. Nilai valid hanya `true` setelah Home All selesai diperintahkan dan workspace dipilih melalui interface. Setelah `G28`, reset, job selesai, stop, error, SD dilepas, atau koneksi Marlin terputus, interface mengirim `work_origin:"NONE"` dan `work_origin_valid:false`. Perubahan workspace yang dikirim dari software lain seperti Repetier Host tidak dapat diketahui interface sampai workspace dipilih ulang dari menu interface.

Contoh `cnc/machine` saat sensor sudah terbaca sebagian:

```json
{
  "state": "CONNECTED",
  "connected": true,
  "activity": "IDLE",
  "area_x": 1250,
  "area_y": 2600,
  "area_z": 140,
  "area_source": "marlin",
  "work_origin": "G54",
  "work_origin_valid": true,
  "work_area": {"x": 1250, "y": 2600, "z": 140, "valid": true, "source": "marlin"},
  "home": {"x": "OPEN", "y": "TRIGGERED", "z": "UNKNOWN"},
  "home_valid": {"x": true, "y": true, "z": false},
  "home_note": {"x": "ok", "y": "ok", "z": "not_loaded"}
}
```

Contoh `cnc/progress` saat file selesai dianalisis dan siap dijalankan:

```json
{
  "file": "contoh.nc",
  "progress": 0,
  "state": "READY"
}
```

Jika belum ada file yang dipilih, field `file` berisi `Belum dipilih`, progress `0`, dan state `IDLE`. Pemilihan file tidak otomatis menjalankan mesin. Saat sender aktif, progress hanya bertambah setelah Marlin mengirim `ok` untuk command yang dikirim.

### Pemetaan Node-RED Dashboard 2.0

- Status Sistem: `cnc/status.connected`, `cnc/status.state`, `cnc/machine.connected`, `cnc/machine.state`, dan `cnc/machine.activity`.
- Posisi Mesin: `cnc/position.x/y/z` serta `cnc/machine.x_home/y_home/z_home` atau `cnc/machine.home.x/y/z`.
- Status Pekerjaan: `cnc/progress.file`, `cnc/progress.progress`, dan `cnc/progress.state`.
- Work Origin: `cnc/machine.work_origin` dan `cnc/machine.work_origin_valid`.
- Status Spindle: `cnc/machine.spindle`.
- Parameter Gerak: `cnc/machine.feed_xy`, `cnc/machine.feed_z`, `cnc/machine.soft_endstop`, `cnc/machine.area_x/y/z`, dan `cnc/machine.work_area.x/y/z/source`.
- Informasi Jaringan: `cnc/network.ssid/ip/mqtt` serta `cnc/time.time/date/sync`.
- Status Alarm: `cnc/alarm.level/message/active/popup/source/code` serta `cnc/error.active/message/source/code`.

Seluruh payload monitoring di atas dipublish retained agar dashboard memperoleh nilai terakhir setelah reconnect. Topic `cnc/command` dan `cnc/gcode` tetap receive-only dan tidak dieksekusi.

Untuk pop-up alarm Node-RED, gunakan `cnc/alarm` sebagai sumber utama. Field `popup:true` menandakan dashboard boleh memunculkan dialog atau toast, `level` membedakan `ERROR` dan `WARNING`, `source` menunjukkan asal masalah, dan `code` dipakai untuk logika flow. Karena `cnc/alarm` bersifat retained, flow dashboard sebaiknya memunculkan pop-up hanya saat payload berubah atau saat transisi `popup:false` ke `popup:true`, bukan setiap dashboard dibuka.

Untuk indikator hidup/mati interface, dashboard harus menjadikan `cnc/status.connected` sebagai sumber utama. Jika `cnc/status.connected` bernilai `false` atau `cnc/status.state` bernilai `OFFLINE`, tampilan `Status MQTT` di dashboard sebaiknya dipaksa menjadi `DISCONNECTED`, walaupun nilai retained terakhir pada `cnc/network.mqtt` masih `CONNECTED`.

Firmware akan mengirim `cnc/network.mqtt` bernilai `DISCONNECTED` saat MQTT dimatikan secara normal dari menu atau sebelum disconnect terkontrol. Namun jika ESP32 mati mendadak atau kehilangan supply, hanya Last Will pada `cnc/status` yang dijamin dikirim oleh broker. Karena itu `cnc/network` diperlakukan sebagai informasi jaringan terakhir, bukan penentu utama apakah interface masih online.

Dashboard Node-RED untuk progres skripsi dibuka melalui:

```text
http://127.0.0.1:1880/dashboard/cnc
```

Alamat `127.0.0.1` hanya berlaku dari laptop/PC yang menjalankan Node-RED. Jika dashboard dibuka dari perangkat lain dalam jaringan yang sama, gunakan IP laptop/PC Node-RED, misalnya:

```text
http://<IP-LAPTOP>:1880/dashboard/cnc
```

Jalur simulasi posisi MQTT sudah disiapkan melalui `MqttConfig::ENABLE_POSITION_SIMULATION`, tetapi default tetap `false` agar dashboard tidak menerima data palsu. Aktifkan hanya untuk pengujian terkontrol tanpa Marlin.

`cnc/command` dan `cnc/gcode` pada tahap ini bersifat receive-only. Payload hanya dicetak ke Serial Monitor dengan prefix `[MQTT][COMMAND]` atau `[MQTT][GCODE]`. Firmware tidak mengeksekusi payload dan tidak meneruskannya ke UART Marlin.

### Upload File G-code via MQTT

Firmware menerima protokol transfer file G-code dari Node-RED dan menyimpannya ke MicroSD melalui file sementara:

1. Node-RED publish `cnc/file/start`.
2. ESP32-S3 memvalidasi SD, metadata, nama file, ekstensi, ukuran, dan jumlah chunk.
3. ESP32-S3 membuka file sementara, misalnya `/TES2.gcode.part`.
4. ESP32-S3 membalas `cnc/file/ack` dengan ACK START `ready`.
5. Node-RED publish `cnc/file/chunk` berisi `chunkIndex` berurutan mulai dari `0` dan data chunk.
6. ESP32-S3 menulis chunk ke file sementara, lalu membalas ACK CHUNK hanya jika seluruh byte berhasil ditulis.
7. Node-RED publish `cnc/file/finish`.
8. ESP32-S3 memeriksa jumlah chunk, total byte tertulis, ukuran file sementara di MicroSD, lalu rename file sementara menjadi file final.
9. ESP32-S3 membalas ACK FINISH `saved` hanya setelah file final tersedia.

Payload START memakai `fileName` sebagai field utama, tetapi firmware masih menerima `filename` sebagai fallback. Field wajib START adalah `fileName`, `fileSize`, `chunkSize`, dan `totalChunks`. Field opsional `overwrite` default `false`. Payload CHUNK memakai `chunkIndex` dan data teks chunk pada field `data`; firmware juga menerima fallback `content`, `chunk`, atau `payload` jika flow Node-RED sudah memakai nama tersebut.

Contoh ACK START:

```json
{"type":"start","status":"ready","fileName":"TES2.gcode","fileSize":189,"totalChunks":2}
```

Contoh ACK CHUNK:

```json
{"type":"chunk","status":"received","chunkIndex":0}
```

Contoh ACK FINISH berhasil:

```json
{"type":"finish","status":"saved","fileName":"TES2.gcode","path":"/TES2.gcode","size":189,"totalChunks":2}
```

Contoh ACK error:

```json
{"type":"finish","status":"error","reason":"size_mismatch","fileName":"TES2.gcode"}
```

Reason error upload yang tersedia adalah `sd_not_ready`, `invalid_payload`, `invalid_filename`, `unsupported_extension`, `transfer_busy`, `no_active_transfer`, `open_failed`, `invalid_chunk`, `unexpected_chunk`, `write_failed`, `incomplete_transfer`, `size_mismatch`, dan `rename_failed`.

Jika target sudah tersedia dan `overwrite` tidak dikirim atau bernilai `false`, START ditolak dengan `reason:"file_exists"` dan file lama tidak disentuh. Jika `overwrite:true`, firmware tetap menulis ke `.part`, memverifikasi seluruh transfer, lalu mengganti file final setelah FINISH. Penggantian memakai backup sementara `.bak`; jika rename final gagal, firmware mencoba memulihkan file lama.

Jika transfer gagal, firmware menutup file sementara yang masih terbuka, menghapus file `.part`, membersihkan state transfer, dan tidak membuat file G-code parsial sebagai file final. Retry CHUNK terakhir akibat QoS 1 tetap dibalas ulang tanpa menulis data dua kali.

### Status dan Hot-Swap MicroSD

MicroSD diperiksa secara non-blocking dengan interval internal sehingga kartu dapat dipasang setelah ESP32-S3 menyala. Jika kartu atau modul SD belum terpasang, retry mount otomatis diberi jeda 30 detik agar percobaan `SD.begin()` tidak membuat tombol/rotary lambat. Build ESP32 memakai `CORE_DEBUG_LEVEL=0` agar Serial Monitor tidak dipenuhi log internal Arduino core seperti `sd_diskio.cpp` saat SD sengaja dilepas. Ketika kartu dilepas, firmware membatalkan upload aktif, membersihkan daftar file UI, membatalkan pilihan job lokal dari SD, dan mengirim status SD terbaru jika MQTT tersambung.

Node-RED dapat meminta status melalui `cnc/sd/status/request`. Firmware mengirim ke `cnc/sd/status`:

```json
{"type":"sd_status","status":"ready","mounted":true,"fileCount":7}
{"type":"sd_status","status":"absent","mounted":false,"fileCount":0,"reason":"sd_not_ready"}
{"type":"sd_status","status":"error","mounted":false,"fileCount":0,"reason":"mount_failed"}
```

### Hapus File MicroSD via MQTT

Dashboard dapat menghapus file job dengan publish ke `cnc/file/delete`:

```json
{"filePath":"/TES2.gcode"}
```

ACK berhasil:

```json
{"type":"file_delete_ack","status":"deleted","filePath":"/TES2.gcode"}
```

ACK gagal:

```json
{"type":"file_delete_ack","status":"error","filePath":"/TES2.gcode","reason":"file_not_found"}
```

Delete hanya menghapus file berekstensi `.gcode`, `.gco`, `.gc`, `.nc`, atau `.tap`. Firmware menolak path kosong, path berisi `..`, target folder, file `.part`, file yang sedang dipakai job lokal atau menunggu konfirmasi lokal, dan delete saat upload aktif. Delete tidak memilih job lain, tidak menjalankan G-code, dan tidak mengirim perintah ke Marlin.

Tahap ini hanya menyimpan file hasil upload ke MicroSD. File yang sudah tersimpan belum otomatis menjadi job aktif LCD, belum diteruskan ke `GcodeJobController`, belum dikirim ke UART Marlin, dan belum dieksekusi. Buffer MQTT di `CloudMqtt` tetap 2048 byte sehingga aman untuk CHUNK sekitar 128 byte beserta wrapper JSON.

### Daftar File Job MicroSD via MQTT

Node-RED dapat meminta daftar file job yang sudah ada di MicroSD melalui:

```json
{
  "type": "list_files"
}
```

Payload tersebut dikirim ke topic `cnc/file/list/request`. Firmware kemudian memindai MicroSD dari root `/` secara rekursif melalui `SdCardReader`, bukan dari `CloudMqtt`. Pemindaian hanya berjalan saat ada request MQTT, dibatasi maksimal 8 tingkat folder dan 50 file, mengabaikan file/folder tersembunyi atau sistem, serta hanya menerima ekstensi `.gcode`, `.gco`, `.gc`, `.nc`, dan `.tap` tanpa membedakan huruf besar/kecil.

Hasil dikirim ke `cnc/file/list` dalam beberapa paket agar tetap aman untuk buffer MQTT 2048 byte:

```json
{"type":"file_list_begin","status":"ok","count":1}
{"type":"file_list_item","index":0,"label":"Latihan/TES1.gcode","path":"/Latihan/TES1.gcode","size":1250}
{"type":"file_list_end","count":1}
```

Jika MicroSD belum siap, firmware mengirim:

```json
{"type":"file_list_error","status":"sd_not_ready","count":0}
```

`label` dipakai untuk tampilan dropdown Node-RED dan tidak memakai `/` di awal. `path` adalah nilai internal firmware dan selalu memakai `/` di awal. Fitur ini hanya mengirim daftar file yang sudah ada; tidak menyimpan upload MQTT ke SD card, tidak memilih job otomatis, tidak mengirim G-code ke Marlin, dan tidak menjalankan mesin.

### Pemilihan File MicroSD via MQTT

Setelah dropdown Node-RED menerima daftar file, dashboard dapat mengirim pilihan file ke topic `cnc/file/select`:

```json
{
  "type": "select_file",
  "path": "/1/2/3/TEST2.nc"
}
```

Topic ini dipertahankan hanya untuk kompatibilitas flow lama. Firmware sekarang selalu menolak remote selection agar dashboard tidak dapat menentukan job CNC.

Jika valid, firmware mengirim ACK ke `cnc/file/select/ack`:

```json
{
  "type": "file_select_ack",
  "status": "ok",
  "label": "1/2/3/TEST2.nc",
  "path": "/1/2/3/TEST2.nc",
  "size": 1234
}
```

Jika gagal, firmware mengirim:

```json
{
  "type": "file_select_ack",
  "status": "error",
  "reason": "file_not_found",
  "path": "/1/2/3/TEST2.nc"
}
```

Reason error yang dikirim adalah `remote_selection_disabled`. Pemilihan job tetap hanya melalui menu fisik `Select Job` pada interface, dengan konfirmasi lokal dari operator.

Menu Reset WiFi mengembalikan parameter MQTT ke default `MqttConfig.h` sebelum portal konfigurasi berikutnya.

## Integrasi Marlin

Firmware menggunakan UART `Serial1` untuk komunikasi ke Marlin/SKR.

Komunikasi CNC dikendalikan dari `src/main.cpp`:

```cpp
#define CNC_ENABLE_MARLIN_CONNECTION
```

Status integrasi saat ini:

- Saat komunikasi diaktifkan, ESP32 mengirim `M114` secara periodik untuk meminta posisi.
- UART `Serial1` ke SKR baru dibuka saat `CNC_ENABLE_MARLIN_CONNECTION` aktif, sehingga Serial Monitor USB/UART tetap dapat menampilkan log firmware saat pengujian MQTT tanpa Marlin.
- UART Marlin memakai ESP32-S3 `RX=44` dan `TX=43`; sambungan ke SKR harus silang, yaitu ESP32 TX ke RX SKR dan ESP32 RX ke TX SKR.
- Log diagnostik UART Marlin aktif sementara melalui `AppConfig::ENABLE_MARLIN_UART_LOG`. Serial Monitor menampilkan command keluar dengan prefix `[MARLIN][TX]` dan respons masuk dengan prefix `[MARLIN][RX]`.
- Respons Marlin dengan format posisi `X:... Y:... Z:...` diparse ke `_posX`, `_posY`, dan `_posZ`.
- Nilai posisi tersebut dipakai oleh tampilan standby, Set Origin, dan MQTT `cnc/position`.
- Setelah Marlin mulai merespons, interface melakukan prefetch status non-gerak secara round-robin untuk `M203`, `M115`, `M119`, dan `M211` agar Feedrate, Area X/Y/Z, sensor home, dan soft endstop sudah tersedia ketika user masuk menu Machine Ctrl&Status. Jika salah satu respons belum bisa diparse, query lain tetap berjalan dan tidak tertahan.
- Perintah gerak, spindle, feedrate, dan Set Origin hanya dikirim saat status Marlin `CONNECTED`.
- Pemilihan workspace mengirim `M400`, `G53`, `G54`-`G59.3`, `M211 S1`, dan `M114`; urutan `G53` memastikan slot yang sama dimuat ulang setelah homing.
- Penyimpanan origin mengirim `G92 X0 Y0 Z0` dan `M500`, sehingga firmware Marlin harus dibangun dengan `CNC_COORDINATE_SYSTEMS` dan `EEPROM_SETTINGS` aktif.
- Job sender membaca file SD satu baris per transaksi dan menunggu respons `ok` sebelum mengirim baris berikutnya.
- Polling status reguler dihentikan sementara saat sender memiliki UART; ketika `PAUSED`, sender mengirim heartbeat `M114` sendiri.

Jika SKR V1.4 Turbo belum tersambung, komentari kembali toggle tersebut. Standby akan menampilkan `CNC:[X] | OFF`, polling `M114` tidak dijalankan, perintah mesin tidak dikirim, dan alarm buzzer Marlin tidak aktif.

Status ringkas pada standby:

- `CNC:[X] | OFF`: Marlin belum tersambung atau komunikasi CNC dinonaktifkan.
- `CNC:[OK] | IDLE`: Marlin tersambung tetapi mesin tidak menjalankan job.
- `CNC:[OK] | RUN`: Marlin tersambung dan job sedang berjalan.
- `CNC:[OK] | PAUS`: job sedang pause.
- `CNC:[X] | ERR`: koneksi Marlin error/lost atau job sender masuk error.

Status detail tetap tersedia pada menu Machine Ctrl&Status dan MQTT. Menu Machine Ctrl&Status menampilkan `CNC: OFF`, `CNC: WAITING`, `CNC: DISCONNECTED`, `CNC: CONNECTED`, `CNC: LOST`, atau `CNC: ERROR`, sedangkan MQTT memisahkan status koneksi pada `cnc/machine.state` dan aktivitas kerja pada `cnc/machine.activity`.

Jika koneksi yang sebelumnya aktif terputus, MQTT `cnc/alarm` mengirim:

```json
{
  "level": "ERROR",
  "message": "Marlin connection lost",
  "active": true,
  "popup": true,
  "source": "marlin",
  "code": "MARLIN_CONNECTION_LOST"
}
```

Jika Marlin tersambung tetapi status sensor home/endstop belum terbaca valid, MQTT `cnc/alarm` mengirim warning agar dashboard dapat menampilkan pop-up tanpa menganggapnya error mesin:

```json
{
  "level": "WARNING",
  "message": "Sensor home belum terbaca: Z",
  "active": true,
  "popup": true,
  "source": "home_sensor",
  "code": "HOME_SENSOR_UNKNOWN"
}
```

Status `DISCONNECTED` sebelum koneksi pertama tidak dianggap alarm. Jika Marlin kembali merespons normal dan status sensor sudah terbaca, `cnc/alarm` kembali menjadi:

```json
{
  "level": "INFO",
  "message": "OK",
  "active": false,
  "popup": false,
  "source": "system",
  "code": "OK"
}
```

Topic `cnc/error` tetap dikirim untuk kompatibilitas dashboard lama dan hanya aktif untuk error keras seperti koneksi Marlin hilang, Marlin error, atau job sender error. Warning sensor home dikirim melalui `cnc/alarm`, sedangkan detail status sensor tetap tersedia di `cnc/machine.home`, `cnc/machine.home_valid`, dan `cnc/machine.home_note`.

## Build

Environment PlatformIO:

```text
esp32-s3-devkitm-1
```

Perintah build:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

### GitHub Actions

Saya menambahkan workflow GitHub Actions di `.github/workflows/platformio-build.yml` supaya setiap perubahan yang masuk ke GitHub bisa dicek kompilasinya lebih awal.

Workflow ini berjalan ketika:

- `push` ke branch `main`.
- `pull_request` yang ditujukan ke branch `main`.

Di dalam workflow, GitHub menyiapkan Ubuntu, Python, lalu memasang PlatformIO melalui `pip`. Setelah itu firmware dibuild dengan perintah:

```bash
platformio run -e esp32-s3-devkitm-1
```

Workflow ini hanya saya pakai untuk memastikan firmware masih bisa dikompilasi. Workflow tidak mengunggah firmware ke ESP32-S3 dan tidak menggantikan pengujian langsung pada hardware.

## Batasan Saat Ini

- Job controller file SD sudah tersedia tetapi belum divalidasi pada mesin CNC nyata.
- Pause/Stop bersifat controlled dan memakai `M400` untuk menunggu planner kosong; bukan emergency stop.
- Estimasi waktu belum memperhitungkan akselerasi, pergantian tool, dan waktu proses non-gerak lainnya.
- Interlock Home/Origin saat ini mencatat command yang dikirim interface; validasi independen bahwa homing benar-benar berhasil masih perlu ditambahkan dari status Marlin.
- Workspace tersimpan belum divalidasi pada hardware setelah power-cycle. Penggunaan ulang origin tetap mensyaratkan fixture tidak bergeser dan panjang tool tidak berubah.
- MQTT command sudah dapat diterima dan dicatat di Serial, tetapi belum dieksekusi.
- Baris G-code MQTT sudah dapat diterima dan dicatat di Serial, tetapi belum dikirim ke Marlin atau disimpan ke SD card.
- Upload file G-code via MQTT sudah menyimpan file ke MicroSD, tetapi belum otomatis memilih job, belum masuk `GcodeJobController`, dan belum menjalankan mesin.
- GitHub Actions hanya membuktikan firmware bisa dikompilasi, bukan membuktikan upload firmware atau pengujian perangkat keras sudah berhasil.
