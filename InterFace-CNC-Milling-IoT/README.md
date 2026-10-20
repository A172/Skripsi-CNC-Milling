# AYB Interface

Firmware antarmuka CNC milling berbasis ESP32-S3 untuk mengendalikan Marlin pada BTT SKR V1.4 Turbo. Interface menyediakan LCD, tombol jog, rotary encoder, MicroSD, WiFi, MQTT, pemilihan Work Origin, dan pengiriman file G-code melalui UART.

> Build yang berhasil hanya membuktikan source dapat dikompilasi. Gerakan mesin, sensor, spindle, LCD, MicroSD, UART, dan jaringan tetap harus diuji pada hardware.

## Gambaran Sistem

```text
Operator
   |
LCD + tombol + rotary
   |
ESP32-S3 AYB Interface
   |-- MicroSD: file G-code
   |-- WiFi/MQTT: monitoring dan manajemen file
   `-- UART 115200: Marlin
                         |
                  BTT SKR V1.4 Turbo
                         |
                 Motor, sensor, spindle
```

Tanggung jawab source dipisahkan sebagai berikut:

- `LcdHandler`: menggambar tampilan.
- `Menu`: menyimpan daftar dan pilihan menu.
- `AppController`: mengatur state UI, hardware, MQTT, SD, dan Marlin.
- `GcodeJobController`: membaca file serta menunggu respons `ok` sebelum mengirim baris berikutnya.

## Hardware Utama

- ESP32-S3 DevKitM-1.
- LCD 128x64 ST7920 dalam mode SPI.
- Delapan push button untuk jog, Enter, dan Back.
- Rotary encoder dengan tombol tekan.
- Modul MicroSD SPI.
- Passive buzzer pada GPIO38.
- UART Marlin: RX GPIO44, TX GPIO43, baudrate 115200.

Mapping lengkap berada di `src/config/PinConfig.h`. Jangan mengubah pin tanpa menyesuaikan PCB dan wiring.

## Fitur Utama

- Boot screen, standby, menu, submenu, info, dan dialog konfirmasi.
- Jog X/Y/Z dan Home All.
- Sembilan Work Origin Marlin: G54 sampai G59.3.
- Penyimpanan origin menggunakan `G92 X0 Y0 Z0` dan `M500`.
- Browser file MicroSD dan analisis estimasi waktu non-blocking.
- Sender G-code dengan flow control respons `ok`.
- Start, Pause, Resume, controlled Stop, dan Repeat Job.
- Status posisi, sensor home, soft endstop, area kerja, spindle, dan koneksi CNC.
- WiFiManager, MQTT lokal, NTP, serta editor alamat broker dari LCD.
- Upload, daftar, status, dan penghapusan file MicroSD melalui MQTT dengan validasi.
- Monitoring Node-RED melalui topic retained.

## Menu Operator

```text
MAIN MENU
|- Move & Jog
|  |- Home All (G28)
|  `- Work Origin
|- Select Job
|- Machine Ctrl&Status
|- Network
`- About

WORK ORIGIN
|- G54
|- G55
|- G56
|- G57
|- G58
|- G59
|- G59.1
|- G59.2
`- G59.3

SETIAP ORIGIN
|- Pilih Origin
`- Perbarui Origin
```

Aturan dialog berlaku konsisten:

- Enter fisik memilih `Yes`.
- Back memilih `No`.
- Rotary memindahkan sorotan `Yes/No`.
- Klik rotary menjalankan pilihan yang disorot.
- Pilihan awal adalah `No`.

## Workflow Pekerjaan

Untuk pekerjaan pertama setelah power-on:

1. Jalankan `Home All`.
2. Buka `Work Origin`.
3. Pilih G54-G59.3.
4. Gunakan `Pilih Origin` untuk memakai data tersimpan, atau `Perbarui Origin` untuk menentukan nol baru.
5. Pilih file melalui `Select Job`.
6. Tunggu analisis selesai.
7. Tahan tombol `X+ / PLAY`, kemudian konfirmasi Start.

Selama mesin dan interface tetap menyala, Home All dan penyelesaian job normal memuat ulang workspace terakhir secara otomatis. Repeat Job tidak memerlukan pemilihan origin ulang. Reset/power-cycle, controlled Stop, error, MicroSD dilepas, atau koneksi Marlin terputus tetap mewajibkan Home All dan pemilihan origin ulang.

File `examples/gcode/CEK_ORIGIN_XY.gcode` dapat disalin ke MicroSD untuk memeriksa X0 Y0 origin aktif. Jalankan tanpa mata pahat dan pastikan lintasan bebas.

## Aturan Work Origin

`Pilih Origin` mengirim:

```gcode
M400
G53
G54
M211 S1
M114
```

Kode terakhir berubah sesuai slot yang dipilih. `G53` memastikan offset slot dimuat ulang setelah homing.

`Perbarui Origin` membuka layar jog. Setelah operator menekan Enter dan menyetujui konfirmasi, interface mengirim:

```gcode
M400
G92 X0 Y0 Z0
M500
M211 S1
```

Origin hanya boleh digunakan kembali jika fixture dan referensi tool tidak berubah.

## Job Control

- `X+ / PLAY`: tahan untuk Start atau Repeat; tekan singkat untuk Resume.
- `X- / PAUSE`: Pause setelah command aktif mendapat respons `ok`.
- `Z- / STOP`: tahan untuk controlled Stop dan spindle OFF.

Controlled Stop bukan emergency stop. Emergency stop hardware tetap wajib tersedia.

## MQTT Ringkas

Prefix default adalah `cnc`. Konfigurasi berada di `src/config/MqttConfig.h` dan dapat diganti melalui WiFiManager/Preferences.

Topic monitoring utama:

- `cnc/status`, `cnc/machine`, `cnc/position`, dan `cnc/progress`.
- `cnc/network`, `cnc/time`, `cnc/alarm`, dan `cnc/error`.
- `cnc/sd/status` dan `cnc/file/list`.

Topic file menggunakan `cnc/file/start`, `chunk`, `finish`, `ack`, `list`, dan `delete`. Upload hanya menyimpan file ke MicroSD; file tidak otomatis dipilih atau dijalankan. `cnc/command` dan `cnc/gcode` tetap receive-only dan tidak diteruskan ke Marlin.

## Build

Environment PlatformIO:

```text
esp32-s3-devkitm-1
```

Perintah build PowerShell:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32-s3-devkitm-1
```

Build GitHub Actions berjalan pada push dan pull request ke branch `main`, tetapi tidak mengunggah firmware ke perangkat.

## Konfigurasi Penting

- `src/config/AppConfig.h`: fitur, timeout, area kerja, jog, dan job control.
- `src/config/PinConfig.h`: mapping pin hardware.
- `src/config/MqttConfig.h`: broker, topic, dan interval MQTT.
- `src/main.cpp`: toggle LCD/input, jaringan, dan koneksi Marlin.
- `platformio.ini`: board, framework, build flags, dan dependency.

`UI_DEVELOPMENT_MODE` saat ini aktif. Menu manual spindle hanya menampilkan preview selama mode tersebut aktif; sender file G-code tetap memiliki jalur kontrol job tersendiri.

## Batasan dan Keselamatan

- Home/Origin preflight masih berdasarkan command yang dikirim interface, bukan verifikasi independen bahwa homing berhasil.
- Persistensi G54-G59.3 setelah power-cycle masih memerlukan validasi hardware menyeluruh.
- Pause dan Stop bersifat controlled, bukan penghentian darurat.
- Estimasi waktu belum memperhitungkan seluruh efek akselerasi dan proses non-gerak.
- MQTT tidak diizinkan menjalankan mesin secara langsung.
- Pengujian pertama harus dilakukan tanpa mata pahat dan tanpa benda yang menghalangi lintasan.

## Dokumentasi

- `PROJECT_STATUS.md`: status, bug, hasil validasi, dan TODO terbaru.
- `CHANGELOG.md`: riwayat perubahan.
- `ARCHITECTURE.md`: struktur software.
- `AGENTS.md`: aturan pengembangan dan keselamatan proyek.
