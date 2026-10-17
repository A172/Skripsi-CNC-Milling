#ifndef SD_CARD_READER_H
#define SD_CARD_READER_H

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <vector>

struct SdCardEntry {
  String path;
  String name;
  bool isDirectory = false;
  size_t size = 0;
};

struct SdJobFileValidation {
  bool valid = false;
  String reason;
  String label;
  String path;
  size_t size = 0;
};

struct SdUploadPathValidation {
  bool valid = false;
  String reason;
  String fileName;
  String finalPath;
  String temporaryPath;
};

struct SdDeleteValidation {
  bool valid = false;
  String reason;
  String path;
};

class SdCardReader {
  public:
    enum class CardState {
      Absent,
      Mounting,
      Ready,
      Error
    };

    // Membuat objek pembaca SD card dengan pin SPI yang ditentukan dari PinConfig.
    SdCardReader(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin);

    // Menginisialisasi bus SPI dan mount SD card.
    bool begin();

    // Mengecek hot-swap MicroSD secara non-blocking.
    bool update();

    // Mengambil status internal MicroSD.
    CardState cardState() const;

    // Mengambil status MicroSD dalam format teks untuk MQTT/Serial.
    const char *cardStatusText() const;

    // Alasan kegagalan terakhir pada MicroSD.
    const String &lastErrorReason() const;

    // Mengecek apakah SD card berhasil diinisialisasi dan siap digunakan.
    bool isReady() const;

    // Mengambil nama volume SD card jika tersedia dari label FAT.
    const String &volumeLabel() const;

    // Menampilkan tipe, ukuran, total ruang, dan ruang terpakai SD card ke Serial Monitor.
    void printCardInfo();

    // Menampilkan daftar file/folder pada direktori tertentu.
    void listFiles(const char *dirname = "/", uint8_t levels = 0);

    // Mengambil daftar file/folder agar bisa ditampilkan di LCD/menu.
    std::vector<String> listFileNames(const char *dirname = "/", uint8_t levels = 0, size_t maxItems = 20);

    // Mengambil isi folder sebagai entry terstruktur untuk browser job.
    std::vector<SdCardEntry> listEntries(const char *dirname = "/", size_t maxItems = 50, bool includeParent = true);

    // Memindai file job G-code dari root SD secara rekursif untuk dikirim lewat MQTT.
    std::vector<SdCardEntry> listJobFilesRecursive(uint8_t maxDepth = 8, size_t maxFiles = 50);

    // Memvalidasi path file job dari MQTT tanpa memilih atau menjalankan job.
    SdJobFileValidation validateJobFilePath(const String &path);

    // Memvalidasi nama upload MQTT dan membentuk path final serta path sementara.
    SdUploadPathValidation validateUploadFileName(const String &fileName);

    // Membuka file sementara untuk upload MQTT dalam mode tulis baru.
    bool beginUploadFile(const String &temporaryPath, const String &finalPath);

    // Menulis chunk upload MQTT ke file sementara yang sedang terbuka.
    bool writeUploadChunk(const uint8_t *data, size_t length, size_t &written);

    // Flush, close, validasi ukuran, lalu rename file sementara menjadi file final.
    bool finishUploadFile(const String &temporaryPath, const String &finalPath, size_t expectedSize, size_t &savedSize, String &reason);

    // Menutup upload aktif dan menghapus file sementara jika transfer gagal.
    void abortUploadFile(const String &temporaryPath);

    // Menghapus file upload lama/backup/part yang tersisa jika tersedia.
    void cleanupTemporaryUploadFiles(const String &finalPath, const String &temporaryPath);

    // Memvalidasi path delete dari dashboard.
    SdDeleteValidation validateDeletePath(const String &filePath);

    // Menghapus file job dari MicroSD setelah validasi AppController.
    bool deleteJobFile(const String &filePath, String &reason);

    // Menghapus pilihan job lokal saat MicroSD dilepas.
    void clearSelectedJobFile();

    // Menjalankan rangkaian tes baca/tulis/hapus file untuk validasi SD card.
    void runDiagnostics();

    // Membuat folder baru pada SD card.
    bool createDir(const char *path);

    // Menghapus folder dari SD card.
    bool removeDir(const char *path);

    // Membaca isi file dan menampilkannya ke Serial Monitor.
    bool readFile(const char *path);

    // Menulis file baru atau menimpa file yang sudah ada.
    bool writeFile(const char *path, const char *message);

    // Menambahkan teks ke akhir file.
    bool appendFile(const char *path, const char *message);

    // Mengubah nama atau lokasi file.
    bool renameFile(const char *from, const char *to);

    // Menghapus file dari SD card.
    bool deleteFile(const char *path);

    // Menguji performa baca/tulis file pada SD card.
    void testFileIO(const char *path);

    // Mengecek apakah file tersedia pada SD card.
    bool fileExists(const char *path);

    // Memilih file G-code/job yang akan dipakai oleh sistem CNC.
    bool selectJobFile(const char *path);

    // Mengecek apakah sudah ada file job yang dipilih.
    bool hasSelectedJobFile() const;

    // Mengambil path file job yang sedang dipilih.
    const String &selectedJobFile() const;

    // Menampilkan file job yang sedang dipilih ke Serial Monitor.
    void printSelectedJobFile() const;

    // Membuka file untuk parser/job sender tanpa mengekspos konfigurasi SPI.
    File openFileForRead(const char *path) const;

  // Ambil preview (beberapa baris pertama) dari sebuah file untuk ditampilkan di UI
  // Mengembalikan vector<String> berisi baris (atau potongan) file, maksimal maxLines
  std::vector<String> previewFileLines(const char *path, size_t maxLines = 5, size_t charsPerLine = 20);

  private:
    uint8_t _csPin;
    uint8_t _sckPin;
    uint8_t _misoPin;
    uint8_t _mosiPin;
    bool _ready = false;
    CardState _cardState = CardState::Absent;
    unsigned long _lastCardCheckMs = 0;
    unsigned long _nextMountAttemptMs = 0;
    String _lastErrorReason;
    File _uploadFile;
    String _selectedJobFile;
    String _volumeLabel;

    // Mengubah kode tipe kartu dari library SD menjadi teks.
    const char *cardTypeName(uint8_t cardType) const;

    // Membaca label volume dari boot sector FAT.
    String readVolumeLabel();

    // Mount ulang kartu SD.
    bool mountCard();

    // Lepas mount dan reset status internal kartu.
    void unmountCard(const char *reason);

    // Fungsi internal rekursif untuk menampilkan isi direktori.
    void listDir(fs::FS &fs, const char *dirname, uint8_t levels);

    // Fungsi internal rekursif untuk mengambil nama file/folder.
    void collectFileNames(fs::FS &fs, const char *dirname, uint8_t levels, size_t maxItems, std::vector<String> &items);

    // Fungsi internal rekursif untuk mengambil file job G-code.
    void collectJobFiles(fs::FS &fs, const String &dirname, uint8_t depth, uint8_t maxDepth, size_t maxFiles, std::vector<SdCardEntry> &items);
};

#endif
