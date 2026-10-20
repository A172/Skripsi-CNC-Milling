# Skripsi CNC Milling

Repository proyek akhir CNC milling berbasis Marlin pada BTT SKR V1.4 Turbo (LPC1769), dengan interface operator ESP32-S3 dan post-processor Fusion 360.

Repository publik: <https://github.com/A172/Skripsi-CNC-Milling>

## Struktur Repository

| Folder | Isi |
|---|---|
| `InterFace-CNC-Milling-IoT/` | Firmware ESP32-S3 untuk LCD, input operator, MicroSD, WiFi, MQTT, dan UART Marlin. |
| `Marlin-CNC-Milling-AYB-SKR-V1.4-Turbo/` | Firmware Marlin untuk BTT SKR V1.4 Turbo dan driver DM542. |
| `mpcnc_post_processor-master/` | Post-processor Fusion 360 untuk menghasilkan G-code milling Marlin. |

## Konfigurasi Mesin

- Area kerja: X `0..1250 mm`, Y `0..2600 mm`, Z `-140..0 mm`.
- Steps/mm: X `20.38`, Y `18.86`, Z `400`.
- Batas feedrate Marlin: X `200`, Y `280`, Z `39 mm/s`.
- Homing penuh menjalankan Z ke proximity atas terlebih dahulu, kemudian X/Y.
- Work Origin tersedia pada G54-G59.3 dan disimpan melalui EEPROM/flash Marlin.
- Parkir akhir normal: X `1250`, Y `100`, Z `-20`.

Nilai tersebut berasal dari konfigurasi dan pengujian yang sudah dilakukan, tetapi tetap memerlukan validasi ulang setelah perubahan mekanik, driver, tool, atau firmware.

## Workflow Operator

```text
Home All
  -> pilih G54-G59.3
  -> Pilih Origin / Perbarui Origin
  -> Select Job
  -> Start
```

Selama mesin tetap menyala, Home All dan job selesai normal memuat ulang workspace terakhir sehingga Repeat Job tidak membutuhkan pemilihan origin ulang. Setelah reset/power-cycle, Stop, error, MicroSD dilepas, atau koneksi Marlin terputus, operator wajib melakukan Home All dan memilih origin kembali.

File `InterFace-CNC-Milling-IoT/examples/gcode/CEK_ORIGIN_XY.gcode` tersedia untuk pemeriksaan origin tanpa mata pahat.

## Build

Interface ESP32-S3:

```powershell
cd InterFace-CNC-Milling-IoT
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32-s3-devkitm-1
```

Marlin LPC1769:

```powershell
cd Marlin-CNC-Milling-AYB-SKR-V1.4-Turbo
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Build sukses hanya membuktikan source dapat dikompilasi dan bukan bukti mesin sudah aman secara fisik.

## Keselamatan

- Uji G-code baru tanpa mata pahat dan benda kerja terlebih dahulu.
- Pastikan sensor home, arah gerak, soft endstop, fixture, dan emergency stop berfungsi.
- Pemilihan atau upload file tidak boleh otomatis menjalankan mesin.
- MQTT command tetap receive-only dan tidak diteruskan langsung ke Marlin.
- Tinjau seluruh G-code hasil Fusion 360 sebelum menjalankan spindle.

## Dokumentasi

- [Firmware interface](InterFace-CNC-Milling-IoT/README.md)
- [Status interface](InterFace-CNC-Milling-IoT/PROJECT_STATUS.md)
- [Konfigurasi CNC Marlin](Marlin-CNC-Milling-AYB-SKR-V1.4-Turbo/README_AYB_CNC_MILLING.md)
- [Post-processor Fusion 360](mpcnc_post_processor-master/README.md)
- `PROJECT_STATUS.md`: status gabungan dan rencana pengujian.
- `CHANGELOG.md`: riwayat perubahan repository.
