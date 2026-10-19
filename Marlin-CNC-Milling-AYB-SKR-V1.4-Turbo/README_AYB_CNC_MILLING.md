# Marlin CNC Milling AYB - SKR V1.4 Turbo

Konfigurasi ini adalah firmware Marlin untuk mesin CNC milling berbasis BTT SKR V1.4 Turbo (LPC1769) yang dipakai bersama AYB Interface ESP32-S3.

## Target Hardware

- Board: BTT SKR V1.4 Turbo
- MCU: LPC1769
- Firmware basis: Marlin 2.1.2.4
- Fungsi mesin: CNC milling
- Interface eksternal: ESP32-S3 AYB Interface melalui UART

## Perubahan Penting

- Mengaktifkan fitur spindle Marlin melalui `SPINDLE_FEATURE`.
- Menggunakan command `M3` dan `M5` untuk kontrol spindle ON/OFF.
- Output spindle memakai `SPINDLE_LASER_ENA_PIN P2_05`.
- `SPINDLE_LASER_ACTIVE_STATE` diset `HIGH`.
- PWM spindle tidak diaktifkan; output dipakai sebagai sinyal ON/OFF untuk relay atau modul kontrol spindle.
- Mengaktifkan `M115_GEOMETRY_REPORT` agar AYB Interface dapat membaca area kerja mesin melalui command `M115`.
- Konfigurasi ini disiapkan untuk integrasi monitoring dari interface, termasuk pembacaan feedrate, area kerja, soft endstop, dan sensor homing.
- Arah motor Z memakai `INVERT_Z_DIR true` agar arah koordinat positif Z bergerak naik menuju proximity pada Z-Max sesuai hasil uji hardware: perintah Z negatif sebelumnya justru menggerakkan spindle naik.
- `HOME_Z_FIRST` aktif dan `Z_SAFE_HOMING` nonaktif karena Z memakai proximity Z-Max sebagai endstop nyata. Pada `G28` penuh, Z harus homing ke atas sebelum X dan Y bergerak untuk memberi jarak aman terhadap rangka mesin.
- Steps/mm hasil kalibrasi hardware pada feedrate rendah adalah X `20.38`, Y `18.86`, dan Z `400`. X dan Y diuji tiga kali pada jarak 500 mm, sedangkan Z diuji tiga kali pada jarak 50 mm; seluruh pengujian kembali ke titik ukur awal pada resolusi meteran yang digunakan.
- Batas maksimum `M203` ditetapkan dalam mm/detik menjadi X `200`, Y `280`, dan Z `39`, yaitu sekitar 80% dari feedrate maksimum tanpa kehilangan langkah yang dilaporkan pada pengujian tanpa beban pemotongan. Nilai ini setara dengan F12000, F16800, dan F2340 dalam mm/menit.

Status proximity Z-Max wajib menunjukkan `open` saat bebas dan `TRIGGERED` saat mendeteksi logam melalui `M119`. Setelah firmware baru dipasang, lakukan jog Z jarak kecil dari tengah lintasan: Z negatif harus turun dan Z positif harus naik. Jalankan `G28 Z` hanya setelah arah jog benar, kemudian uji `G28` penuh tanpa tool dan benda kerja. Perintah `G28 X` atau `G28 Y` saja tidak menjalankan homing Z terlebih dahulu.

`HOME_Z_FIRST` hanya mengatur urutan saat command homing dijalankan. Fitur ini tidak otomatis menaikkan Z atau memarkir spindle setelah pekerjaan selesai; workflow akhir pekerjaan harus diatur terpisah dan wajib mematikan spindle sebelum gerakan parkir.

## Catatan Integrasi AYB Interface

AYB Interface ESP32-S3 membaca status Marlin dengan query non-gerak:

- `M114` untuk posisi X/Y/Z.
- `M203` untuk feedrate maksimum.
- `M115` untuk geometry report area kerja.
- `M119` untuk sensor homing/endstop.
- `M211` untuk soft endstop.

Command gerak dan spindle tetap harus diuji tanpa tool dan benda kerja terlebih dahulu. Kontrol spindle melalui `M3/M5` harus divalidasi terhadap wiring relay, driver, atau modul spindle yang dipakai.

## File Konfigurasi Utama

- `Marlin/Configuration.h`
- `Marlin/Configuration_adv.h`
- `platformio.ini`

Nama file internal Marlin tersebut tidak diganti karena dibutuhkan oleh proses build Marlin.

## Nama Release yang Disarankan

Jika firmware binary diunggah ke GitHub Release, gunakan nama seperti:

```text
marlin-cnc-milling-ayb-skr-v1.4-turbo-2026-08-22.bin
```
