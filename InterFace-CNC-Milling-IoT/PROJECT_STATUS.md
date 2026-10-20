# Project Status

## Ringkasan Status Project Saat Ini

Firmware **AYB Interface** pada ESP32-S3 DevKitM-1 sudah memiliki UI LCD, input tombol/rotary, passive buzzer, SD card browser, konfigurasi jaringan lewat WiFiManager, MQTT monitoring lokal, sinkronisasi waktu NTP, dan integrasi awal Marlin pada BTT SKR V1.4 Turbo untuk membaca posisi melalui `M114`. Di sisi repository, saya juga menambahkan GitHub Actions agar build PlatformIO bisa dicek otomatis saat ada perubahan ke branch `main`.

Pada sesi terbaru, fitur network di entrypoint diaktifkan untuk pengujian integrasi Node-RED dengan ESP32-S3. MQTT sekarang menerima protokol upload file G-code melalui `cnc/file/start`, `cnc/file/chunk`, dan `cnc/file/finish`, menulis data ke file sementara di MicroSD, lalu membalas ACK melalui `cnc/file/ack`. Tahap ini sudah menyimpan file ke MicroSD, tetapi belum mengirim G-code ke Marlin dan belum mengeksekusi G-code.

Pengembangan terbaru menambahkan hot-swap MicroSD, status MicroSD via MQTT, penghapusan file via MQTT, dan overwrite eksplisit pada upload. Dashboard tetap hanya untuk monitoring dan manajemen file; pemilihan job CNC tetap melalui interface fisik.

Pada sesi 09/08/2026 ditambahkan mode sementara `CNC_HEADLESS_WIFI_MQTT_DEV` agar prosedur pengembangan WiFi/MQTT tetap bisa berjalan ketika LCD bermasalah. Mode ini melewati inisialisasi LCD dan input fisik, tetapi tetap menjalankan Serial Monitor, buzzer, MicroSD, WiFi, dan MQTT.

Pada sesi 21/08/2026 ditambahkan editor IP broker MQTT dari menu Network > Broker. User dapat mengisi broker dengan format fixed digit `000.000.000.000`, misalnya `192.168.250.009`, lalu menyimpan perubahan melalui dialog konfirmasi LCD. Mode headless tetap tersedia sebagai toggle pengembangan, tetapi default dibuat nonaktif agar menu LCD dapat dipakai.

Setelah uji awal hardware, respons rotary pada editor broker diperbaiki dengan mematikan log input per-tick secara default, membatasi redraw LCD editor broker agar tidak menulis frame penuh pada setiap pulsa rotary, dan menjeda proses connect/reconnect MQTT selama editor broker aktif.
Highlight digit aktif pada editor broker dibuat negatif seperti highlight menu agar posisi input lebih jelas terlihat.
Respons input saat modul MicroSD belum terpasang diperbaiki dengan menambahkan jeda retry mount SD 5 detik setelah `SD.begin()` gagal, sehingga loop utama tidak tertahan percobaan mount yang terlalu sering.

Dashboard sekarang mendapat status sensor home/endstop yang lebih jelas melalui `cnc/machine.home`, `cnc/machine.home_valid`, dan `cnc/machine.home_note`, sambil tetap mempertahankan field lama `x_home`, `y_home`, dan `z_home`.

Pada sesi 22/08/2026 payload alarm MQTT diperjelas untuk kebutuhan pop-up Node-RED. Topic `cnc/alarm` sekarang membawa `active`, `popup`, `source`, dan `code` selain `level` dan `message`, termasuk warning saat sensor home/endstop belum terbaca valid.

Koneksi Marlin sekarang diaktifkan untuk uji awal hardware SKR. Mapping UART interface disesuaikan menjadi RX GPIO44 dan TX GPIO43, serta log diagnostik `[MARLIN][TX]` dan `[MARLIN][RX]` diaktifkan sementara untuk memeriksa respons `M114`, `M203`, `M115`, `M119`, dan `M211`.

Saat MicroSD memang dilepas untuk pengujian Marlin, retry mount otomatis diperlambat menjadi 30 detik dan build ESP32 memakai `CORE_DEBUG_LEVEL=0` agar log internal Arduino core seperti `sd_diskio.cpp` tidak memenuhi Serial Monitor.

Area X/Y/Z sekarang memiliki fallback lokal 1250/2600/140 mm sejak koneksi Marlin diaktifkan agar LCD dan dashboard tidak kosong setelah boot. Firmware tetap meminta `M115` dan mengganti fallback jika data runtime Marlin berhasil diparse. Nilai X diselaraskan dengan batas kerja Marlin terbaru `0..1250 mm`.

Area kerja mesin juga dikirim ke dashboard melalui payload retained `cnc/machine` sebagai `area_x`, `area_y`, `area_z`, `area_source`, dan objek `work_area` agar panel Data Mesin Node-RED dapat menampilkan Area X/Y/Z tanpa membaca LCD.

Workflow origin terbaru memakai workspace Marlin `G54`-`G59.3`. Setiap slot menyediakan `Pilih Origin` untuk menggunakan data tersimpan dan `Perbarui Origin` untuk mengirim `G92 X0 Y0 Z0` lalu `M500`. Selama satu sesi mesin, Home All dan penyelesaian job normal memuat ulang workspace terakhir. Reset/power-cycle, stop, error, SD dilepas, atau koneksi terputus tetap mewajibkan pemilihan ulang.

Firmware sekarang melakukan prefetch status mesin non-gerak setelah Marlin mulai merespons. Query `M203`, `M115`, `M119`, dan `M211` dikirim round-robin dengan jeda pendek agar nilai Feedrate, Area X/Y/Z, sensor home, dan soft endstop sudah siap saat menu Machine Ctrl&Status dibuka, tanpa tertahan jika salah satu respons belum bisa diparse.

Masuk ke menu Machine Ctrl&Status sekarang hanya menampilkan nilai terakhir yang sudah diprefetch, tanpa memanggil refresh otomatis. Refresh ulang parameter tetap tersedia lewat item `Refresh Status`.

## Fitur yang Sudah Selesai

- README diringkas menjadi gambaran sistem, hardware, fitur, workflow operator, MQTT, build, dan keselamatan; rincian status tetap dipisahkan ke PROJECT_STATUS dan riwayat ke CHANGELOG.
- LCD menu system untuk booting, standby, menu utama, submenu, info, dan konfirmasi.
- Input push button dan rotary encoder.
- Passive buzzer GPIO 38 dengan PWM LEDC dan pola bunyi non-blocking.
- Feedback bunyi untuk Enter, Back, klik rotary, hasil WiFi/MQTT, warning, job selesai, dan alarm timeout Marlin.
- Pola alarm buzzer dibuat tiga beep tajam dengan jeda sangat pendek dan restart langsung agar koneksi Marlin yang terputus terdengar lebih urgent.
- Buzzer dihentikan sebelum portal WiFiManager dibuka sehingga tidak ada tone tombol yang tertahan saat menunggu koneksi user.
- Saat mencoba WiFi tersimpan, buzzer memberi pola loading pendek berulang; portal WiFiManager tetap dibuat senyap saat user harus mengisi koneksi.
- Identitas `AYB Interface` digunakan secara terpusat oleh boot screen, Serial Monitor, menu About, dan dokumentasi.
- Layar About menampilkan tiga halaman identitas, termasuk `PT AUDIO YUDISTIRO BUANA` pada halaman kedua, yang berganti otomatis setiap 3 detik tanpa blocking.
- Toggle `CNC_ENABLE_MARLIN_CONNECTION` mengendalikan komunikasi UART/polling Marlin selama hardware CNC belum tersambung.
- Untuk uji integrasi SKR saat ini, `CNC_ENABLE_MARLIN_CONNECTION` aktif dan UART Marlin memakai RX GPIO44 serta TX GPIO43.
- Log diagnostik UART Marlin dapat menampilkan command keluar `[MARLIN][TX]` dan respons masuk `[MARLIN][RX]` melalui Serial Monitor.
- Prefetch status mesin otomatis setelah boot mengirim query non-gerak `M203`, `M115`, `M119`, dan `M211` secara round-robin setelah Marlin merespons.
- Menu Machine Ctrl&Status tidak lagi memanggil refresh otomatis saat dibuka sehingga tidak memicu query status tambahan dan jeda masuk menu.
- Area X/Y/Z memakai fallback lokal saat geometry report `M115` belum terbaca, lalu diganti otomatis ketika data Marlin valid masuk.
- `cnc/machine` mengirim Area X/Y/Z melalui field `area_x`, `area_y`, `area_z`, `area_source`, dan `work_area`.
- Standby menampilkan status koneksi `CNC:[OK]` atau `CNC:[X]`.
- Perintah mesin diblokir sampai Marlin berstatus `CONNECTED`.
- Standby screen menampilkan waktu `HH:MM`, koordinat X/Y/Z, job aktif, estimasi, dan status Marlin.
- Baris atas standby memakai format `CNC:[...] | aktivitas` agar status koneksi CNC dan status kerja mesin tidak tercampur.
- Koordinat standby menggunakan `?` sampai posisi nyata diterima dari `M114`.
- Standby screen menampilkan `WiFi:[OK]/WiFi:[X]` dan `MQTT:[OK]/MQTT:[X]` pada satu baris khusus menggunakan font koordinat `6x10`.
- Garis horizontal pada standby memisahkan status WiFi/MQTT dari nama job ketika file sudah dipilih.
- Dialog konfirmasi memiliki aturan tetap: Enter = Yes, Back = No, rotary memilih dan mengeksekusi pilihan.
- SD card browser dengan pemilihan file job.
- Select Job langsung membuka SD card, menampilkan label/path, dan mengurutkan folder sebelum file.
- G-code sender file SD mengirim satu command dan menunggu respons `ok` Marlin sebelum melanjutkan.
- Analisis G-code non-blocking menghitung estimasi `HH:MM:SS` untuk G0/G1/G2/G3/G4.
- Tombol standby `X+/PLAY`, `X-/PAUSE`, dan `Z-/STOP` mengendalikan job dengan long-press pada Start/Stop.
- Preflight Start memeriksa file SD, koneksi Marlin, Home All, pemilihan Work Origin, dan soft endstop.
- Menu Work Origin menampilkan sembilan slot `G54`-`G59.3`; setiap slot memiliki submenu `Pilih Origin` dan `Perbarui Origin`.
- `Pilih Origin` menggunakan dialog konfirmasi standar dan mengaktifkan workspace tanpa menggerakkan mesin.
- `Perbarui Origin` membuka jog X/Y/Z dan baru mengirim `G92 X0 Y0 Z0`, `M500`, serta opsi `M211 S1` setelah konfirmasi akhir.
- Workspace terakhir dimuat ulang otomatis setelah Home All dan job selesai normal selama sesi mesin tetap aktif; reset/power-cycle/stop/error/SD dilepas/disconnect tetap menginvalidasi workspace.
- Pemilihan file mempertahankan status Home dan Work Origin karena proses tersebut tidak menggerakkan mesin; workflow operator dapat memakai `Home All -> G54 -> Pilih Origin -> Select Job -> Start`.
- File contoh `examples/gcode/CEK_ORIGIN_XY.gcode` tersedia untuk memeriksa `X0 Y0` workspace aktif tanpa menurunkan Z; pengujian wajib tanpa mata pahat dan tetap memerlukan konfirmasi operator.
- Menu Machine Ctrl&Status untuk alarm, test buzzer, spindle, feedrate, area kerja, soft endstop, dan sensor homing.
- Machine Ctrl&Status menampilkan `Alarm: OK/ERROR` dan item `Test Buzzer` untuk uji pola alarm lokal tanpa mengirim command ke Marlin.
- Urutan Machine Ctrl&Status memprioritaskan status CNC, alarm, test buzzer, sensor home, dan soft endstop sebelum spindle serta parameter gerak.
- Menu Network untuk WiFi, MQTT, broker, AP setup, waktu `HH:MM`, tanggal `DD/MM/YYYY`, status sinkronisasi, dan reset WiFi.
- Editor IP broker MQTT dari menu Network > Broker dengan input digit `000.000.000.000`, navigasi rotary/Enter/Back, validasi oktet maksimal `255`, dan konfirmasi sebelum simpan.
- Digit aktif pada editor broker memakai highlight negatif seperti pemilihan menu.
- Redraw editor broker dibatasi dan proses MQTT reconnect dijeda saat editor broker aktif agar callback rotary tetap cepat.
- Mode sementara tanpa LCD untuk pengembangan WiFi/MQTT melalui toggle `CNC_HEADLESS_WIFI_MQTT_DEV` di `src/main.cpp`.
- Reset WiFi langsung membuka kembali portal WiFiManager tanpa toggle OFF/ON manual.
- WiFiManager untuk WiFi dan parameter MQTT.
- MQTT lokal memakai PubSubClient dengan konfigurasi terpisah di `src/config/MqttConfig.h`.
- Publish topic `cnc/status`, `cnc/progress`, `cnc/error`, dan `cnc/position`.
- `cnc/status.state` mengirim status koneksi interface dalam huruf kapital, yaitu `ONLINE` atau `OFFLINE`.
- Disconnect MQTT normal mengirim ulang `cnc/network.mqtt` sebagai `DISCONNECTED` sebelum koneksi ditutup.
- Serial Monitor tetap aktif saat pengujian MQTT tanpa Marlin karena `Serial1` ke SKR hanya dibuka ketika fitur koneksi Marlin diaktifkan.
- Publish retained `cnc/machine` berisi state koneksi CNC, activity mesin, spindle, Work Origin, soft endstop, feedrate, dan sensor home.
- Publish retained `cnc/machine` juga membawa area kerja mesin untuk tampilan Data Mesin dashboard.
- `cnc/machine` mengirim status sensor home terstruktur melalui `home`, `home_valid`, dan `home_note` untuk membedakan `OPEN`, `TRIGGERED`, dan `UNKNOWN`.
- `cnc/machine` mengirim Work Origin pilihan interface melalui `work_origin` dan `work_origin_valid` untuk dashboard Node-RED.
- `cnc/progress` berisi nama file job, progress command yang diakui Marlin, dan state proses dengan fallback aman `Belum dipilih`, `0`, dan `IDLE`.
- Placeholder data mesin yang belum tersedia menggunakan `UNKNOWN` dan nilai feedrate `null` tanpa membuat data Marlin palsu.
- `cnc/position` membawa field `valid`, `source`, serta display `?` ketika posisi belum tersedia.
- Jalur simulasi posisi MQTT tersedia tetapi default dinonaktifkan agar tidak menghasilkan data dummy permanen.
- Subscribe topic `cnc/command` dan `cnc/gcode` dalam mode receive-only ke Serial Monitor.
- Topic monitoring lama `cnc/network`, `cnc/time`, dan `cnc/alarm` tetap dipertahankan.
- `cnc/alarm` membawa field `active`, `popup`, `source`, dan `code` agar Node-RED dapat memunculkan pop-up warning/error dari payload yang terstruktur.
- `cnc/alarm` mengirim `WARNING` dengan `code:"HOME_SENSOR_UNKNOWN"` ketika Marlin tersambung tetapi status sensor home/endstop belum terbaca valid.
- `cnc/error` tetap dipertahankan untuk error keras dan kompatibilitas dashboard lama, dengan field `active`, `source`, `code`, dan `message`.
- Subscribe topic `cnc/file/start`, `cnc/file/chunk`, dan `cnc/file/finish` untuk validasi transfer file dari Node-RED.
- Publish ACK non-retained ke `cnc/file/ack` untuk START, CHUNK, dan FINISH.
- Upload G-code MQTT menyimpan data ke file sementara `.part`, memvalidasi total chunk dan ukuran byte, lalu rename ke file final setelah FINISH berhasil.
- ACK START `ready` hanya dikirim setelah file sementara berhasil dibuka di MicroSD.
- ACK CHUNK hanya dikirim setelah seluruh byte chunk berhasil ditulis ke file sementara.
- ACK FINISH `saved` hanya dikirim setelah ukuran file final sesuai dan rename berhasil.
- Retry CHUNK terakhir dan FINISH setelah file tersimpan dibuat idempotent agar timeout/retry Node-RED tidak memproses data dua kali.
- Subscribe topic `cnc/file/list/request` untuk permintaan daftar file job MicroSD dari Node-RED.
- Publish daftar file job ke `cnc/file/list` dalam paket `file_list_begin`, `file_list_item`, dan `file_list_end`.
- Pemindaian daftar file job dilakukan oleh `SdCardReader` dari root `/`, rekursif maksimal 8 tingkat, maksimal 50 file, dan hanya untuk ekstensi `.gcode`, `.gco`, `.gc`, `.nc`, serta `.tap`.
- Topic `cnc/file/select` dipertahankan untuk kompatibilitas, tetapi selalu ditolak dengan `remote_selection_disabled`.
- Subscribe `cnc/file/delete` dan publish `cnc/file/delete/ack` untuk hapus file job MicroSD dengan validasi path dan proteksi file aktif.
- Subscribe `cnc/sd/status/request` dan publish `cnc/sd/status` untuk status MicroSD aktual.
- Upload menolak target yang sudah ada kecuali START membawa `overwrite:true`.
- Hot-swap MicroSD melakukan mount ulang non-blocking, membatalkan upload aktif saat kartu dilepas, dan membersihkan cache daftar file.
- Retry mount MicroSD saat kartu/modul belum ada diberi jeda 30 detik dan build flag `CORE_DEBUG_LEVEL=0` mematikan log internal Arduino core agar tombol/rotary tetap responsif serta Serial Monitor tidak penuh saat SD sengaja dilepas.
- Sinkronisasi waktu NTP WIB.
- Polling Marlin `M114` secara periodik.
- Parsing posisi Marlin X/Y/Z dari respons `M114`.
- Alarm MQTT/buzzer aktif saat koneksi Marlin yang sebelumnya berhasil menjadi `LOST` atau Marlin mengirim `ERROR`.
- Dokumentasi hardware mencakup ESP32-S3 DevKitM-1, BTT SKR V1.4 Turbo, Marlin, DM542, NEMA23, LCD, rotary encoder, push button, MicroSD, WiFi, dan MQTT broker.
- GitHub Actions sudah disiapkan untuk menjalankan `platformio run -e esp32-s3-devkitm-1` saat ada push atau pull request ke branch `main`.
- `ARCHITECTURE.md` sudah diperbarui dengan diagram ASCII dan pembagian modul firmware.
- README menjelaskan fungsi menu utama, submenu, setiap item Machine Ctrl&Status, Network, dan Refresh Status berdasarkan source aktual.
- README mencatat alamat dashboard Node-RED skripsi `http://127.0.0.1:1880/dashboard/cnc` beserta catatan penggunaan IP laptop untuk akses dari perangkat lain.

## Fitur yang Sedang Dikerjakan

- Validasi runtime komunikasi Marlin pada hardware SKR V1.4 Turbo.
- Validasi log UART Marlin untuk memastikan `M203` dan `M115` dibalas sehingga Feedrate serta Area X/Y/Z tidak lagi tampil `?`.
- Validasi prefetch boot agar Machine Ctrl&Status langsung menampilkan parameter yang sudah terbaca tanpa jeda panjang saat menu dibuka.
- Validasi payload `cnc/machine` dan transisi validitas `cnc/position` melalui MQTT Explorer.
- Validasi `work_origin` tetap aktif setelah Home All/job selesai normal, serta kembali `NONE` setelah reset/power-cycle/stop/error/SD dilepas/disconnect.
- Validasi tampilan Area X/Y/Z pada Data Mesin Node-RED dari `cnc/machine.area_x/y/z` atau `cnc/machine.work_area`.
- Validasi tampilan dashboard untuk sensor X/Y/Z dari `cnc/machine.home` dan indikator error ketika `home_valid` bernilai `false`.
- Validasi pop-up Node-RED dari `cnc/alarm.popup`, termasuk level `ERROR` dan `WARNING`.
- Validasi seluruh pemetaan payload MQTT pada Node-RED Dashboard 2.0.
- Validasi koneksi WiFi/MQTT pada mode sementara tanpa LCD.
- Validasi editor IP broker MQTT dari LCD, termasuk input `192.168.250.009`, simpan Preferences, dan reconnect MQTT.
- Validasi ulang respons rotary dan flicker LCD setelah throttling redraw editor broker.
- Validasi upload file MQTT dari Node-RED sampai file hasil upload muncul kembali pada daftar MicroSD.
- Validasi hot-swap MicroSD, delete file MQTT, overwrite upload, dan status SD pada hardware nyata.
- Validasi respons tombol/rotary saat modul MicroSD tidak dipasang dan saat kartu dipasang kembali.
- Validasi aturan dashboard agar `cnc/status` menjadi sumber utama status interface/MQTT saat device offline.
- Validasi pergantian halaman serta tombol keluar layar About pada LCD/input hardware.
- Validasi item `Alarm` dan `Test Buzzer` pada Machine Ctrl&Status di hardware nyata.
- Validasi transisi status `WAIT`, `DISCONNECTED`, `CONNECTED`, `LOST`, dan `ERROR` pada hardware nyata.
- Penyempurnaan machine status berbasis data nyata firmware Marlin.
- Pemanfaatan posisi Marlin untuk workflow job yang lebih lengkap.
- Validasi G-code sender, controlled pause/stop, progress, serta estimasi pada hardware nyata.
- Validasi workflow `Home All -> G54 -> Pilih Origin -> Select Job -> Start` dan pastikan pemilihan file tidak menghapus indikator Work Origin.
- Validasi navigasi submenu tiap workspace serta kontrol konfirmasi Enter=Yes, Back=No, rotary=pilih, dan klik rotary=eksekusi pilihan.
- Validasi simpan, power-cycle, dan pemanggilan ulang origin `G54`-`G59.3` pada Marlin/SKR nyata.
- Perancangan validasi dan izin eksekusi command/G-code MQTT yang aman.
- Hasil GitHub Actions masih perlu dicek langsung setelah workflow ini dipush ke GitHub.

## Daftar Bug yang Masih Ada

- Broker Mosquitto memakai IP laptop; koneksi akan gagal jika DHCP mengubah alamat laptop dan konfigurasi belum diperbarui.
- Jika ESP32 mati mendadak, Last Will hanya menjamin `cnc/status` berubah `OFFLINE`; dashboard harus menurunkan status MQTT dari `cnc/status`, bukan hanya dari retained `cnc/network`.
- Selama portal WiFiManager aktif, loop UI menunggu proses konfigurasi atau timeout portal; buzzer dibuat diam selama fase tunggu ini.
- Pada mode sementara tanpa LCD, instruksi portal WiFiManager tidak tampil di layar sehingga Serial Monitor menjadi sumber status utama.
- Jika credential WiFi tersimpan salah atau jaringan tidak tersedia, firmware tetap menunggu `WIFI_TIMEOUT_MS` sebelum membuka portal WiFiManager.
- Editor broker hanya menerima alamat IPv4 numerik; hostname broker tetap harus diganti melalui WiFiManager/Preferences jika dibutuhkan.
- G-code sender belum diuji pada SKR/Marlin dan sistem gerak nyata.
- Pause/Stop memakai barrier `M400` dan bukan emergency stop.
- Estimasi belum memperhitungkan akselerasi, pergantian tool, dan waktu spindle mencapai RPM.
- Flag Home/Origin preflight masih berdasarkan command yang dikirim interface, belum bukti independen dari Marlin.
- Persistensi `G54`-`G59.3` melalui `M500` belum diuji setelah power-cycle pada SKR V1.4 Turbo.
- Payload `cnc/command` baru dicetak ke Serial dan belum dieksekusi.
- Payload `cnc/gcode` baru dicetak ke Serial dan belum dikirim ke Marlin atau SD card.
- Upload file G-code lewat MQTT sudah menyimpan file ke MicroSD, tetapi belum memilih job otomatis dan belum dieksekusi.
- Publikasi daftar file MQTT hanya membaca file yang sudah ada di MicroSD; penyimpanan upload MQTT ke MicroSD masih ditunda.
- Dashboard tidak dapat memilih job CNC; topic lama `cnc/file/select` sekarang ditolak agar selected job lokal tetap dilindungi.
- Validasi sensor homing dan parameter mesin masih perlu diuji di hardware.
- Level suara, rangkaian driver, dan pola passive buzzer terbaru belum divalidasi pada hardware nyata.
- Detail wiring final antar ESP32-S3, SKR, driver DM542, sensor, dan aktuator masih perlu divalidasi terhadap rangkaian nyata.
- Edit nilai Area X/Y/Z pada Machine Ctrl&Status masih lokal dan belum diterapkan ke Marlin.
- GitHub Actions hanya mengecek build firmware. Workflow ini tidak melakukan upload dan tidak bisa dianggap sebagai pengujian hardware.

## TODO Prioritas Berikutnya

- Uji `M114` langsung dengan SKR/Marlin dan pastikan `cnc/position` berubah sesuai gerakan mesin.
- Uji boot dengan SKR tersambung dan pastikan Serial Monitor menampilkan query prefetch `M203`, `M115`, `M119`, dan `M211` bertahap.
- Verifikasi `cnc/position` menampilkan `valid:false`, nilai `null`, dan display `?` sebelum Marlin tersambung.
- Verifikasi `cnc/machine` mengikuti state CNC dan data status mesin.
- Verifikasi dashboard menampilkan `work_origin` hanya ketika `work_origin_valid:true`.
- Verifikasi Data Mesin dashboard menampilkan Area X/Y/Z dari payload `cnc/machine`.
- Uji Enter, Back, dan klik rotary untuk keluar dari layar About.
- Aktifkan `CNC_ENABLE_MARLIN_CONNECTION` setelah SKR tersambung, lalu uji seluruh transisi status CNC.
- Uji koneksi yang terputus setelah pernah aktif dan verifikasi `cnc/alarm` berubah ke `ERROR`.
- Uji alarm kembali ke `INFO` setelah Marlin merespons lagi.
- Uji pop-up dashboard dari `cnc/alarm.popup:true` dan pastikan retained message lama tidak memunculkan pop-up berulang saat dashboard dibuka ulang.
- Uji passive buzzer GPIO 38 dan pastikan rangkaian driver sesuai arus buzzer.
- Uji pola loading WiFi tersimpan dan pastikan portal WiFiManager tetap senyap setelah Reset WiFi.
- Uji `CNC_HEADLESS_WIFI_MQTT_DEV` dengan LCD dilepas atau bermasalah, lalu pastikan `cnc/status` dan topic MQTT lain muncul di broker.
- Uji menu Network > Broker dengan input broker laptop aktual, konfirmasi simpan, reset ESP32, dan pastikan nilai broker bertahan dari Preferences.
- Uji item `Test Buzzer` pada Machine Ctrl&Status dan pastikan pola alarm terdengar tanpa mengganggu input.
- Uji sender terlebih dahulu tanpa tool dan benda kerja menggunakan file G-code pendek.
- Verifikasi sender tidak mengirim command berikutnya sebelum respons `ok`.
- Uji Start, Pause, Resume, controlled Stop, completion, timeout, dan repeat job.
- Uji `G28`, buka `G54`, jalankan `Perbarui Origin`, matikan-nyalakan mesin, `G28`, lalu gunakan `G54 -> Pilih Origin` dan verifikasi nol benda kerja tidak berubah.
- Salin `examples/gcode/CEK_ORIGIN_XY.gcode` ke MicroSD dan uji tiap workspace yang digunakan tanpa mata pahat; pastikan Z tetap di home, X/Y berhenti tepat pada origin, lalu mesin kembali home setelah 10 detik.
- Bandingkan estimasi dengan durasi nyata untuk menentukan faktor koreksi mesin.
- Uji field `file`, `progress`, dan `state` pada `cnc/progress` setelah memilih file SD card.
- Verifikasi tujuh kelompok dashboard Node-RED terhadap field MQTT yang didokumentasikan.
- Uji `cnc/file/start -> cnc/file/chunk -> cnc/file/finish` dari Node-RED dan pastikan ACK diterima pada `cnc/file/ack`.
- Uji retry CHUNK dan FINISH dari Node-RED untuk memastikan ACK ulang tidak memproses data dua kali.
- Setelah ACK FINISH `saved`, uji `cnc/file/list/request` dan pastikan file upload baru muncul pada daftar MicroSD.
- Uji `cnc/file/list/request` dari Node-RED dan pastikan paket begin, item, end, serta error SD dikirim sesuai kondisi MicroSD.
- Uji `cnc/file/select` dari Node-RED dan pastikan `cnc/file/select/ack` selalu mengirim `remote_selection_disabled`.
- Uji `cnc/file/delete` untuk file root, file subfolder, file tidak ada, target folder, path berisi `..`, dan file yang sedang dipilih lokal.
- Uji upload nama file yang sudah ada dengan `overwrite:false` dan `overwrite:true`.
- Uji boot tanpa MicroSD, pasang MicroSD setelah menyala, lepas saat file list, dan lepas saat upload berjalan.
- Pastikan Node-RED memaksa `Status MQTT` menjadi `DISCONNECTED` saat `cnc/status.connected` bernilai `false`.
- Jaga README, PROJECT_STATUS, dan CHANGELOG tetap sinkron setiap ada perubahan fitur/workflow.
- Lengkapi dokumentasi wiring/pinout jika hardware final sudah dikunci.
- Tentukan whitelist command MQTT dan workflow konfirmasi sebelum eksekusi mesin diaktifkan.
- Lanjutkan protokol upload G-code dengan validasi ukuran, checksum, dan penyimpanan SD card setelah komunikasi START/CHUNK/FINISH terbukti stabil.
- Tetapkan DHCP reservation/static IP untuk PC Mosquitto agar alamat broker stabil.
- Tentukan command dan workflow aman jika Area X/Y/Z nantinya perlu diterapkan ke firmware Marlin.
- Setelah perubahan dipush, lihat tab Actions di GitHub untuk memastikan workflow berjalan sesuai konfigurasi.
