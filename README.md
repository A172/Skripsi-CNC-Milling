# Skripsi CNC Milling

Repository utama proyek akhir CNC milling berbasis firmware Marlin pada board BTT SKR V1.4 Turbo (LPC1769) dan antarmuka ESP32-S3.

Repository GitHub Public: <https://github.com/A172/Skripsi-CNC-Milling>

## Struktur Repository

| Folder | Fungsi |
|---|---|
| `InterFace-CNC-Milling-IoT/` | Firmware antarmuka ESP32-S3 untuk LCD, input operator, MicroSD, WiFi, MQTT, dan komunikasi UART dengan Marlin. |
| `Marlin-CNC-Milling-AYB-SKR-V1.4-Turbo/` | Firmware Marlin untuk pengendali utama mesin CNC pada BTT SKR V1.4 Turbo. |
| `mpcnc_post_processor-master/` | Post-processor Fusion 360 untuk menghasilkan G-code yang sesuai dengan firmware CNC. |

Setiap komponen memiliki dokumentasi dan konfigurasi build sendiri. Repository ini menyatukan seluruh source code yang digunakan dalam sistem skripsi tanpa menggabungkan proses build ketiga komponen.

## Build Firmware Interface ESP32-S3

```powershell
cd InterFace-CNC-Milling-IoT
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32-s3-devkitm-1
```

Build hanya memverifikasi kompilasi. Pengujian LCD, tombol, rotary encoder, MicroSD, WiFi, MQTT, dan UART Marlin tetap harus dilakukan pada hardware.

## Build Firmware Marlin

```powershell
cd Marlin-CNC-Milling-AYB-SKR-V1.4-Turbo
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Environment build yang aktif mengikuti `default_envs` pada `platformio.ini` Marlin. Konfigurasi keselamatan, pin, homing, origin, soft endstop, dan spindle tidak boleh diubah tanpa pemeriksaan wiring dan persetujuan yang jelas.

Konfigurasi aktual mengarahkan homing Z ke proximity Z-Max di bagian atas. Berdasarkan uji hardware yang menunjukkan perintah Z negatif sebelumnya menggerakkan spindle naik, `INVERT_Z_DIR true` digunakan agar arah positif Z bergerak naik. Sebelum `G28 Z`, validasi `M119` harus menunjukkan `open` saat sensor bebas dan `TRIGGERED` saat mendeteksi logam, kemudian pastikan jog Z negatif turun dan Z positif naik.

`HOME_Z_FIRST` diaktifkan dan `Z_SAFE_HOMING` dinonaktifkan karena mesin menggunakan proximity Z-Max sebagai endstop nyata. Dengan konfigurasi ini, `G28` penuh melakukan homing Z ke atas terlebih dahulu, kemudian X dan Y. Perintah parsial seperti `G28 X` atau `G28 Y` tetap hanya melakukan homing pada sumbu yang diminta.

Pengaturan tersebut hanya berlaku saat homing dan tidak otomatis memarkir spindle setelah job selesai. Prosedur akhir job yang mematikan spindle lalu memindahkan Z ke posisi aman harus dirancang dan diuji sebagai workflow terpisah.

Kalibrasi gerak hardware pada feedrate rendah menetapkan steps/mm X `20.38`, Y `18.86`, dan Z `400`. X dan Y diuji tiga kali pada perpindahan 500 mm, sedangkan Z diuji tiga kali pada perpindahan 50 mm. Semua hasil sesuai jarak perintah dan kembali ke titik ukur awal pada resolusi meteran yang digunakan. Akurasi pada feedrate lebih tinggi masih harus diuji sebelum digunakan untuk proses milling.

Pengujian travel tanpa beban pemotongan mencapai F15000 pada X, F21000 pada Y, dan F2940 pada Z tanpa kehilangan langkah. Firmware memakai batas `M203` sekitar 80% dari hasil tersebut, yaitu X `200`, Y `280`, dan Z `39` mm/detik (setara F12000, F16800, dan F2340). Batas travel ini bukan feedrate pemotongan; feedrate proses tetap harus disesuaikan dengan tool, spindle, dan material.

Feedrate homing menggunakan X/Y `3000` mm/menit dan Z `600` mm/menit. Pendekatan kedua setelah bump dibagi empat menjadi X/Y `750` mm/menit dan Z `150` mm/menit agar pemicuan sensor tetap lebih pelan. Konfigurasi ini tidak memakai batas travel maksimum sebagai kecepatan homing dan masih memerlukan validasi langsung pada hardware.

## Post-processor Fusion 360

Petunjuk pemasangan dan penggunaan tersedia pada `mpcnc_post_processor-master/README.md`. Periksa kembali seluruh G-code hasil post-processing sebelum dijalankan pada mesin.

Post-processor Marlin untuk job milling sekarang menambahkan parkir otomatis hanya pada penyelesaian normal. Footer menunggu planner selesai, memastikan spindle OFF, menjalankan `G28` dengan Z lebih dahulu, lalu bergerak ke posisi parkir yang sudah divalidasi pada hardware: X `1250`, Y `100`, Z `-20`. Gerakan masuk parkir memakai XY `3000` mm/menit dan Z `600` mm/menit. Jika kontrol spindle manual aktif, terdapat konfirmasi operator setelah `M5` sebelum homing dimulai. Fitur dapat dinonaktifkan melalui properti `Job: Safe park on finish` dan tidak dijalankan pada emergency stop, job yang dibatalkan, laser/jet, firmware non-Marlin, atau saat `Extern: Stop File` digunakan.

## Keselamatan

- Pemilihan file atau upload G-code tidak boleh otomatis menjalankan mesin.
- Perintah gerakan dan spindle harus melalui validasi serta konfirmasi operator.
- Build sukses tidak membuktikan fungsi hardware atau keselamatan mesin.
- Lakukan pengujian awal tanpa tool terpasang dan gunakan emergency stop yang berfungsi.

## Dokumentasi Komponen

- Dokumentasi interface: `InterFace-CNC-Milling-IoT/README.md`
- Status interface: `InterFace-CNC-Milling-IoT/PROJECT_STATUS.md`
- Dokumentasi konfigurasi CNC: `Marlin-CNC-Milling-AYB-SKR-V1.4-Turbo/README_AYB_CNC_MILLING.md`
- Dokumentasi Marlin: `Marlin-CNC-Milling-AYB-SKR-V1.4-Turbo/README.md`
