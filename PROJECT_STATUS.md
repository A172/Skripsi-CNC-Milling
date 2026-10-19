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
- Pengujian travel tanpa beban tidak menunjukkan kehilangan langkah hingga F15000 pada X, F21000 pada Y, dan F2940 pada Z.
- Batas maksimum `M203` ditetapkan dengan margin sekitar 80% menjadi X `200`, Y `280`, dan Z `39` mm/detik.
- Feedrate homing ditingkatkan menjadi X/Y `3000` mm/menit dan Z `600` mm/menit; pendekatan kedua dibagi empat menjadi X/Y `750` mm/menit dan Z `150` mm/menit.
- Kecepatan homing baru sudah diuji pada hardware: Z bergerak naik lebih cepat dan halus, proximity menghentikan homing dengan benar, pendekatan kedua berjalan lebih lambat, dan `G28` penuh tetap menyelesaikan Z sebelum X/Y.
- Post-processor Fusion 360 memiliki opsi parkir otomatis untuk penyelesaian normal job milling Marlin: `M400`, spindle OFF, `G28`, lalu parkir pada X `1250`, Y `100`, Z `-20` dengan gerakan X/Y lebih dahulu dan Z terakhir.

Status fitur rinci firmware interface tersedia pada `InterFace-CNC-Milling-IoT/PROJECT_STATUS.md`.

## Fitur yang Sedang Dikerjakan

- Validasi integrasi interface ESP32-S3 dengan Marlin pada hardware CNC.
- Validasi ketahanan gerak berulang pada batas `M203` baru.
- Validasi satu file G-code baru hasil post-processing untuk memastikan footer parkir otomatis terbentuk dan dijalankan hanya setelah job selesai normal.
- Peninjauan akselerasi sebelum pengujian job CNC.

## Daftar Bug yang Masih Ada

- Belum ada bug baru yang ditemukan akibat penggabungan folder; kedua firmware berhasil dikompilasi.
- Build Marlin masih menghasilkan warning konfigurasi mengenai metode feedback pengguna dan pin DIAG yang perlu ditinjau berdasarkan wiring aktual.
- Pengujian hardware baru mencakup homing dan akurasi gerak tanpa proses pemotongan.
- Urutan parkir otomatis sudah divalidasi secara statis pada post-processor, tetapi footer hasil Fusion 360 belum diuji sebagai satu job lengkap pada hardware.
- Posisi parkir X `1250`, Y `100`, Z `-20` sudah diuji secara manual, tetapi workflow otomatis setelah job masih menunggu validasi end-to-end.
- Batasan firmware yang belum selesai tetap mengikuti dokumentasi masing-masing komponen.

## TODO Prioritas Berikutnya

1. Pasang firmware hasil kalibrasi dan konfirmasi `M503` menampilkan `M92 X20.38 Y18.86 Z400.00`.
2. Uji akurasi pergi-kembali minimal 10 kali pada batas `M203` baru tanpa tool dan benda kerja.
3. Tinjau dan koreksi akselerasi sesuai kemampuan mekanik mesin.
4. Tinjau warning Marlin berdasarkan konfigurasi feedback dan wiring pin DIAG aktual.
5. Lanjutkan validasi komunikasi UART dan pengujian hardware sesuai prosedur keselamatan.
6. Buat satu file uji dari Fusion 360, tinjau footer, lalu jalankan tanpa tool dan benda kerja untuk memvalidasi parkir otomatis.

## Hasil Verifikasi Build

- Interface ESP32-S3, environment `esp32-s3-devkitm-1`: **SUCCESS**; RAM 14,8% dan Flash 30,2%.
- Marlin, environment `LPC1769`: **SUCCESS**; RAM 16,9% dan Flash 18,7%.
- Pengujian hardware: **sebagian selesai** untuk arah gerak, sensor proximity, homing, akurasi jarak, dan pencarian batas travel tanpa beban; ketahanan jangka panjang serta pemotongan belum diuji.
