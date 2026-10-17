# Skripsi CNC Milling

Repository utama proyek akhir CNC milling berbasis firmware Marlin pada board BTT SKR V1.4 Turbo (LPC1769) dan antarmuka ESP32-S3.

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

## Post-processor Fusion 360

Petunjuk pemasangan dan penggunaan tersedia pada `mpcnc_post_processor-master/README.md`. Periksa kembali seluruh G-code hasil post-processing sebelum dijalankan pada mesin.

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
