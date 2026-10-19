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
- Arah motor Z dikoreksi menjadi `INVERT_Z_DIR true` berdasarkan uji hardware: perintah Z negatif sebelumnya justru menggerakkan spindle naik. `Z_HOME_DIR 1` tetap digunakan agar `G28 Z` menuju proximity Z-Max di bagian atas.
- Urutan homing penuh dikonfigurasi dengan `HOME_Z_FIRST` aktif dan `Z_SAFE_HOMING` nonaktif agar Z mencapai proximity atas sebelum X/Y bergerak secara horizontal.

Status fitur rinci firmware interface tersedia pada `InterFace-CNC-Milling-IoT/PROJECT_STATUS.md`.

## Fitur yang Sedang Dikerjakan

- Validasi integrasi interface ESP32-S3 dengan Marlin pada hardware CNC.
- Validasi jog Z jarak kecil, homing Z, dan urutan `G28` penuh setelah firmware baru dipasang.

## Daftar Bug yang Masih Ada

- Belum ada bug baru yang ditemukan akibat penggabungan folder; kedua firmware berhasil dikompilasi.
- Build Marlin masih menghasilkan warning konfigurasi mengenai metode feedback pengguna dan pin DIAG yang perlu ditinjau berdasarkan wiring aktual.
- Pengujian hardware tidak dilakukan dalam proses penyiapan repository.
- Koreksi arah Z sudah diverifikasi pada konfigurasi dan build, tetapi firmware baru belum diuji dengan gerakan mesin setelah dipasang.
- Urutan Z lebih dahulu sudah diverifikasi dari source dan build, tetapi belum diuji pada hardware.
- Workflow parkir aman setelah job selesai belum ditentukan; `HOME_Z_FIRST` hanya berlaku ketika homing dijalankan.
- Batasan firmware yang belum selesai tetap mengikuti dokumentasi masing-masing komponen.

## TODO Prioritas Berikutnya

1. Pasang firmware Marlin hasil build dan lakukan jog Z jarak kecil dari tengah lintasan.
2. Pastikan Z negatif bergerak turun, Z positif bergerak naik, dan `M119` tetap berubah `open` ke `TRIGGERED`.
3. Uji `G28 Z` tanpa tool dan benda kerja, dengan emergency stop atau pemutus daya siap digunakan.
4. Uji `G28` penuh dan pastikan urutannya Z ke atas, kemudian X, lalu Y.
5. Tinjau warning Marlin berdasarkan konfigurasi feedback dan wiring pin DIAG aktual.
6. Lanjutkan validasi komunikasi UART dan pengujian hardware sesuai prosedur keselamatan.
7. Tentukan dan validasi workflow akhir job: spindle OFF, Z ke posisi aman, lalu parkir X/Y bila diperlukan.

## Hasil Verifikasi Build

- Interface ESP32-S3, environment `esp32-s3-devkitm-1`: **SUCCESS**; RAM 14,8% dan Flash 30,2%.
- Marlin, environment `LPC1769`: **SUCCESS**; RAM 16,9% dan Flash 18,8%.
- Pengujian hardware: **belum dilakukan**.
