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
- Homing X, Y, Z secara terpisah dan `G28` penuh sudah berhasil diuji pada hardware dengan posisi akhir X=1300, Y=0, dan Z=0.
- Steps/mm hasil kalibrasi hardware ditetapkan menjadi X `20.38`, Y `18.86`, dan Z `400`; pengujian berulang pada feedrate rendah menghasilkan jarak aktual sesuai perintah.

Status fitur rinci firmware interface tersedia pada `InterFace-CNC-Milling-IoT/PROJECT_STATUS.md`.

## Fitur yang Sedang Dikerjakan

- Validasi integrasi interface ESP32-S3 dengan Marlin pada hardware CNC.
- Validasi akurasi dan kehilangan langkah pada feedrate gerak yang lebih tinggi.
- Peninjauan batas maksimum `M203` dan akselerasi sebelum pengujian job CNC.

## Daftar Bug yang Masih Ada

- Belum ada bug baru yang ditemukan akibat penggabungan folder; kedua firmware berhasil dikompilasi.
- Build Marlin masih menghasilkan warning konfigurasi mengenai metode feedback pengguna dan pin DIAG yang perlu ditinjau berdasarkan wiring aktual.
- Pengujian hardware baru mencakup homing dan akurasi gerak tanpa proses pemotongan.
- Workflow parkir aman setelah job selesai belum ditentukan; `HOME_Z_FIRST` hanya berlaku ketika homing dijalankan.
- Batasan firmware yang belum selesai tetap mengikuti dokumentasi masing-masing komponen.

## TODO Prioritas Berikutnya

1. Pasang firmware hasil kalibrasi dan konfirmasi `M503` menampilkan `M92 X20.38 Y18.86 Z400.00`.
2. Uji akurasi berulang dengan kenaikan feedrate bertahap tanpa tool dan benda kerja.
3. Tinjau dan koreksi batas maksimum `M203` serta akselerasi sesuai kemampuan mekanik mesin.
4. Tinjau warning Marlin berdasarkan konfigurasi feedback dan wiring pin DIAG aktual.
5. Lanjutkan validasi komunikasi UART dan pengujian hardware sesuai prosedur keselamatan.
6. Tentukan dan validasi workflow akhir job: spindle OFF, Z ke posisi aman, lalu parkir X/Y bila diperlukan.

## Hasil Verifikasi Build

- Interface ESP32-S3, environment `esp32-s3-devkitm-1`: **SUCCESS**; RAM 14,8% dan Flash 30,2%.
- Marlin, environment `LPC1769`: **SUCCESS**; RAM 16,9% dan Flash 18,7%.
- Pengujian hardware: **sebagian selesai** untuk arah gerak, sensor proximity, homing, dan akurasi jarak pada feedrate rendah; pemotongan dan feedrate tinggi belum diuji.
