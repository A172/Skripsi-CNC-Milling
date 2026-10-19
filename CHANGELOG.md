# Changelog

Semua perubahan penting pada repository gabungan didokumentasikan pada file ini.

## [Unreleased]

### Added

- Struktur repository gabungan untuk firmware interface ESP32-S3, firmware Marlin BTT SKR V1.4 Turbo, dan post-processor Fusion 360.
- README utama berisi gambaran sistem, petunjuk build, dan catatan keselamatan.
- PROJECT_STATUS utama untuk melacak status repository gabungan.
- `.gitignore` utama untuk mencegah cache dan artefak build masuk ke Git.
- Repository Git lokal dengan branch utama `main`.

### Removed

- Metadata repository Git yang ikut tersalin dari proyek interface.
- Cache build PlatformIO dan cache Python dari salinan proyek.

### Changed

- Mengubah `INVERT_Z_DIR` dari `true` menjadi `false` agar homing Z bergerak naik menuju proximity Z-Max sesuai `Z_HOME_DIR 1`.
- Menambahkan prosedur validasi `M119`, jog Z jarak kecil, dan pengujian homing aman pada dokumentasi.
- Mengoreksi `INVERT_Z_DIR` kembali menjadi `true` setelah uji hardware menunjukkan perintah Z negatif justru menggerakkan spindle naik. `Z_HOME_DIR 1` dan input proximity Z-Max tidak diubah.
- Memperjelas prosedur uji arah: Z negatif harus turun dan Z positif harus naik sebelum menjalankan `G28 Z`.
- Mengaktifkan `HOME_Z_FIRST` dan menonaktifkan `Z_SAFE_HOMING` agar `G28` penuh melakukan homing Z ke proximity atas sebelum X/Y bergerak.
- Mendokumentasikan bahwa homing parsial `G28 X` atau `G28 Y` tidak otomatis melakukan homing Z.
- Menegaskan bahwa `HOME_Z_FIRST` tidak mengatur parkir setelah job; workflow spindle OFF dan parkir aman tetap harus dibuat terpisah.

### Notes

- Source code firmware dan konfigurasi hardware tidak diubah pada proses penggabungan ini.
- Build interface ESP32-S3 environment `esp32-s3-devkitm-1` berhasil.
- Build Marlin environment `LPC1769` berhasil dengan warning konfigurasi yang sudah ada.
- Repository telah dipublikasikan ke `https://github.com/A172/Skripsi-CNC-Milling`.
