# Project Status

## Ringkasan Status Project Saat Ini

Repository gabungan telah disiapkan untuk menyimpan firmware interface ESP32-S3, firmware Marlin BTT SKR V1.4 Turbo, dan post-processor Fusion 360 dalam satu repository skripsi. Source setiap komponen tetap dipisahkan agar proses build dan tanggung jawab masing-masing tidak tercampur.

## Fitur yang Sudah Selesai

- Struktur repository gabungan untuk tiga komponen proyek.
- Pemisahan folder firmware interface, firmware Marlin, dan post-processor.
- Aturan ignore untuk cache, hasil build, file editor lokal, dan file sementara.
- Dokumentasi tingkat repository mengenai struktur, build, dan batasan keselamatan.
- Repository Git lokal pada branch `main` dan staging commit awal.
- Repository GitHub Public tersedia di `https://github.com/A172/Skripsi-CNC-Milling`.

Status fitur rinci firmware interface tersedia pada `InterFace-CNC-Milling-IoT/PROJECT_STATUS.md`.

## Fitur yang Sedang Dikerjakan

- Validasi integrasi interface ESP32-S3 dengan Marlin pada hardware CNC.

## Daftar Bug yang Masih Ada

- Belum ada bug baru yang ditemukan akibat penggabungan folder; kedua firmware berhasil dikompilasi.
- Build Marlin masih menghasilkan warning konfigurasi mengenai metode feedback pengguna dan pin DIAG yang perlu ditinjau berdasarkan wiring aktual.
- Pengujian hardware tidak dilakukan dalam proses penyiapan repository.
- Batasan firmware yang belum selesai tetap mengikuti dokumentasi masing-masing komponen.

## TODO Prioritas Berikutnya

1. Tinjau warning Marlin berdasarkan konfigurasi feedback dan wiring pin DIAG aktual.
2. Lanjutkan validasi komunikasi UART dan pengujian hardware sesuai prosedur keselamatan.

## Hasil Verifikasi Build

- Interface ESP32-S3, environment `esp32-s3-devkitm-1`: **SUCCESS**; RAM 14,8% dan Flash 30,2%.
- Marlin, environment `LPC1769`: **SUCCESS**; RAM 16,9% dan Flash 18,8%.
- Pengujian hardware: **belum dilakukan**.
