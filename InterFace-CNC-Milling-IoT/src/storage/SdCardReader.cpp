#include "SdCardReader.h"

#include <algorithm>
#include <esp_log.h>

namespace {
  constexpr unsigned long SD_CARD_CHECK_INTERVAL_MS = 1000;
  constexpr unsigned long SD_CARD_MOUNT_RETRY_MS = 30000;

  uint16_t readLe16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
  }

  uint32_t readLe32(const uint8_t *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
  }

  bool hasBootSignature(const uint8_t *sector) {
    return sector[510] == 0x55 && sector[511] == 0xAA;
  }

  bool isLikelyFatBootSector(const uint8_t *sector) {
    if (!hasBootSignature(sector)) {
      return false;
    }

    uint16_t bytesPerSector = readLe16(sector + 11);
    bool validSectorSize = bytesPerSector == 512 ||
                           bytesPerSector == 1024 ||
                           bytesPerSector == 2048 ||
                           bytesPerSector == 4096;

    return validSectorSize &&
           sector[13] != 0 &&
           readLe16(sector + 14) != 0 &&
           sector[16] != 0;
  }

  bool isFatPartitionType(uint8_t type) {
    switch (type) {
      case 0x01: // FAT12
      case 0x04: // FAT16 < 32 MB
      case 0x06: // FAT16
      case 0x0B: // FAT32
      case 0x0C: // FAT32 LBA
      case 0x0E: // FAT16 LBA
        return true;
      default:
        return false;
    }
  }

  String parseFatVolumeLabel(const uint8_t *labelBytes) {
    char label[12];
    for (uint8_t i = 0; i < 11; ++i) {
      char c = (char)labelBytes[i];
      label[i] = (c >= 32 && c <= 126) ? c : ' ';
    }
    label[11] = '\0';

    String result(label);
    result.trim();

    if (result == "NO NAME") {
      return String();
    }

    return result;
  }

  String normalizeDirectoryPath(const String &path) {
    String normalized = path.length() ? path : "/";
    normalized.replace("//", "/");

    if (!normalized.startsWith("/")) {
      normalized = "/" + normalized;
    }

    if (normalized.length() > 1 && normalized.endsWith("/")) {
      normalized.remove(normalized.length() - 1);
    }

    return normalized;
  }

  String parentDirectoryPath(const String &path) {
    String normalized = normalizeDirectoryPath(path);
    if (normalized == "/") {
      return "/";
    }

    int slashIndex = normalized.lastIndexOf('/');
    if (slashIndex <= 0) {
      return "/";
    }

    return normalized.substring(0, slashIndex);
  }

  String basenameFromPath(const String &path) {
    String normalized = path;
    normalized.replace("//", "/");

    if (normalized.length() > 1 && normalized.endsWith("/")) {
      normalized.remove(normalized.length() - 1);
    }

    int slashIndex = normalized.lastIndexOf('/');
    if (slashIndex >= 0 && slashIndex < (int)normalized.length() - 1) {
      return normalized.substring(slashIndex + 1);
    }

    return normalized;
  }

  String normalizeFilePath(const String &path) {
    String normalized = path.length() ? path : "/";
    normalized.replace("//", "/");

    if (!normalized.startsWith("/")) {
      normalized = "/" + normalized;
    }

    return normalized;
  }

  bool isIgnoredStorageEntry(const String &name) {
    String lower = name;
    lower.trim();
    lower.toLowerCase();

    return lower.length() == 0 ||
           lower.startsWith(".") ||
           lower.startsWith("._") ||
           lower == "system volume information" ||
           lower == "$recycle.bin" ||
           lower == "recycler" ||
           lower == "found.000" ||
           lower == "indexervolumeguid" ||
           lower == "wp settings.dat" ||
           lower == "desktop.ini";
  }

  bool isSupportedJobPath(const String &path) {
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".gcode") ||
           lower.endsWith(".gco") ||
           lower.endsWith(".gc") ||
           lower.endsWith(".nc") ||
           lower.endsWith(".tap");
  }

  String uploadBackupPathFor(const String &finalPath) {
    return finalPath + ".bak";
  }

  bool jobEntryPathLess(const SdCardEntry &left, const SdCardEntry &right) {
    String leftPath = left.path;
    String rightPath = right.path;
    leftPath.toLowerCase();
    rightPath.toLowerCase();
    return leftPath < rightPath;
  }
}

SdCardReader::SdCardReader(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin)
  : _csPin(csPin), _sckPin(sckPin), _misoPin(misoPin), _mosiPin(mosiPin) {
}

bool SdCardReader::begin() {
  // Library SD ESP32 mencetak error berulang saat kartu sengaja dilepas.
  // Status tetap dikelola oleh SdCardReader agar Serial Monitor tidak penuh.
  esp_log_level_set("sd_diskio", ESP_LOG_NONE);
  return mountCard();
}

bool SdCardReader::update() {
  unsigned long now = millis();
  if (now - _lastCardCheckMs < SD_CARD_CHECK_INTERVAL_MS) {
    return false;
  }
  _lastCardCheckMs = now;

  bool wasReady = _ready;

  if (_ready) {
    File root = SD.open("/");
    bool stillReady = root && root.isDirectory() && SD.cardType() != CARD_NONE;
    if (root) {
      root.close();
    }

    if (!stillReady) {
      unmountCard("sd_removed");
    }
  } else {
    if (now < _nextMountAttemptMs) {
      return false;
    }

    _cardState = CardState::Mounting;
    mountCard();
  }

  return wasReady != _ready;
}

SdCardReader::CardState SdCardReader::cardState() const {
  return _cardState;
}

const char *SdCardReader::cardStatusText() const {
  switch (_cardState) {
    case CardState::Absent: return "absent";
    case CardState::Mounting: return "mounting";
    case CardState::Ready: return "ready";
    case CardState::Error: return "error";
  }

  return "error";
}

const String &SdCardReader::lastErrorReason() const {
  return _lastErrorReason;
}

bool SdCardReader::mountCard() {
  SPI.begin(_sckPin, _misoPin, _mosiPin, _csPin);

  if (!SD.begin(_csPin)) {
    Serial.println("SD Card gagal diinisialisasi");
    _ready = false;
    _cardState = CardState::Error;
    _volumeLabel = "";
    _lastErrorReason = "mount_failed";
    _nextMountAttemptMs = millis() + SD_CARD_MOUNT_RETRY_MS;
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("SD Card tidak terdeteksi");
    _ready = false;
    _cardState = CardState::Absent;
    _volumeLabel = "";
    _lastErrorReason = "sd_not_ready";
    _nextMountAttemptMs = millis() + SD_CARD_MOUNT_RETRY_MS;
    SD.end();
    return false;
  }

  _ready = true;
  _cardState = CardState::Ready;
  _lastErrorReason = "";
  _nextMountAttemptMs = 0;
  _volumeLabel = readVolumeLabel();
  Serial.println("SD Card siap digunakan");
  if (_volumeLabel.length()) {
    Serial.print("Nama SD Card: ");
    Serial.println(_volumeLabel);
  }
  return true;
}

void SdCardReader::unmountCard(const char *reason) {
  if (_uploadFile) {
    _uploadFile.close();
  }

  _ready = false;
  _volumeLabel = "";
  _selectedJobFile = "";
  _lastErrorReason = reason != nullptr ? reason : "sd_not_ready";
  _cardState = _lastErrorReason == "sd_removed" ? CardState::Absent : CardState::Error;
  _nextMountAttemptMs = millis() + SD_CARD_MOUNT_RETRY_MS;
  SD.end();
  Serial.print("[SD] status=");
  Serial.println(_lastErrorReason);
}

bool SdCardReader::isReady() const {
  return _ready;
}

const String &SdCardReader::volumeLabel() const {
  return _volumeLabel;
}

void SdCardReader::printCardInfo() {
  if (!_ready) {
    Serial.println("SD Card belum siap");
    return;
  }

  Serial.print("SD Card Type: ");
  Serial.println(cardTypeName(SD.cardType()));
  Serial.print("Volume Label: ");
  Serial.println(_volumeLabel.length() ? _volumeLabel : "SD");
  Serial.printf("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}

String SdCardReader::readVolumeLabel() {
  if (!_ready) {
    return String();
  }

  uint8_t sector[512];
  if (!SD.readRAW(sector, 0)) {
    return String();
  }

  if (!isLikelyFatBootSector(sector)) {
    uint32_t bootSector = 0;
    for (uint8_t i = 0; i < 4; ++i) {
      const uint8_t *entry = sector + 446 + (i * 16);
      uint8_t partitionType = entry[4];
      uint32_t partitionStart = readLe32(entry + 8);

      if (isFatPartitionType(partitionType) && partitionStart > 0) {
        bootSector = partitionStart;
        break;
      }
    }

    if (bootSector == 0 || !SD.readRAW(sector, bootSector)) {
      return String();
    }
  }

  if (!isLikelyFatBootSector(sector)) {
    return String();
  }

  bool fat32 = readLe16(sector + 17) == 0 && readLe16(sector + 22) == 0;
  return parseFatVolumeLabel(sector + (fat32 ? 71 : 43));
}

void SdCardReader::listFiles(const char *dirname, uint8_t levels) {
  if (!_ready) {
    Serial.println("SD Card belum siap");
    return;
  }

  listDir(SD, dirname, levels);
}

std::vector<String> SdCardReader::listFileNames(const char *dirname, uint8_t levels, size_t maxItems) {
  std::vector<String> items;

  if (!_ready) {
    Serial.println("SD Card belum siap");
    return items;
  }

  collectFileNames(SD, dirname, levels, maxItems, items);
  return items;
}

std::vector<SdCardEntry> SdCardReader::listEntries(const char *dirname, size_t maxItems, bool includeParent) {
  std::vector<SdCardEntry> entries;

  if (!_ready) {
    Serial.println("SD Card belum siap");
    return entries;
  }

  String directory = normalizeDirectoryPath(dirname != nullptr ? String(dirname) : String("/"));
  File root = SD.open(directory.c_str());
  if (!root || !root.isDirectory()) {
    Serial.print("Folder SD tidak dapat dibuka: ");
    Serial.println(directory);
    if (root) {
      root.close();
    }
    return entries;
  }

  if (includeParent && directory != "/" && entries.size() < maxItems) {
    SdCardEntry parentEntry;
    parentEntry.path = parentDirectoryPath(directory);
    parentEntry.name = "..";
    parentEntry.isDirectory = true;
    parentEntry.size = 0;
    entries.push_back(parentEntry);
  }

  File file = root.openNextFile();
  while (file && entries.size() < maxItems) {
    SdCardEntry entry;
    entry.path = String(file.path());
    if (entry.path.length() == 0) {
      String fileName = String(file.name());
      entry.path = directory == "/" ? "/" + basenameFromPath(fileName) : directory + "/" + basenameFromPath(fileName);
    } else if (!entry.path.startsWith("/")) {
      entry.path = directory == "/" ? "/" + entry.path : directory + "/" + entry.path;
    }

    entry.path = normalizeDirectoryPath(entry.path);
    entry.name = basenameFromPath(entry.path);
    entry.isDirectory = file.isDirectory();
    entry.size = file.size();
    entries.push_back(entry);

    file.close();
    file = root.openNextFile();
  }

  root.close();
  return entries;
}

std::vector<SdCardEntry> SdCardReader::listJobFilesRecursive(uint8_t maxDepth, size_t maxFiles) {
  std::vector<SdCardEntry> entries;

  if (!_ready) {
    Serial.println("SD Card belum siap");
    return entries;
  }

  Serial.println("[SD][FILE LIST] scanning");
  collectJobFiles(SD, "/", 0, maxDepth, maxFiles, entries);
  std::sort(entries.begin(), entries.end(), jobEntryPathLess);

  Serial.print("[SD][FILE LIST] found=");
  Serial.println(entries.size());
  return entries;
}

SdJobFileValidation SdCardReader::validateJobFilePath(const String &path) {
  SdJobFileValidation result;
  String requested = path;
  requested.trim();
  result.path = requested;

  Serial.print("[SD][FILE SELECT] validating path=");
  Serial.println(requested);

  auto reject = [&result](const char *reason) {
    result.valid = false;
    result.reason = reason;
    Serial.print("[SD][FILE SELECT] rejected reason=");
    Serial.println(reason);
    return result;
  };

  if (!_ready) {
    return reject("sd_not_ready");
  }

  if (requested.length() == 0 ||
      !requested.startsWith("/") ||
      requested.length() > 255 ||
      requested.indexOf("..") >= 0) {
    return reject("invalid_path");
  }

  result.path = normalizeFilePath(requested);

  if (!SD.exists(result.path.c_str())) {
    return reject("file_not_found");
  }

  File file = SD.open(result.path.c_str(), FILE_READ);
  if (!file) {
    return reject("open_failed");
  }

  if (file.isDirectory()) {
    file.close();
    return reject("not_a_file");
  }

  if (!isSupportedJobPath(result.path)) {
    file.close();
    return reject("unsupported_extension");
  }

  result.size = file.size();
  file.close();
  result.label = result.path.startsWith("/") ? result.path.substring(1) : result.path;
  result.valid = true;
  result.reason = "";

  Serial.print("[SD][FILE SELECT] valid size=");
  Serial.println(result.size);
  return result;
}

SdUploadPathValidation SdCardReader::validateUploadFileName(const String &fileName) {
  SdUploadPathValidation result;
  String normalized = fileName;
  normalized.trim();
  result.fileName = normalized;

  if (!_ready) {
    result.reason = "sd_not_ready";
    return result;
  }

  if (normalized.length() == 0 ||
      normalized.length() > 64 ||
      normalized.indexOf("..") >= 0 ||
      normalized.indexOf('/') >= 0 ||
      normalized.indexOf('\\') >= 0) {
    result.reason = "invalid_filename";
    return result;
  }

  String finalPath = "/" + normalized;
  if (!isSupportedJobPath(finalPath)) {
    result.reason = "unsupported_extension";
    return result;
  }

  result.valid = true;
  result.reason = "";
  result.finalPath = finalPath;
  result.temporaryPath = finalPath + ".part";
  return result;
}

bool SdCardReader::beginUploadFile(const String &temporaryPath, const String &finalPath) {
  if (!_ready || temporaryPath.length() == 0) {
    return false;
  }

  if (_uploadFile) {
    _uploadFile.close();
  }

  if (SD.exists(temporaryPath.c_str())) {
    SD.remove(temporaryPath.c_str());
  }

  String backupPath = uploadBackupPathFor(finalPath);
  if (SD.exists(backupPath.c_str())) {
    SD.remove(backupPath.c_str());
  }

  _uploadFile = SD.open(temporaryPath.c_str(), FILE_WRITE);
  if (!_uploadFile || _uploadFile.isDirectory()) {
    if (_uploadFile) {
      _uploadFile.close();
    }
    return false;
  }

  Serial.print("[SD][UPLOAD] temporary file opened path=");
  Serial.println(temporaryPath);
  return true;
}

bool SdCardReader::writeUploadChunk(const uint8_t *data, size_t length, size_t &written) {
  written = 0;

  if (!_ready || !_uploadFile || data == nullptr || length == 0) {
    return false;
  }

  written = _uploadFile.write(data, length);
  return written == length;
}

bool SdCardReader::finishUploadFile(const String &temporaryPath, const String &finalPath, size_t expectedSize, size_t &savedSize, String &reason) {
  savedSize = 0;
  reason = "";

  if (!_ready) {
    reason = "sd_not_ready";
    return false;
  }

  if (_uploadFile) {
    _uploadFile.flush();
    _uploadFile.close();
  }

  File temp = SD.open(temporaryPath.c_str(), FILE_READ);
  if (!temp || temp.isDirectory()) {
    if (temp) {
      temp.close();
    }
    reason = "open_failed";
    return false;
  }

  savedSize = temp.size();
  temp.close();

  if (savedSize != expectedSize) {
    reason = "size_mismatch";
    return false;
  }

  String backupPath = uploadBackupPathFor(finalPath);
  bool hasExistingFinal = SD.exists(finalPath.c_str());
  bool backupCreated = false;

  if (hasExistingFinal) {
    if (SD.exists(backupPath.c_str())) {
      SD.remove(backupPath.c_str());
    }

    if (!SD.rename(finalPath.c_str(), backupPath.c_str())) {
      reason = "replace_failed";
      return false;
    }
    backupCreated = true;
  }

  if (!SD.rename(temporaryPath.c_str(), finalPath.c_str())) {
    if (backupCreated) {
      SD.rename(backupPath.c_str(), finalPath.c_str());
    }
    reason = hasExistingFinal ? "replace_failed" : "rename_failed";
    return false;
  }

  File finalFile = SD.open(finalPath.c_str(), FILE_READ);
  if (!finalFile || finalFile.isDirectory()) {
    if (finalFile) {
      finalFile.close();
    }
    if (backupCreated) {
      SD.remove(finalPath.c_str());
      SD.rename(backupPath.c_str(), finalPath.c_str());
    }
    reason = "open_failed";
    return false;
  }

  savedSize = finalFile.size();
  finalFile.close();

  if (savedSize != expectedSize) {
    if (backupCreated) {
      SD.remove(finalPath.c_str());
      SD.rename(backupPath.c_str(), finalPath.c_str());
    }
    reason = "size_mismatch";
    return false;
  }

  if (backupCreated) {
    SD.remove(backupPath.c_str());
  }

  Serial.print("[SD][UPLOAD] saved path=");
  Serial.print(finalPath);
  Serial.print(" size=");
  Serial.println(savedSize);
  return true;
}

void SdCardReader::abortUploadFile(const String &temporaryPath) {
  if (_uploadFile) {
    _uploadFile.close();
  }

  if (_ready && temporaryPath.length() > 0 && SD.exists(temporaryPath.c_str())) {
    SD.remove(temporaryPath.c_str());
  }
}

void SdCardReader::cleanupTemporaryUploadFiles(const String &finalPath, const String &temporaryPath) {
  if (!_ready) {
    return;
  }

  if (temporaryPath.length() > 0 && SD.exists(temporaryPath.c_str())) {
    SD.remove(temporaryPath.c_str());
  }

  String backupPath = uploadBackupPathFor(finalPath);
  if (backupPath.length() > 0 && SD.exists(backupPath.c_str())) {
    SD.remove(backupPath.c_str());
  }
}

SdDeleteValidation SdCardReader::validateDeletePath(const String &filePath) {
  SdDeleteValidation result;
  String requested = filePath;
  requested.trim();
  result.path = normalizeFilePath(requested);

  Serial.print("[SD][DELETE] checking path=");
  Serial.println(requested);

  auto reject = [&result](const char *reason) {
    result.valid = false;
    result.reason = reason;
    Serial.print("[SD][DELETE] rejected reason=");
    Serial.println(reason);
    return result;
  };

  if (!_ready) {
    result.path = requested;
    return reject("sd_not_ready");
  }

  if (requested.length() == 0 ||
      !requested.startsWith("/") ||
      requested.indexOf("..") >= 0 ||
      requested.endsWith(".part")) {
    result.path = requested;
    return reject("invalid_path");
  }

  if (!isSupportedJobPath(result.path)) {
    return reject("unsupported_extension");
  }

  if (!SD.exists(result.path.c_str())) {
    return reject("file_not_found");
  }

  File file = SD.open(result.path.c_str(), FILE_READ);
  if (!file) {
    return reject("file_not_found");
  }

  if (file.isDirectory()) {
    file.close();
    return reject("target_is_directory");
  }

  file.close();
  result.valid = true;
  result.reason = "";
  return result;
}

bool SdCardReader::deleteJobFile(const String &filePath, String &reason) {
  reason = "";
  SdDeleteValidation validation = validateDeletePath(filePath);
  if (!validation.valid) {
    reason = validation.reason;
    return false;
  }

  if (!SD.remove(validation.path.c_str())) {
    reason = "delete_failed";
    Serial.print("[SD][DELETE] rejected reason=");
    Serial.println(reason);
    return false;
  }

  Serial.print("[SD][DELETE] deleted path=");
  Serial.println(validation.path);
  return true;
}

void SdCardReader::clearSelectedJobFile() {
  _selectedJobFile = "";
}

void SdCardReader::runDiagnostics() {
  if (!_ready) {
    Serial.println("Tes SD Card dibatalkan karena kartu belum siap");
    return;
  }

  printCardInfo();
  listFiles("/", 0);

  createDir("/cnc_test");
  listFiles("/", 0);
  removeDir("/cnc_test");
  listFiles("/", 1);

  writeFile("/hello.txt", "Hello ");
  appendFile("/hello.txt", "World!\n");
  readFile("/hello.txt");

  deleteFile("/foo.txt");
  renameFile("/hello.txt", "/foo.txt");
  readFile("/foo.txt");

  testFileIO("/test.txt");
  printCardInfo();
}

bool SdCardReader::createDir(const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  bool success = SD.mkdir(path);
  Serial.println(success ? "Dir created" : "mkdir failed");
  return success;
}

bool SdCardReader::removeDir(const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  bool success = SD.rmdir(path);
  Serial.println(success ? "Dir removed" : "rmdir failed");
  return success;
}

bool SdCardReader::readFile(const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = SD.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return false;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }

  Serial.println();
  file.close();
  return true;
}

bool SdCardReader::writeFile(const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }

  bool success = file.print(message);
  Serial.println(success ? "File written" : "Write failed");
  file.close();
  return success;
}

bool SdCardReader::appendFile(const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return false;
  }

  bool success = file.print(message);
  Serial.println(success ? "Message appended" : "Append failed");
  file.close();
  return success;
}

bool SdCardReader::renameFile(const char *from, const char *to) {
  Serial.printf("Renaming file %s to %s\n", from, to);
  bool success = SD.rename(from, to);
  Serial.println(success ? "File renamed" : "Rename failed");
  return success;
}

bool SdCardReader::deleteFile(const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  bool success = SD.remove(path);
  Serial.println(success ? "File deleted" : "Delete failed");
  return success;
}

void SdCardReader::testFileIO(const char *path) {
  File file = SD.open(path);
  static uint8_t buffer[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t elapsed = 0;

  if (file) {
    len = file.size();
    size_t fileLength = len;
    start = millis();

    while (len) {
      size_t toRead = len > sizeof(buffer) ? sizeof(buffer) : len;
      file.read(buffer, toRead);
      len -= toRead;
    }

    elapsed = millis() - start;
    Serial.printf("%zu bytes read for %lu ms\n", fileLength, elapsed);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }

  file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  start = millis();
  for (size_t i = 0; i < 2048; i++) {
    file.write(buffer, sizeof(buffer));
  }

  elapsed = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, elapsed);
  file.close();
}

bool SdCardReader::fileExists(const char *path) {
  if (!_ready) {
    Serial.println("SD Card belum siap");
    return false;
  }

  bool exists = SD.exists(path);
  Serial.printf("File %s: %s\n", path, exists ? "tersedia" : "tidak ditemukan");
  return exists;
}

bool SdCardReader::selectJobFile(const char *path) {
  if (!fileExists(path)) {
    Serial.println("File job gagal dipilih");
    return false;
  }

  _selectedJobFile = path;
  Serial.print("File job SD dipilih: ");
  Serial.println(_selectedJobFile);
  return true;
}

bool SdCardReader::hasSelectedJobFile() const {
  return _selectedJobFile.length() > 0;
}

const String &SdCardReader::selectedJobFile() const {
  return _selectedJobFile;
}

void SdCardReader::printSelectedJobFile() const {
  if (!hasSelectedJobFile()) {
    Serial.println("Belum ada file job SD yang dipilih");
    return;
  }

  Serial.print("File job SD aktif: ");
  Serial.println(_selectedJobFile);
}

File SdCardReader::openFileForRead(const char *path) const {
  if (!_ready || path == nullptr || path[0] == '\0') {
    return File();
  }
  return SD.open(path, FILE_READ);
}

std::vector<String> SdCardReader::previewFileLines(const char *path, size_t maxLines, size_t charsPerLine) {
  std::vector<String> result;

  if (!_ready) {
    Serial.println("SD Card belum siap (preview)");
    return result;
  }

  File file = SD.open(path);
  if (!file) {
    Serial.printf("Gagal membuka file untuk preview: %s\n", path);
    return result;
  }

  String line;
  size_t linesRead = 0;

  while (file.available() && linesRead < maxLines) {
    char c = file.read();
    if (c == '\n' || line.length() >= (int)charsPerLine) {
      // push the current line (trim if necessary)
      if (line.length() > (int)charsPerLine) line = line.substring(0, charsPerLine);
      result.push_back(line);
      line = "";
      linesRead++;
      // if line ended with newline, continue; else, the next char is part of next segment
    } else if (c != '\r') {
      line += c;
    }
  }

  // push remaining partial line if any and we haven't reached maxLines
  if (linesRead < maxLines && line.length() > 0) {
    if (line.length() > (int)charsPerLine) line = line.substring(0, charsPerLine);
    result.push_back(line);
  }

  file.close();
  return result;
}

const char *SdCardReader::cardTypeName(uint8_t cardType) const {
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "UNKNOWN";
  }
}

void SdCardReader::listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }

  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());

      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }

    file = root.openNextFile();
  }

  root.close();
}

void SdCardReader::collectFileNames(fs::FS &fs, const char *dirname, uint8_t levels, size_t maxItems, std::vector<String> &items) {
  if (items.size() >= maxItems) {
    return;
  }

  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    return;
  }

  File file = root.openNextFile();
  while (file && items.size() < maxItems) {
    String name = file.name();

    if (file.isDirectory()) {
      items.push_back("[D] " + name);

      if (levels > 0 && items.size() < maxItems) {
        collectFileNames(fs, file.path(), levels - 1, maxItems, items);
      }
    } else {
      items.push_back(name);
    }

    file.close();
    file = root.openNextFile();
  }

  root.close();
}

void SdCardReader::collectJobFiles(fs::FS &fs, const String &dirname, uint8_t depth, uint8_t maxDepth, size_t maxFiles, std::vector<SdCardEntry> &items) {
  if (items.size() >= maxFiles || depth > maxDepth) {
    return;
  }

  String directory = normalizeDirectoryPath(dirname);
  File root = fs.open(directory.c_str());
  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    return;
  }

  File file = root.openNextFile();
  while (file && items.size() < maxFiles) {
    String path = String(file.path());
    String name = basenameFromPath(path.length() ? path : String(file.name()));
    if (path.length() == 0) {
      path = directory == "/" ? "/" + name : directory + "/" + name;
    } else if (!path.startsWith("/")) {
      path = directory == "/" ? "/" + path : directory + "/" + path;
    }
    path = normalizeFilePath(path);

    if (!isIgnoredStorageEntry(name)) {
      if (file.isDirectory()) {
        if (depth < maxDepth) {
          collectJobFiles(fs, normalizeDirectoryPath(path), depth + 1, maxDepth, maxFiles, items);
        }
      } else if (isSupportedJobPath(path)) {
        SdCardEntry entry;
        entry.path = path;
        entry.name = name;
        entry.isDirectory = false;
        entry.size = file.size();
        items.push_back(entry);
      }
    }

    file.close();
    file = root.openNextFile();
  }

  root.close();
}
