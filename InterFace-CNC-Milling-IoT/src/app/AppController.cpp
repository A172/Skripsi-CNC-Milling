#include "AppController.h"
#include <SD.h>
#include <algorithm>
#include <cstring>
#include <math.h>
#include <Preferences.h>
#include <WiFi.h>

namespace {
  // Penghubung callback dari modul input ke objek aplikasi yang sedang aktif.
  AppController *activeApp = nullptr;

  void handleButtonPressed(uint8_t buttonNumber) {
    if (activeApp != nullptr) {
      activeApp->onButtonPressed(buttonNumber);
    }
  }

  void handleEncoderTurned(int8_t direction) {
    if (activeApp != nullptr) {
      activeApp->onEncoderTurned(direction);
    }
  }

  void handleEncoderPressed() {
    if (activeApp != nullptr) {
      activeApp->onEncoderPressed();
    }
  }

  String basenameForLcd(const String &path) {
    String normalized = path;
    normalized.replace("//", "/");

    int slashIndex = normalized.lastIndexOf('/');
    if (slashIndex >= 0 && slashIndex < (int)normalized.length() - 1) {
      return normalized.substring(slashIndex + 1);
    }

    return normalized;
  }

  String formatDurationHms(uint32_t totalSeconds) {
    uint32_t hours = totalSeconds / 3600UL;
    uint8_t minutes = (totalSeconds % 3600UL) / 60UL;
    uint8_t seconds = totalSeconds % 60UL;
    char text[16];
    snprintf(
      text,
      sizeof(text),
      "%02lu:%02u:%02u",
      static_cast<unsigned long>(hours),
      minutes,
      seconds
    );
    return String(text);
  }

  String normalizeSdDirectory(const String &path) {
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

  String normalizeSdFilePath(const String &path) {
    String normalized = path;
    normalized.trim();
    normalized.replace("//", "/");

    if (!normalized.startsWith("/")) {
      normalized = "/" + normalized;
    }

    return normalized;
  }

  String parentSdDirectory(const String &path) {
    String normalized = normalizeSdDirectory(path);
    if (normalized == "/") {
      return "/";
    }

    int slashIndex = normalized.lastIndexOf('/');
    if (slashIndex <= 0) {
      return "/";
    }

    return normalized.substring(0, slashIndex);
  }

  bool isSupportedJobFile(const String &path) {
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".gcode") ||
           lower.endsWith(".gco") ||
           lower.endsWith(".gc") ||
           lower.endsWith(".nc") ||
           lower.endsWith(".tap");
  }

  bool isIgnoredSdBrowserEntry(const String &name) {
    String lower = name;
    lower.trim();
    lower.toLowerCase();

    if (lower == "..") {
      return false;
    }

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

  void logMarlinTx(const char *command) {
    if (!AppConfig::ENABLE_MARLIN_UART_LOG) {
      return;
    }

    Serial.print("[MARLIN][TX] ");
    Serial.println(command);
  }

  void logMarlinRx(const String &line) {
    if (!AppConfig::ENABLE_MARLIN_UART_LOG) {
      return;
    }

    Serial.print("[MARLIN][RX] ");
    Serial.println(line);
  }

  String fileListTitle(const String &volumeLabel, const String &directory) {
    String label = volumeLabel;
    label.trim();

    String title = "SD";
    if (label.length() > 0) {
      title += " ";
      title += label;
    }

    if (directory == "/") {
      return title + ":/";
    }

    return title + ":" + directory;
  }

  String entrySortKey(const SdCardEntry &entry) {
    String key = entry.name;
    key.toLowerCase();
    return key;
  }

  bool entryNameLess(const SdCardEntry &left, const SdCardEntry &right) {
    return entrySortKey(left) < entrySortKey(right);
  }

  unsigned int clampUnsigned(int value, unsigned int minValue, unsigned int maxValue) {
    if (value < (int)minValue) {
      return minValue;
    }

    if (value > (int)maxValue) {
      return maxValue;
    }

    return (unsigned int)value;
  }

  unsigned int clampFeedrate(int value, unsigned int maxValue) {
    return clampUnsigned(value, AppConfig::MACHINE_FEEDRATE_MIN_MM_S, maxValue);
  }

  bool parseAxisValue(const String &line, char axis, float &value) {
    String axisToken;
    axisToken += axis;
    axisToken += ':';

    int axisIndex = line.indexOf(axisToken);
    int valueStart = axisIndex >= 0 ? axisIndex + axisToken.length() : -1;
    if (axisIndex < 0) {
      axisIndex = line.indexOf(axis);
      valueStart = axisIndex + 1;
    }

    if (axisIndex < 0 || axisIndex + 1 >= (int)line.length()) {
      return false;
    }

    while (valueStart < (int)line.length() && line[valueStart] == ' ') {
      valueStart++;
    }

    int valueEnd = valueStart;
    while (valueEnd < (int)line.length()) {
      char c = line[valueEnd];
      if (!isDigit(c) && c != '.' && c != '-') {
        break;
      }
      valueEnd++;
    }

    if (valueEnd == valueStart) {
      return false;
    }

    value = line.substring(valueStart, valueEnd).toFloat();
    return true;
  }

  bool parseGeometryAxisValue(const String &line, const char *section, const char *bound, char axis, float &value) {
    int sectionIndex = line.indexOf(section);
    if (sectionIndex < 0) {
      return false;
    }

    int boundIndex = line.indexOf(bound, sectionIndex);
    if (boundIndex < 0) {
      return false;
    }

    int objectEnd = line.indexOf('}', boundIndex);
    if (objectEnd < 0) {
      return false;
    }

    String object = line.substring(boundIndex, objectEnd);
    return parseAxisValue(object, axis, value);
  }

  String feedrateItem(const char *label, unsigned int value, bool loaded, bool editing) {
    String item(label);
    item += editing ? ":*" : ": ";
    if (!loaded) {
      item += "?";
      return item;
    }

    item += value;
    item += "mm/s";
    return item;
  }

  void applyDefaultMachineWorkArea(
    unsigned int &areaX,
    unsigned int &areaY,
    unsigned int &areaZ
  ) {
    areaX = AppConfig::MACHINE_WORK_AREA_DEFAULT_X_MM;
    areaY = AppConfig::MACHINE_WORK_AREA_DEFAULT_Y_MM;
    areaZ = AppConfig::MACHINE_WORK_AREA_DEFAULT_Z_MM;
  }

  String millimeterItem(const char *label, unsigned int value, bool loaded, bool editing) {
    String item(label);
    item += editing ? ":*" : ": ";
    if (!loaded) {
      item += "?";
      return item;
    }

    item += value;
    item += "mm";
    return item;
  }

  constexpr size_t MACHINE_ITEM_MARLIN = 0;
  constexpr size_t MACHINE_ITEM_ALARM = 1;
  constexpr size_t MACHINE_ITEM_BUZZER_TEST = 2;
  constexpr size_t MACHINE_ITEM_HOME_X = 3;
  constexpr size_t MACHINE_ITEM_HOME_Y = 4;
  constexpr size_t MACHINE_ITEM_HOME_Z = 5;
  constexpr size_t MACHINE_ITEM_SOFT_ENDSTOP = 6;
  constexpr size_t MACHINE_ITEM_SPINDLE = 7;
  constexpr size_t MACHINE_ITEM_FEED_XY = 8;
  constexpr size_t MACHINE_ITEM_FEED_Z = 9;
  constexpr size_t MACHINE_ITEM_AREA_X = 10;
  constexpr size_t MACHINE_ITEM_AREA_Y = 11;
  constexpr size_t MACHINE_ITEM_AREA_Z = 12;
  constexpr size_t MACHINE_ITEM_REFRESH = 13;

  constexpr size_t NETWORK_ITEM_WIFI = 0;
  constexpr size_t NETWORK_ITEM_SSID = 1;
  constexpr size_t NETWORK_ITEM_IP = 2;
  constexpr size_t NETWORK_ITEM_MQTT = 3;
  constexpr size_t NETWORK_ITEM_MQTT_LINK = 4;
  constexpr size_t NETWORK_ITEM_BROKER = 5;
  constexpr size_t NETWORK_ITEM_AP = 6;
  constexpr size_t NETWORK_ITEM_TIME = 7;
  constexpr size_t NETWORK_ITEM_DATE = 8;
  constexpr size_t NETWORK_ITEM_SYNC = 9;
  constexpr size_t NETWORK_ITEM_RESET = 10;

  constexpr const char *NETWORK_PREFS_NAMESPACE = "cnc_network";
  constexpr const char *NETWORK_PREF_WIFI = "wifi";
  constexpr const char *NETWORK_PREF_MQTT = "mqtt";
  constexpr const char *NETWORK_PREF_MQTT_BROKER = "mqtt_host";
  constexpr const char *NETWORK_PREF_MQTT_PORT = "mqtt_port";
  constexpr const char *NETWORK_PREF_MQTT_TOPIC = "mqtt_topic";
  constexpr const char *NETWORK_PREF_MQTT_USER = "mqtt_user";
  constexpr const char *NETWORK_PREF_MQTT_PASS = "mqtt_pass";

  String normalizedMqttText(String value, const char *fallback, size_t maxLen, bool allowEmpty) {
    value.trim();

    if (value.length() == 0 && !allowEmpty) {
      value = fallback;
    }

    if (value.length() > maxLen) {
      value.remove(maxLen);
    }

    return value;
  }

  uint16_t normalizedMqttPort(uint32_t port) {
    if (port == 0 || port > 65535) {
      return MqttConfig::PORT;
    }

    return (uint16_t)port;
  }

  uint16_t parseMqttPort(const String &value) {
    String trimmed = value;
    trimmed.trim();
    return normalizedMqttPort((uint32_t)trimmed.toInt());
  }

  String stripMqttBrokerOctetPadding(const String &value) {
    String result;
    int start = 0;

    for (uint8_t group = 0; group < 4; ++group) {
      int end = value.indexOf('.', start);
      if (end < 0) {
        end = value.length();
      }

      String octet = value.substring(start, end);
      octet.trim();
      int numeric = octet.toInt();
      if (group > 0) {
        result += ".";
      }
      result += String(numeric);
      start = end + 1;
    }

    return result;
  }

  bool readMqttBrokerIpDigits(const String &broker, uint8_t digits[12]) {
    String text = broker;
    text.trim();
    int start = 0;

    for (uint8_t group = 0; group < 4; ++group) {
      int end = text.indexOf('.', start);
      if (group < 3 && end < 0) {
        return false;
      }
      if (group == 3) {
        end = text.length();
      }

      String octet = text.substring(start, end);
      octet.trim();
      if (octet.length() == 0 || octet.length() > 3) {
        return false;
      }

      for (uint8_t i = 0; i < octet.length(); ++i) {
        if (!isDigit(octet.charAt(i))) {
          return false;
        }
      }

      int value = octet.toInt();
      if (value < 0 || value > 255) {
        return false;
      }

      digits[group * 3] = (value / 100) % 10;
      digits[group * 3 + 1] = (value / 10) % 10;
      digits[group * 3 + 2] = value % 10;
      start = end + 1;
    }

    return true;
  }

  String wifiNetworkStatus(bool enabled, bool connected) {
    if (!enabled) {
      return String("WiFi: OFF");
    }

    if (connected) {
      return String("WiFi terkoneksi !!!");
    }

    // Versi singkat agar muat di satu baris LCD 128x64.
    return String("WiFi gagal konek !!!");
  }

  std::vector<String> mainMenuItems() {
    return {
      "Move & Jog",
      "Select Job",
      "Machine Ctrl&Status",
      "Network",
      "About"
    };
  }

  const char *workspaceCode(uint8_t index) {
    static const char *const codes[] = {
      "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3"
    };
    return index < AppConfig::WORK_COORDINATE_COUNT ? codes[index] : "G54";
  }

  static_assert(AppConfig::WORK_COORDINATE_COUNT == 9, "Marlin menyediakan G54-G59.3 sebanyak 9 workspace");
}

// --------------------
// Penyusunan modul hardware
// --------------------

AppController::AppController()
  : _buttons(
      PinConfig::BTN1,
      PinConfig::BTN2,
      PinConfig::BTN3,
      PinConfig::BTN4,
      PinConfig::BTN5,
      PinConfig::BTN6,
      PinConfig::BTN7,
      PinConfig::BTN8
    ),
    _encoder(
      PinConfig::ENCODER_CLK,
      PinConfig::ENCODER_DT,
      PinConfig::ENCODER_SW
    ),
    _buzzer(
      PinConfig::BUZZER,
      AppConfig::BUZZER_PWM_CHANNEL,
      AppConfig::BUZZER_ALARM_REPEAT_MS
    ),
    _lcd(),
    _menu(_lcd),
    _sdCard(
      PinConfig::SD_CS,
      PinConfig::SD_SCK,
      PinConfig::SD_MISO,
      PinConfig::SD_MOSI
    ),
    _jogAxisIndex(0),
    _isJogAdjusting(false),
    _encoderMovedWhilePressed(false) {
}

void AppController::pushMenuState(const std::vector<String> &items, size_t selected) {
  _menuHistory.push_back({_menu.title(), items, selected});
}

bool AppController::popMenuState() {
  if (_menuHistory.empty()) return false;
  MenuState entry = _menuHistory.back();
  _menuHistory.pop_back();
  _menu.showMenu(entry.title.c_str(), entry.items, entry.selected);
  return true;
}

void AppController::showStandbyScreen() {
  _uiState = UiState::Standby;
  _standbyScreenDrawn = true;

  if (!_displayEnabled) {
    _lastStandbyNetworkStatus = standbyNetworkStatusText();
    return;
  }

  String jobName = computeJobName();
  String standbyJobName = jobName.length() ? "Job: " + jobName : String();
  String eta = computeEta();
  String time = getLocalTimeString();
  const char *jobNamePtr = standbyJobName.length() ? standbyJobName.c_str() : nullptr;
  const char *etaPtr = eta.length() ? eta.c_str() : nullptr;
  const char *timePtr = time.length() ? time.c_str() : nullptr;
  String machineStatus = standbyMarlinStatusText();
  const char *machineStatusPtr = machineStatus.length() ? machineStatus.c_str() : nullptr;
  String machineActivity = machineActivityText();
  const char *machineActivityPtr = machineActivity.length() ? machineActivity.c_str() : nullptr;
  String networkStatus = standbyNetworkStatusText();
  const char *networkStatusPtr = networkStatus.length() ? networkStatus.c_str() : nullptr;
  _lastStandbyNetworkStatus = networkStatus;
  bool positionValid =
    marlinConnectionState() == MarlinConnectionState::Connected &&
    _positionValid;

  _menu.showStandby(
    _posX,
    _posY,
    _posZ,
    etaPtr,
    jobNamePtr,
    computeProgress(),
    timePtr,
    machineStatusPtr,
    networkStatusPtr,
    positionValid,
    machineActivityPtr
  );
}

void AppController::showMainMenuScreen(size_t selected) {
  if (!_displayEnabled) {
    return;
  }

  _menuHistory.clear();
  _uiState = UiState::Menu;
  _menu.showMainMenu(mainMenuItems(), selected);
}

void AppController::showSubMenu(const char *title, const std::vector<String> &items) {
  if (!_displayEnabled) {
    return;
  }

  pushMenuState(_menu.items(), _menu.selectedIndex());
  _uiState = UiState::Menu;
  _menu.showMenu(title, items, 0);
}

std::vector<String> AppController::workOriginItems() const {
  std::vector<String> items;
  items.reserve(AppConfig::WORK_COORDINATE_COUNT);
  for (uint8_t index = 0; index < AppConfig::WORK_COORDINATE_COUNT; ++index) {
    items.push_back(workspaceCode(index));
  }
  return items;
}

void AppController::invalidateWorkOrigin() {
  _originConfigured = false;
  _workspaceSelected = false;
}

void AppController::requestWorkspaceSelection(uint8_t index) {
  if (index >= AppConfig::WORK_COORDINATE_COUNT) {
    showInfoScreen("WORK ORIGIN", "Workspace tidak valid");
    return;
  }

  if (_job.isActive()) {
    showJobControlMessage("Job sedang aktif");
    return;
  }

  if (!_homeCycleIssued) {
    showInfoScreen("WORK ORIGIN", "Home All dulu");
    return;
  }

  if (!canSendMarlinCommand()) {
    showCncUnavailable(UiState::Menu);
    return;
  }

  _pendingWorkspaceIndex = index;
  String title = String(workspaceCode(index)) + " ORIGIN";
  String message = String("Gunakan ") + workspaceCode(index) + "?";
  showActionConfirm(
    PendingAction::SelectWorkspace,
    title.c_str(),
    message.c_str()
  );
}

bool AppController::activateWorkspace(uint8_t index) {
  if (!AppConfig::ENABLE_WORK_COORDINATE_CONTROL ||
      index >= AppConfig::WORK_COORDINATE_COUNT ||
      !canSendMarlinCommand()) {
    return false;
  }

  // G28 mereset position_shift tetapi Marlin dapat tetap mengingat indeks workspace.
  // G53 memaksa kembali ke machine space sehingga pemilihan G54-G59.3 selalu memuat
  // ulang offset yang tersimpan, termasuk ketika slot yang sama dipilih setelah homing.
  Serial1.println("M400");
  Serial1.println("G53");
  Serial1.println(workspaceCode(index));
  Serial1.println("M211 S1");
  Serial1.println("M114");

  _activeWorkspaceIndex = index;
  _workspaceSelected = true;
  _originConfigured = true;
  _softEndstopEnabled = true;
  _softEndstopLoaded = true;
  _lastM211Request = millis();
  _lastMarlinStatusPoll = _lastM211Request;
  return true;
}

void AppController::beginWorkspaceOriginUpdate(uint8_t index) {
  if (index >= AppConfig::WORK_COORDINATE_COUNT) {
    showInfoScreen("WORK ORIGIN", "Workspace tidak valid");
    return;
  }

  if (_job.isActive()) {
    showJobControlMessage("Job sedang aktif");
    return;
  }

  if (!_homeCycleIssued) {
    showInfoScreen("WORK ORIGIN", "Home All dulu");
    return;
  }

  if (!activateWorkspace(index)) {
    showCncUnavailable(UiState::Menu);
    return;
  }

  enterSetOrigin();
}

void AppController::selectWorkspace(uint8_t index, UiState returnState) {
  if (!activateWorkspace(index)) {
    showCncUnavailable(returnState);
    return;
  }

  String message = String(workspaceCode(index)) + " aktif";
  showInfoScreen(
    "WORK ORIGIN",
    message.c_str(),
    returnState,
    AppConfig::CONFIRM_RESULT_MESSAGE_MS
  );
}

void AppController::showAboutScreen() {
  if (!_displayEnabled) {
    return;
  }

  _uiState = UiState::About;
  _aboutPageIndex = 0;
  _aboutLastPageChange = millis();
  _lcd.showAbout(
    AppConfig::FIRMWARE_NAME,
    AppConfig::FIRMWARE_VERSION,
    AppConfig::FIRMWARE_AUTHOR,
    AppConfig::FIRMWARE_AUTHOR_ID,
    AppConfig::FIRMWARE_ORGANIZATION,
    AppConfig::FIRMWARE_LAST_UPDATED,
    _aboutPageIndex
  );
}

void AppController::updateAboutScreen() {
  if (!_displayEnabled || _uiState != UiState::About) {
    return;
  }

  unsigned long now = millis();
  if (now - _aboutLastPageChange < AppConfig::ABOUT_PAGE_INTERVAL_MS) {
    return;
  }
  _aboutLastPageChange = now;
  _aboutPageIndex = (_aboutPageIndex + 1) % AppConfig::ABOUT_PAGE_COUNT;

  _lcd.showAbout(
    AppConfig::FIRMWARE_NAME,
    AppConfig::FIRMWARE_VERSION,
    AppConfig::FIRMWARE_AUTHOR,
    AppConfig::FIRMWARE_AUTHOR_ID,
    AppConfig::FIRMWARE_ORGANIZATION,
    AppConfig::FIRMWARE_LAST_UPDATED,
    _aboutPageIndex
  );
}

void AppController::exitAboutScreen() {
  _uiState = UiState::Menu;
  _menu.redraw();
}

bool AppController::isMarlinResponding() const {
  if (!_marlinConnectionEnabled || !_marlinEverResponded) {
    return false;
  }

  return millis() - _lastMarlinResponseMs <= AppConfig::MARLIN_RESPONSE_TIMEOUT_MS;
}

AppController::MarlinConnectionState AppController::marlinConnectionState() const {
  if (!_marlinConnectionEnabled) {
    return MarlinConnectionState::Off;
  }

  if (_marlinErrorActive) {
    return MarlinConnectionState::Error;
  }

  if (isMarlinResponding()) {
    return MarlinConnectionState::Connected;
  }

  if (_marlinEverResponded) {
    return MarlinConnectionState::Lost;
  }

  if (_marlinMonitorStartedAt == 0 ||
      millis() - _marlinMonitorStartedAt <= AppConfig::MARLIN_RESPONSE_TIMEOUT_MS) {
    return MarlinConnectionState::Waiting;
  }

  return MarlinConnectionState::Disconnected;
}

bool AppController::canSendMarlinCommand() const {
  return marlinConnectionState() == MarlinConnectionState::Connected;
}

void AppController::showCncUnavailable(UiState returnState) {
  const char *message = "Marlin belum terhubung";
  switch (marlinConnectionState()) {
    case MarlinConnectionState::Off: message = "Koneksi CNC OFF"; break;
    case MarlinConnectionState::Waiting: message = "Menunggu Marlin"; break;
    case MarlinConnectionState::Disconnected: message = "Marlin belum terhubung"; break;
    case MarlinConnectionState::Lost: message = "Koneksi terputus"; break;
    case MarlinConnectionState::Error: message = "Marlin error"; break;
    case MarlinConnectionState::Connected: return;
  }

  if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
    _buzzer.play(BuzzerHandler::Cue::Warning);
  }
  showInfoScreen(
    "CNC CONNECTION",
    message,
    returnState,
    AppConfig::CONFIRM_RESULT_MESSAGE_MS
  );
}

String AppController::marlinStatusText() const {
  switch (marlinConnectionState()) {
    case MarlinConnectionState::Off: return String("OFF");
    case MarlinConnectionState::Waiting: return String("WAITING");
    case MarlinConnectionState::Disconnected: return String("DISCONNECTED");
    case MarlinConnectionState::Connected: return String("CONNECTED");
    case MarlinConnectionState::Lost: return String("LOST");
    case MarlinConnectionState::Error: return String("ERROR");
  }

  return String("UNKNOWN");
}

String AppController::machineActivityText() const {
  if (_job.state() == GcodeJobController::State::Error ||
      marlinConnectionState() == MarlinConnectionState::Lost ||
      marlinConnectionState() == MarlinConnectionState::Error) {
    return String("ERROR");
  }

  if (marlinConnectionState() != MarlinConnectionState::Connected) {
    return String("OFF");
  }

  switch (_job.state()) {
    case GcodeJobController::State::Starting:
    case GcodeJobController::State::Running:
    case GcodeJobController::State::Completing:
      return String("RUNNING");

    case GcodeJobController::State::Pausing:
    case GcodeJobController::State::Paused:
      return String("PAUSED");

    case GcodeJobController::State::Stopping:
      return String("STOPPING");

    case GcodeJobController::State::Idle:
    case GcodeJobController::State::Analyzing:
    case GcodeJobController::State::Ready:
    case GcodeJobController::State::Completed:
    case GcodeJobController::State::Stopped:
      return String("IDLE");

    case GcodeJobController::State::Error:
      return String("ERROR");
  }

  return String("IDLE");
}

String AppController::currentUiStateName() const {
  switch (_uiState) {
    case UiState::Boot: return String("boot");
    case UiState::Standby: return String("standby");
    case UiState::Menu: return String("menu");
    case UiState::Info: return String("info");
    case UiState::FileList: return String("file_list");
    case UiState::ConfirmSelect: return String("confirm_select");
    case UiState::ConfirmAction: return String("confirm_action");
    case UiState::SetOrigin: return String("set_origin");
    case UiState::BrokerEditor: return String("broker_editor");
    case UiState::MachineStatus: return String("machine_status");
    case UiState::NetworkStatus: return String("network_status");
    case UiState::About: return String("about");
  }

  return String("unknown");
}

String AppController::softEndstopStatusText() const {
  if (!_softEndstopLoaded) {
    return String("?");
  }

  return _softEndstopEnabled ? String("ON") : String("OFF");
}

bool AppController::isSystemAlarmActive() const {
  MarlinConnectionState cncState = marlinConnectionState();
  return cncState == MarlinConnectionState::Lost ||
         cncState == MarlinConnectionState::Error ||
         _job.state() == GcodeJobController::State::Error;
}

String AppController::alarmStatusText() const {
  return isSystemAlarmActive() ? String("ERROR") : String("OK");
}

String AppController::alarmMessageText() const {
  MarlinConnectionState cncState = marlinConnectionState();
  if (_job.state() == GcodeJobController::State::Error) {
    String error = _job.errorMessage();
    return error.length() > 0 ? error : String("G-code job error");
  }
  if (cncState == MarlinConnectionState::Lost) {
    return String("Marlin connection lost");
  }
  if (cncState == MarlinConnectionState::Error) {
    return String("Marlin error");
  }

  return String("OK");
}

String AppController::standbyMarlinStatusText() const {
  return marlinConnectionState() == MarlinConnectionState::Connected
    ? String("CNC:[OK]")
    : String("CNC:[X]");
}

String AppController::standbyNetworkStatusText() {
  const char *wifiStatus =
    (_wifiEnabled && _wifi.isConnected()) ? "OK" : "X";
  const char *mqttStatus =
    (_mqttEnabled && _cloud.isConnected()) ? "OK" : "X";

  char status[24];
  snprintf(status, sizeof(status), "WiFi:[%s] MQTT:[%s]", wifiStatus, mqttStatus);
  return String(status);
}

void AppController::refreshStandbyNetworkStatus() {
  if (_uiState != UiState::Standby) {
    return;
  }

  String current = standbyNetworkStatusText();
  if (current == _lastStandbyNetworkStatus) {
    return;
  }

  showStandbyScreen();
}

std::vector<String> AppController::machineStatusItems() const {
  String softEndstop = String("SoftEnd: ") + softEndstopStatusText();

  return {
    String("CNC: ") + marlinStatusText(),
    String("Alarm: ") + alarmStatusText(),
    String("Test Buzzer"),
    String("Home X: ") + _homeXStatus,
    String("Home Y: ") + _homeYStatus,
    String("Home Z: ") + _homeZStatus,
    softEndstop,
    String("Spindle: ") + (_spindleOn ? "ON" : "OFF"),
    feedrateItem("Feed XY", _machineFeedrateXY, _machineFeedrateLoaded, _machineFeedrateEditing && _machineStatusSelected == MACHINE_ITEM_FEED_XY),
    feedrateItem("Feed Z", _machineFeedrateZ, _machineFeedrateLoaded, _machineFeedrateEditing && _machineStatusSelected == MACHINE_ITEM_FEED_Z),
    millimeterItem("Area X", _machineWorkAreaX, _machineWorkAreaLoaded, _machineWorkAreaEditing && _machineStatusSelected == MACHINE_ITEM_AREA_X),
    millimeterItem("Area Y", _machineWorkAreaY, _machineWorkAreaLoaded, _machineWorkAreaEditing && _machineStatusSelected == MACHINE_ITEM_AREA_Y),
    millimeterItem("Area Z", _machineWorkAreaZ, _machineWorkAreaLoaded, _machineWorkAreaEditing && _machineStatusSelected == MACHINE_ITEM_AREA_Z),
    String("Refresh Status")
  };
}

void AppController::showMachineStatusScreen() {
  _uiState = UiState::MachineStatus;

  std::vector<String> items = machineStatusItems();
  if (_machineStatusSelected >= items.size()) {
    _machineStatusSelected = items.empty() ? 0 : items.size() - 1;
  }

  const size_t pageSize = 5;
  if (_machineStatusSelected >= _machineStatusOffset + pageSize) {
    _machineStatusOffset = _machineStatusSelected - pageSize + 1;
  } else if (_machineStatusSelected < _machineStatusOffset) {
    _machineStatusOffset = _machineStatusSelected;
  }

  _lcd.showMenu(
    "MACHINE CTRL & STATUS",
    items,
    _machineStatusSelected,
    _machineStatusOffset
  );
}

void AppController::refreshMachineStatus() {
  _machineFeedrateLoaded = false;
  applyDefaultMachineWorkArea(_machineWorkAreaX, _machineWorkAreaY, _machineWorkAreaZ);
  _machineWorkAreaLoaded = true;
  _machineWorkAreaFromFallback = true;
  _softEndstopLoaded = false;
  _homeXStatus = "?";
  _homeYStatus = "?";
  _homeZStatus = "?";
  _lastM203Request = 0;
  _lastM115Request = 0;
  _lastM119Request = 0;
  _lastM211Request = 0;

  requestMachineFeedrateStatus(true);
  requestMachineGeometryStatus(true);
  requestHomeSensorStatus(true);
  requestSoftEndstopStatus(true);

  if (_uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  }
}

void AppController::handleMachineStatusSelect() {
  if (_machineStatusSelected == MACHINE_ITEM_SPINDLE) {
    showActionConfirm(
      PendingAction::SpindleToggle,
      "CONFIRM SPINDLE",
      _spindleOn ? "Matikan spindle?" : "Nyalakan spindle?"
    );
    return;
  }

  if (_machineStatusSelected == MACHINE_ITEM_MARLIN) {
    refreshMachineStatus();
    return;
  }

  if (_machineStatusSelected == MACHINE_ITEM_ALARM) {
    String title = String("ALARM: ") + alarmStatusText();
    showInfoScreen(
      title.c_str(),
      alarmMessageText().c_str(),
      UiState::MachineStatus,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (_machineStatusSelected == MACHINE_ITEM_BUZZER_TEST) {
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
      _buzzer.play(BuzzerHandler::Cue::Alarm);
      showInfoScreen(
        "TEST BUZZER",
        "Alarm pattern",
        UiState::MachineStatus,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
    } else {
      showInfoScreen(
        "TEST BUZZER",
        "Buzzer OFF",
        UiState::MachineStatus,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
    }
    return;
  }

  if (_machineStatusSelected == MACHINE_ITEM_SOFT_ENDSTOP) {
    requestSoftEndstopStatus(true);
    showMachineStatusScreen();
    return;
  }

  if (_machineStatusSelected == MACHINE_ITEM_FEED_XY ||
      _machineStatusSelected == MACHINE_ITEM_FEED_Z) {
    if (!_machineFeedrateLoaded) {
      requestMachineFeedrateStatus(true);
      showMachineStatusScreen();
      return;
    }

    if (_machineFeedrateEditing) {
      _machineFeedrateEditing = false;
      if (!applyMachineFeedrate()) {
        return;
      }
    } else {
      _machineFeedrateEditing = true;
    }

    showMachineStatusScreen();
    return;
  }

  if (_machineStatusSelected >= MACHINE_ITEM_AREA_X &&
      _machineStatusSelected <= MACHINE_ITEM_AREA_Z) {
    if (!_machineWorkAreaLoaded) {
      requestMachineGeometryStatus(true);
      showMachineStatusScreen();
      return;
    }

    _machineWorkAreaEditing = !_machineWorkAreaEditing;
    showMachineStatusScreen();
    return;
  }

  if (_machineStatusSelected >= MACHINE_ITEM_HOME_X &&
      _machineStatusSelected <= MACHINE_ITEM_HOME_Z) {
    requestHomeSensorStatus(true);
    showMachineStatusScreen();
    return;
  }

  if (_machineStatusSelected == MACHINE_ITEM_REFRESH) {
    refreshMachineStatus();
    return;
  }

  requestHomeSensorStatus(true);
  showMachineStatusScreen();
}

std::vector<String> AppController::networkStatusItems() {
  String mqttLink = "-";
  if (_mqttEnabled) {
    mqttLink = _cloud.statusText();
  }

  String shortTime = _time.timeShortString();
  if (shortTime.length() == 0) {
    shortTime = "--:--";
  }

  return {
    wifiNetworkStatus(_wifiEnabled, _wifi.isConnected()),
    String("SSID: ") + (_wifiEnabled ? _wifi.ssid() : "-"),
    String("IP: ") + (_wifiEnabled ? _wifi.ipAddress() : "-"),
    String("MQTT: ") + (_mqttEnabled ? "ON" : "OFF"),
    String("Link: ") + mqttLink,
    String("Broker: ") + mqttBrokerInfo(),
    String("AP: ") + _wifi.portalApName(),
    String("Waktu: ") + shortTime,
    String("Tanggal: ") + _time.dateString(),
    String("Sync: ") + (_time.isSynced() ? "OK" : "NO"),
    String("Reset WiFi")
  };
}

void AppController::loadNetworkSettings() {
  Preferences prefs;
  bool brokerMigrated = false;
  prefs.begin(NETWORK_PREFS_NAMESPACE, true);
  _wifiEnabled = prefs.getBool(NETWORK_PREF_WIFI, AppConfig::DEFAULT_WIFI_ENABLED);
  _mqttEnabled = prefs.getBool(NETWORK_PREF_MQTT, MqttConfig::DEFAULT_ENABLED);
  _mqttBroker = normalizedMqttText(
    prefs.getString(NETWORK_PREF_MQTT_BROKER, MqttConfig::BROKER),
    MqttConfig::BROKER,
    MqttConfig::BROKER_MAX_LEN,
    false
  );
  if (_mqttBroker == MqttConfig::LEGACY_BROKER) {
    _mqttBroker = MqttConfig::BROKER;
    brokerMigrated = true;
  }
  _mqttPort = normalizedMqttPort(prefs.getUInt(NETWORK_PREF_MQTT_PORT, MqttConfig::PORT));
  _mqttTopicPrefix = normalizedMqttText(
    prefs.getString(NETWORK_PREF_MQTT_TOPIC, MqttConfig::TOPIC_PREFIX),
    MqttConfig::TOPIC_PREFIX,
    MqttConfig::TOPIC_PREFIX_MAX_LEN,
    false
  );
  if (_mqttTopicPrefix == "skripsi/cnc-interface") {
    _mqttTopicPrefix = MqttConfig::TOPIC_PREFIX;
  }
  _mqttUser = normalizedMqttText(
    prefs.getString(NETWORK_PREF_MQTT_USER, MqttConfig::USER),
    MqttConfig::USER,
    MqttConfig::USER_MAX_LEN,
    true
  );
  _mqttPassword = normalizedMqttText(
    prefs.getString(NETWORK_PREF_MQTT_PASS, MqttConfig::PASSWORD),
    MqttConfig::PASSWORD,
    MqttConfig::PASSWORD_MAX_LEN,
    true
  );
  prefs.end();

  applyMqttSettings();
  if (brokerMigrated) {
    saveNetworkSettings();
    Serial.print("[NETWORK] MQTT broker lama dimigrasikan ke ");
    Serial.println(_mqttBroker);
  }
  logNetworkSettings("loaded");

  if (!_wifiEnabled) {
    _mqttEnabled = false;
  }
}

void AppController::saveNetworkSettings() {
  Preferences prefs;
  prefs.begin(NETWORK_PREFS_NAMESPACE, false);
  prefs.putBool(NETWORK_PREF_WIFI, _wifiEnabled);
  prefs.putBool(NETWORK_PREF_MQTT, _mqttEnabled);
  prefs.putString(NETWORK_PREF_MQTT_BROKER, _mqttBroker);
  prefs.putUInt(NETWORK_PREF_MQTT_PORT, _mqttPort);
  prefs.putString(NETWORK_PREF_MQTT_TOPIC, _mqttTopicPrefix);
  prefs.putString(NETWORK_PREF_MQTT_USER, _mqttUser);
  prefs.putString(NETWORK_PREF_MQTT_PASS, _mqttPassword);
  prefs.end();
}

void AppController::applyMqttSettings() {
  _cloud.configure(
    _mqttBroker,
    _mqttPort,
    _mqttTopicPrefix,
    _mqttUser,
    _mqttPassword
  );
}

void AppController::resetMqttSettingsToDefaults() {
  _mqttBroker = MqttConfig::BROKER;
  _mqttPort = MqttConfig::PORT;
  _mqttTopicPrefix = MqttConfig::TOPIC_PREFIX;
  _mqttUser = MqttConfig::USER;
  _mqttPassword = MqttConfig::PASSWORD;
  applyMqttSettings();
}

void AppController::logNetworkSettings(const char *source) const {
  Serial.print("[NETWORK] ");
  Serial.print(source);
  Serial.print(" WiFi=");
  Serial.print(_wifiEnabled ? "ON" : "OFF");
  Serial.print(" MQTT=");
  Serial.print(_mqttEnabled ? "ON" : "OFF");
  Serial.print(" Broker=");
  Serial.print(_mqttBroker);
  Serial.print(":");
  Serial.print(_mqttPort);
  Serial.print(" Topic=");
  Serial.println(_mqttTopicPrefix);
}

WifiPortalMqttSettings AppController::currentMqttPortalSettings() const {
  WifiPortalMqttSettings settings;
  settings.broker = _mqttBroker;
  settings.port = String(_mqttPort);
  settings.topicPrefix = _mqttTopicPrefix;
  settings.user = _mqttUser;
  settings.password = _mqttPassword;
  return settings;
}

void AppController::updateMqttSettingsFromPortal(const WifiPortalMqttSettings &settings) {
  _mqttBroker = normalizedMqttText(
    settings.broker,
    MqttConfig::BROKER,
    MqttConfig::BROKER_MAX_LEN,
    false
  );
  _mqttPort = parseMqttPort(settings.port);
  _mqttTopicPrefix = normalizedMqttText(
    settings.topicPrefix,
    MqttConfig::TOPIC_PREFIX,
    MqttConfig::TOPIC_PREFIX_MAX_LEN,
    false
  );
  _mqttUser = normalizedMqttText(
    settings.user,
    MqttConfig::USER,
    MqttConfig::USER_MAX_LEN,
    true
  );
  _mqttPassword = normalizedMqttText(
    settings.password,
    MqttConfig::PASSWORD,
    MqttConfig::PASSWORD_MAX_LEN,
    true
  );

  saveNetworkSettings();
  applyMqttSettings();
  logNetworkSettings("portal");
}

String AppController::mqttBrokerInfo() const {
  return _mqttBroker + ":" + String(_mqttPort);
}

void AppController::stopNetworkRuntime() {
  _cloud.disconnect();
  _wifi.disconnect();
  _time.update(false);
  _networkStarted = false;
  _mqttStarted = false;
}

bool AppController::connectStoredWifiWithFeedback() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  const unsigned long startedAt = millis();
  unsigned long lastBeepAt = 0;
  bool firstBeep = true;

  while (millis() - startedAt < AppConfig::WIFI_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      _buzzer.stop();
      return true;
    }

    const unsigned long now = millis();
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION &&
        (firstBeep || now - lastBeepAt >= AppConfig::WIFI_LOADING_BEEP_INTERVAL_MS)) {
      _buzzer.play(BuzzerHandler::Cue::Loading);
      lastBeepAt = now;
      firstBeep = false;
    }

    _buzzer.update();
    delay(5);
  }

  _buzzer.stop();
  return WiFi.status() == WL_CONNECTED;
}

void AppController::startNetworkFromSettings(bool tryStoredCredentials) {
  if (!_wifiEnabled) {
    stopNetworkRuntime();
    return;
  }

  _lcd.showWifiSetup(_wifi.portalApName());

  if (tryStoredCredentials && _wifi.hasStoredCredentials() && connectStoredWifiWithFeedback()) {
    _networkStarted = true;
    _mqttStarted = false;
    _time.requestSync();

    if (_mqttEnabled) {
      _cloud.begin();
      _mqttStarted = true;
    }
    return;
  }

  // WiFiManager menunggu user di portal dan bersifat blocking.
  // Pastikan tone tombol tidak tertahan selama portal aktif.
  _buzzer.stop();
  WifiPortalMqttSettings portalSettings = currentMqttPortalSettings();
  bool wifiReady = _wifi.begin(&portalSettings);
  _networkStarted = wifiReady;
  _mqttStarted = false;

  if (!wifiReady) {
    return;
  }

  updateMqttSettingsFromPortal(portalSettings);
  _time.requestSync();

  if (_mqttEnabled) {
    _cloud.begin();
    _mqttStarted = true;
  }
}

void AppController::setWifiEnabled(bool enabled) {
  _wifiEnabled = enabled;
  if (!_wifiEnabled) {
    _mqttEnabled = false;
  }
  saveNetworkSettings();

  if (_wifiEnabled) {
    startNetworkFromSettings();
  } else {
    stopNetworkRuntime();
  }
}

void AppController::setMqttEnabled(bool enabled) {
  _mqttEnabled = enabled && _wifiEnabled;
  saveNetworkSettings();

  if (!_mqttEnabled) {
    _cloud.disconnect();
    _mqttStarted = false;
    return;
  }

  if (_wifi.isConnected()) {
    applyMqttSettings();
    _cloud.begin();
    _mqttStarted = true;
  }
}

void AppController::openMqttBrokerEditor() {
  uint8_t digits[12] = {0};
  if (!readMqttBrokerIpDigits(_mqttBroker, digits) &&
      !readMqttBrokerIpDigits(MqttConfig::BROKER, digits)) {
    memset(digits, 0, sizeof(digits));
  }

  memcpy(_mqttBrokerEditorDigits, digits, sizeof(_mqttBrokerEditorDigits));
  _mqttBrokerEditorIndex = 0;
  _pendingMqttBroker = "";
  _uiState = UiState::BrokerEditor;
  requestMqttBrokerEditorRedraw(true);
}

void AppController::showMqttBrokerEditor() {
  if (!_displayEnabled) {
    return;
  }

  _lcd.showIpEditor(
    "MQTT BROKER IP",
    mqttBrokerEditorValue().c_str(),
    _mqttBrokerEditorIndex
  );
  _mqttBrokerEditorDirty = false;
  _lastMqttBrokerEditorDraw = millis();
}

void AppController::requestMqttBrokerEditorRedraw(bool immediate) {
  if (_uiState != UiState::BrokerEditor) {
    return;
  }

  _mqttBrokerEditorDirty = true;
  if (immediate) {
    showMqttBrokerEditor();
  }
}

void AppController::updateMqttBrokerEditorRedraw() {
  if (_uiState != UiState::BrokerEditor || !_mqttBrokerEditorDirty) {
    return;
  }

  if (millis() - _lastMqttBrokerEditorDraw < AppConfig::BROKER_EDITOR_REDRAW_MS) {
    return;
  }

  showMqttBrokerEditor();
}

void AppController::moveMqttBrokerEditorRight() {
  if (_mqttBrokerEditorIndex < 11) {
    _mqttBrokerEditorIndex++;
    requestMqttBrokerEditorRedraw(true);
    return;
  }

  if (!mqttBrokerEditorValueValid()) {
    showInfoScreen(
      "BROKER IP",
      "Oktet max 255",
      UiState::BrokerEditor,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  _pendingMqttBroker = stripMqttBrokerOctetPadding(mqttBrokerEditorValue());
  showActionConfirm(
    PendingAction::ChangeBroker,
    "MQTT BROKER",
    _pendingMqttBroker.c_str()
  );
}

void AppController::moveMqttBrokerEditorLeft() {
  if (_mqttBrokerEditorIndex > 0) {
    _mqttBrokerEditorIndex--;
    requestMqttBrokerEditorRedraw(true);
    return;
  }

  _pendingMqttBroker = "";
  _mqttBrokerEditorDirty = false;
  showNetworkStatusScreen();
}

void AppController::changeMqttBrokerEditorDigit(int8_t direction) {
  uint8_t &digit = _mqttBrokerEditorDigits[_mqttBrokerEditorIndex];
  if (direction > 0) {
    digit = (digit + 1) % 10;
  } else {
    digit = (digit == 0) ? 9 : digit - 1;
  }

  requestMqttBrokerEditorRedraw(false);
}

String AppController::mqttBrokerEditorValue() const {
  char broker[16];
  snprintf(
    broker,
    sizeof(broker),
    "%u%u%u.%u%u%u.%u%u%u.%u%u%u",
    static_cast<unsigned int>(_mqttBrokerEditorDigits[0]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[1]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[2]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[3]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[4]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[5]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[6]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[7]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[8]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[9]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[10]),
    static_cast<unsigned int>(_mqttBrokerEditorDigits[11])
  );
  return String(broker);
}

bool AppController::mqttBrokerEditorValueValid() const {
  for (uint8_t group = 0; group < 4; ++group) {
    uint16_t value =
      (_mqttBrokerEditorDigits[group * 3] * 100) +
      (_mqttBrokerEditorDigits[group * 3 + 1] * 10) +
      _mqttBrokerEditorDigits[group * 3 + 2];
    if (value > 255) {
      return false;
    }
  }

  return true;
}

void AppController::applyMqttBrokerEditorValue(UiState returnState) {
  if (_pendingMqttBroker.length() == 0) {
    _pendingMqttBroker = stripMqttBrokerOctetPadding(mqttBrokerEditorValue());
  }

  _mqttBroker = _pendingMqttBroker;
  _pendingMqttBroker = "";
  saveNetworkSettings();
  applyMqttSettings();
  logNetworkSettings("lcd broker");

  if (_mqttEnabled) {
    _cloud.disconnect();
    _mqttStarted = false;

    if (_wifi.isConnected()) {
      _cloud.begin();
      _mqttStarted = true;
    }
  }

  showInfoScreen(
    "MQTT BROKER",
    _mqttBroker.c_str(),
    returnState,
    AppConfig::CONFIRM_RESULT_MESSAGE_MS
  );
}

void AppController::showNetworkStatusScreen() {
  _uiState = UiState::NetworkStatus;

  std::vector<String> items = networkStatusItems();
  if (_networkStatusSelected >= items.size()) {
    _networkStatusSelected = items.empty() ? 0 : items.size() - 1;
  }

  const size_t pageSize = 5;
  if (_networkStatusSelected >= _networkStatusOffset + pageSize) {
    _networkStatusOffset = _networkStatusSelected - pageSize + 1;
  } else if (_networkStatusSelected < _networkStatusOffset) {
    _networkStatusOffset = _networkStatusSelected;
  }

  _lcd.showMenu(
    "NETWORK",
    items,
    _networkStatusSelected,
    _networkStatusOffset
  );
}

void AppController::handleNetworkStatusSelect() {
  if (_networkStatusSelected == NETWORK_ITEM_WIFI) {
    showActionConfirm(
      PendingAction::ToggleWifi,
      "WIFI",
      _wifiEnabled ? "Matikan WiFi?" : "Aktifkan WiFi?"
    );
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_SSID) {
    String ssid = _wifi.isConnected() ? _wifi.ssid() : "Wifi belum terhubung";
    showInfoScreen("SSID", ssid.c_str(), UiState::NetworkStatus, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_IP) {
    String ip = _wifi.isConnected() ? _wifi.ipAddress() : "Wifi belum terhubung";
    showInfoScreen("IP ADDRESS", ip.c_str(), UiState::NetworkStatus, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_MQTT) {
    if (!_wifiEnabled) {
      showInfoScreen(
        "MQTT",
        "WiFi masih OFF",
        UiState::NetworkStatus,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
      return;
    }

    showActionConfirm(
      PendingAction::ToggleMqtt,
      "MQTT",
      _mqttEnabled ? "Matikan MQTT?" : "Aktifkan MQTT?"
    );
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_MQTT_LINK) {
    String title = String("LINK ") + (_mqttEnabled ? _cloud.statusText() : "OFF");
    String detail = _mqttEnabled ? mqttBrokerInfo() : "MQTT OFF";
    showInfoScreen(
      title.c_str(),
      detail.c_str(),
      UiState::NetworkStatus,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_BROKER) {
    openMqttBrokerEditor();
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_AP) {
    showInfoScreen("SETUP AP", _wifi.portalApName(), UiState::NetworkStatus, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_TIME) {
    String shortTime = _time.timeShortString();
    if (shortTime.length() == 0) {
      shortTime = "--:--";
    }
    showInfoScreen(
      "WAKTU",
      shortTime.c_str(),
      UiState::NetworkStatus,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_DATE) {
    String date = _time.dateString();
    showInfoScreen(
      "KALENDER",
      date.c_str(),
      UiState::NetworkStatus,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_SYNC) {
    if (_wifi.isConnected()) {
      _time.requestSync();
      showInfoScreen(
        "TIME SYNC",
        "Sync dimulai",
        UiState::NetworkStatus,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
    } else {
      showInfoScreen(
        "TIME SYNC",
        _time.statusText(),
        UiState::NetworkStatus,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
    }
    return;
  }

  if (_networkStatusSelected == NETWORK_ITEM_RESET) {
    showActionConfirm(
      PendingAction::ResetWifi,
      "RESET WIFI",
      "Hapus WiFi lama?"
    );
  }
}

void AppController::publishMqttMonitoring() {
  if (!_mqttEnabled || !_cloud.isConnected()) {
    return;
  }

  if (millis() - _lastMqttMonitorPublish < MqttConfig::MONITOR_INTERVAL_MS) {
    return;
  }

  _lastMqttMonitorPublish = millis();

  MqttMonitoringSnapshot snapshot;
  snapshot.marlinStatus = marlinStatusText();
  snapshot.timeStatus = _time.statusText();
  snapshot.time = _time.timeString();
  snapshot.date = _time.dateString();
  snapshot.ssid = _wifi.ssid();
  snapshot.ipAddress = _wifi.ipAddress();
  snapshot.mqttLink = _cloud.isConnected() ? "CONNECTED" : "DISCONNECTED";
  snapshot.homeX = _homeXStatus;
  snapshot.homeY = _homeYStatus;
  snapshot.homeZ = _homeZStatus;
  snapshot.softEndstop = softEndstopStatusText();
  snapshot.spindleOn = _spindleOn;
  snapshot.feedrateXY = _machineFeedrateXY;
  snapshot.feedrateZ = _machineFeedrateZ;
  snapshot.feedrateValid = _machineFeedrateLoaded;
  snapshot.workAreaX = _machineWorkAreaX;
  snapshot.workAreaY = _machineWorkAreaY;
  snapshot.workAreaZ = _machineWorkAreaZ;
  snapshot.workAreaValid = _machineWorkAreaLoaded;
  snapshot.workAreaSource = _machineWorkAreaLoaded
    ? (_machineWorkAreaFromFallback ? "fallback" : "marlin")
    : "unknown";
  snapshot.workOriginValid =
    marlinConnectionState() == MarlinConnectionState::Connected &&
    _homeCycleIssued &&
    _workspaceSelected &&
    _originConfigured;
  snapshot.workOrigin = snapshot.workOriginValid
    ? workspaceCode(_activeWorkspaceIndex)
    : "NONE";
  snapshot.machineActivity = machineActivityText();
  snapshot.posX = _posX;
  snapshot.posY = _posY;
  snapshot.posZ = _posZ;
  bool marlinPositionValid =
    marlinConnectionState() == MarlinConnectionState::Connected &&
    _positionValid;
  snapshot.positionValid = marlinPositionValid || MqttConfig::ENABLE_POSITION_SIMULATION;
  snapshot.positionSource = marlinPositionValid
    ? "marlin"
    : (MqttConfig::ENABLE_POSITION_SIMULATION ? "simulation" : "unavailable");
  snapshot.jobFile = computeJobName();
  if (snapshot.jobFile.length() == 0) {
    snapshot.jobFile = "Belum dipilih";
  }
  snapshot.jobState = _job.stateText();
  snapshot.jobError = _job.errorMessage();
  snapshot.progress = computeProgress();
  snapshot.sdReady = _sdCard.isReady();

  _cloud.publishMonitoring(snapshot);
}

void AppController::publishSdJobFileList() {
  if (!_mqttEnabled || !_cloud.isConnected()) {
    return;
  }

  if (!_storageStarted || !_sdCard.isReady()) {
    _cloud.publishFileListError("sd_not_ready");
    Serial.println("[MQTT][FILE LIST] complete count=0");
    return;
  }

  std::vector<SdCardEntry> files = _sdCard.listJobFilesRecursive(8, 50);
  _cloud.publishFileListBegin(files.size());

  for (size_t i = 0; i < files.size(); i++) {
    Serial.print("[MQTT][FILE LIST] item=");
    Serial.print(i);
    Serial.print(" path=");
    Serial.println(files[i].path);
    _cloud.publishFileListItem(i, files[i]);
  }

  _cloud.publishFileListEnd(files.size());
  Serial.print("[MQTT][FILE LIST] complete count=");
  Serial.println(files.size());
}

void AppController::handleMqttFileSelectRequest() {
  if (!_mqttEnabled || !_cloud.isConnected()) {
    return;
  }

  MqttFileSelectRequest request;
  if (!_cloud.consumeFileSelectRequest(request)) {
    return;
  }

  if (!request.validPayload) {
    _cloud.publishFileSelectError(request.reason.c_str(), request.path);
    return;
  }

  SdJobFileValidation validation = _sdCard.validateJobFilePath(request.path);
  if (!validation.valid) {
    _cloud.publishFileSelectError(validation.reason.c_str(), validation.path);
    return;
  }

  _mqttSelectedJobFile = validation.path;
  _mqttSelectedJobFileSize = validation.size;
  _cloud.publishFileSelectOk(validation.label, validation.path, validation.size);
}

void AppController::handleMqttSdStatusRequest(bool force) {
  if (!_mqttEnabled || !_cloud.isConnected()) {
    return;
  }

  if (!force && !_sdStatusPublishPending && !_cloud.consumeSdStatusRequest()) {
    return;
  }

  _sdStatusPublishPending = false;
  size_t fileCount = 0;
  if (_sdCard.isReady()) {
    fileCount = _sdCard.listJobFilesRecursive(8, 50).size();
    _cloud.publishSdStatus("ready", true, fileCount);
    return;
  }

  const char *status = _sdCard.cardStatusText();
  const String &reason = _sdCard.lastErrorReason();
  _cloud.publishSdStatus(
    status,
    false,
    0,
    reason.length() ? reason.c_str() : "sd_not_ready"
  );
}

bool AppController::isPendingLocalSelectionPath(const String &path) const {
  if (_uiState != UiState::ConfirmSelect ||
      _fileListSelected >= _fileListPaths.size()) {
    return false;
  }

  return normalizeSdFilePath(_fileListPaths[_fileListSelected]) == normalizeSdFilePath(path);
}

bool AppController::isSdPathInUse(const String &path) const {
  String normalized = normalizeSdFilePath(path);
  if (_job.isActive()) {
    return true;
  }

  if (_selectedJobSource == JobSource::SdCard &&
      _sdCard.hasSelectedJobFile() &&
      normalizeSdFilePath(_sdCard.selectedJobFile()) == normalized) {
    return true;
  }

  return isPendingLocalSelectionPath(normalized);
}

void AppController::handleMqttFileDeleteRequest() {
  if (!_mqttEnabled || !_cloud.isConnected()) {
    return;
  }

  MqttFileDeleteRequest request;
  if (!_cloud.consumeFileDeleteRequest(request)) {
    return;
  }

  if (!request.validPayload) {
    _cloud.publishFileDeleteError(request.reason.c_str(), request.filePath);
    return;
  }

  if (_mqttUploadActive) {
    _cloud.publishFileDeleteError("transfer_active", request.filePath);
    return;
  }

  SdDeleteValidation validation = _sdCard.validateDeletePath(request.filePath);
  if (!validation.valid) {
    const char *reason = validation.reason == "sd_not_ready" && _sdCard.lastErrorReason() == "sd_removed"
      ? "sd_removed"
      : validation.reason.c_str();
    _cloud.publishFileDeleteError(reason, validation.path.length() ? validation.path : request.filePath);
    return;
  }

  if (isSdPathInUse(validation.path)) {
    Serial.println("[SD][DELETE] rejected reason=file_in_use");
    _cloud.publishFileDeleteError("file_in_use", validation.path);
    return;
  }

  String reason;
  if (!_sdCard.deleteJobFile(validation.path, reason)) {
    if (reason == "sd_not_ready" && _sdCard.lastErrorReason() == "sd_removed") {
      reason = "sd_removed";
    }
    _cloud.publishFileDeleteError(reason.c_str(), validation.path);
    return;
  }

  _cloud.publishFileDeleteDeleted(validation.path);
  _sdStatusPublishPending = true;
}

void AppController::handleSdCardStateChanged() {
  bool sdReady = _sdCard.isReady();
  if (sdReady == _lastSdReady) {
    return;
  }

  _lastSdReady = sdReady;
  _sdStatusPublishPending = true;

  if (sdReady) {
    if (_mqttUploadTemporaryPath.length() > 0) {
      _sdCard.cleanupTemporaryUploadFiles(_mqttUploadFinalPath, _mqttUploadTemporaryPath);
    }
    return;
  }

  _fileList.clear();
  _fileListPaths.clear();
  _fileListIsDirectory.clear();
  _sdCard.clearSelectedJobFile();
  _selectedJobSource = JobSource::None;
  _homeCycleIssued = false;
  invalidateWorkOrigin();

  if (_mqttUploadActive) {
    String fileName = _mqttUploadFileName;
    abortMqttUpload(_sdCard.lastErrorReason() == "sd_removed" ? "sd_removed" : "sd_not_ready");
    if (_mqttEnabled && _cloud.isConnected()) {
      _cloud.publishFileFinishError(_sdCard.lastErrorReason() == "sd_removed" ? "sd_removed" : "sd_not_ready", fileName);
    }
  }

  if (_uiState == UiState::FileList || _uiState == UiState::ConfirmSelect) {
    showInfoScreen("SD CARD", "Kartu dilepas", UiState::Menu, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
  }
}

void AppController::clearMqttUploadState(bool keepFinishedAck) {
  _mqttUploadFileName = "";
  _mqttUploadFinalPath = "";
  _mqttUploadTemporaryPath = "";
  _mqttUploadBackupPath = "";
  _mqttUploadExpectedSize = 0;
  _mqttUploadChunkSize = 0;
  _mqttUploadTotalChunks = 0;
  _mqttUploadExpectedChunkIndex = 0;
  _mqttUploadBytesWritten = 0;
  _mqttUploadLastAckedChunk = -1;
  _mqttUploadActive = false;
  _mqttUploadOverwrite = false;

  if (!keepFinishedAck) {
    _mqttUploadFinished = false;
    _mqttUploadLastSavedFileName = "";
    _mqttUploadLastSavedPath = "";
    _mqttUploadLastSavedSize = 0;
    _mqttUploadLastSavedChunks = 0;
  }
}

void AppController::abortMqttUpload(const char *reason) {
  Serial.print("[SD][UPLOAD] rejected reason=");
  Serial.println(reason);
  _sdCard.abortUploadFile(_mqttUploadTemporaryPath);
  clearMqttUploadState(false);
}

void AppController::handleMqttFileStartRequest() {
  MqttFileStartRequest request;
  if (!_cloud.consumeFileStartRequest(request)) {
    return;
  }

  _mqttUploadFinished = false;

  if (!request.validPayload) {
    _cloud.publishFileStartError(request.reason.c_str(), request.fileName);
    return;
  }

  if (_mqttUploadActive) {
    _cloud.publishFileStartError("transfer_busy", request.fileName);
    return;
  }

  if (!_storageStarted || !_sdCard.isReady()) {
    _cloud.publishFileStartError("sd_not_ready", request.fileName);
    return;
  }

  size_t expectedChunks = (request.fileSize + request.chunkSize - 1) / request.chunkSize;
  if (expectedChunks != request.totalChunks) {
    _cloud.publishFileStartError("invalid_payload", request.fileName);
    return;
  }

  SdUploadPathValidation paths = _sdCard.validateUploadFileName(request.fileName);
  if (!paths.valid) {
    _cloud.publishFileStartError(paths.reason.c_str(), request.fileName);
    return;
  }

  if (!request.overwrite && _sdCard.fileExists(paths.finalPath.c_str())) {
    _cloud.publishFileStartError("file_exists", request.fileName, paths.finalPath);
    return;
  }

  if (!_sdCard.beginUploadFile(paths.temporaryPath, paths.finalPath)) {
    _cloud.publishFileStartError("open_failed", request.fileName);
    return;
  }

  clearMqttUploadState(false);
  _mqttUploadActive = true;
  _mqttUploadFileName = paths.fileName;
  _mqttUploadFinalPath = paths.finalPath;
  _mqttUploadTemporaryPath = paths.temporaryPath;
  _mqttUploadBackupPath = paths.finalPath + ".bak";
  _mqttUploadExpectedSize = request.fileSize;
  _mqttUploadChunkSize = request.chunkSize;
  _mqttUploadTotalChunks = request.totalChunks;
  _mqttUploadExpectedChunkIndex = 0;
  _mqttUploadBytesWritten = 0;
  _mqttUploadLastAckedChunk = -1;
  _mqttUploadOverwrite = request.overwrite;

  _cloud.publishFileStartReady(_mqttUploadFileName, _mqttUploadExpectedSize, _mqttUploadTotalChunks);
}

void AppController::handleMqttFileChunkRequest() {
  MqttFileChunkRequest request;
  if (!_cloud.consumeFileChunkRequest(request)) {
    return;
  }

  if (!request.validPayload) {
    const char *reason = request.reason.length() ? request.reason.c_str() : "invalid_payload";
    String fileName = _mqttUploadFileName;
    if (_mqttUploadActive) {
      abortMqttUpload(reason);
    }
    _cloud.publishFileChunkError(reason, request.chunkIndex, fileName);
    return;
  }

  if (!_mqttUploadActive) {
    _cloud.publishFileChunkError("no_active_transfer", request.chunkIndex, _mqttUploadLastSavedFileName);
    return;
  }

  if (request.chunkIndex == _mqttUploadLastAckedChunk) {
    _cloud.publishFileChunkReceived(request.chunkIndex);
    return;
  }

  if (request.chunkIndex != (int)_mqttUploadExpectedChunkIndex) {
    _cloud.publishFileChunkError("unexpected_chunk", request.chunkIndex, _mqttUploadFileName);
    return;
  }

  bool lastChunk = _mqttUploadExpectedChunkIndex + 1 == _mqttUploadTotalChunks;
  size_t expectedLength = lastChunk
    ? (_mqttUploadExpectedSize - (_mqttUploadChunkSize * _mqttUploadExpectedChunkIndex))
    : _mqttUploadChunkSize;

  if (expectedLength == 0 ||
      request.data.size() != expectedLength ||
      _mqttUploadBytesWritten + request.data.size() > _mqttUploadExpectedSize) {
    String fileName = _mqttUploadFileName;
    abortMqttUpload("invalid_chunk");
    _cloud.publishFileChunkError("invalid_chunk", request.chunkIndex, fileName);
    return;
  }

  size_t written = 0;
  if (!_sdCard.writeUploadChunk(request.data.data(), request.data.size(), written)) {
    String fileName = _mqttUploadFileName;
    abortMqttUpload("write_failed");
    _cloud.publishFileChunkError("write_failed", request.chunkIndex, fileName);
    return;
  }

  _mqttUploadBytesWritten += written;
  _mqttUploadLastAckedChunk = request.chunkIndex;
  _mqttUploadExpectedChunkIndex++;

  Serial.print("[SD][UPLOAD] chunk=");
  Serial.print(request.chunkIndex);
  Serial.print(" bytes=");
  Serial.print(written);
  Serial.print(" total=");
  Serial.println(_mqttUploadBytesWritten);

  _cloud.publishFileChunkReceived(request.chunkIndex);
}

void AppController::handleMqttFileFinishRequest() {
  if (!_cloud.consumeFileFinishRequest()) {
    return;
  }

  if (!_mqttUploadActive) {
    if (_mqttUploadFinished) {
      _cloud.publishFileFinishSaved(
        _mqttUploadLastSavedFileName,
        _mqttUploadLastSavedPath,
        _mqttUploadLastSavedSize,
        _mqttUploadLastSavedChunks
      );
      return;
    }

    _cloud.publishFileFinishError("no_active_transfer", _mqttUploadFileName);
    return;
  }

  Serial.print("[SD][UPLOAD] finish expected=");
  Serial.print(_mqttUploadExpectedSize);
  Serial.print(" written=");
  Serial.println(_mqttUploadBytesWritten);

  if (_mqttUploadExpectedChunkIndex != _mqttUploadTotalChunks) {
    String fileName = _mqttUploadFileName;
    abortMqttUpload("incomplete_transfer");
    _cloud.publishFileFinishError("incomplete_transfer", fileName);
    return;
  }

  if (_mqttUploadBytesWritten != _mqttUploadExpectedSize) {
    String fileName = _mqttUploadFileName;
    abortMqttUpload("size_mismatch");
    _cloud.publishFileFinishError("size_mismatch", fileName);
    return;
  }

  size_t savedSize = 0;
  String reason;
  String fileName = _mqttUploadFileName;
  String finalPath = _mqttUploadFinalPath;
  String temporaryPath = _mqttUploadTemporaryPath;
  size_t totalChunks = _mqttUploadTotalChunks;

  if (!_sdCard.finishUploadFile(temporaryPath, finalPath, _mqttUploadExpectedSize, savedSize, reason)) {
    if (reason.length() == 0) {
      reason = "rename_failed";
    }
    abortMqttUpload(reason.c_str());
    _cloud.publishFileFinishError(reason.c_str(), fileName);
    return;
  }

  _mqttUploadLastSavedFileName = fileName;
  _mqttUploadLastSavedPath = finalPath;
  _mqttUploadLastSavedSize = savedSize;
  _mqttUploadLastSavedChunks = totalChunks;
  _mqttUploadFinished = true;
  clearMqttUploadState(true);

  _cloud.publishFileFinishSaved(fileName, finalPath, savedSize, totalChunks);
}

void AppController::handleMqttFileTransferRequests() {
  handleMqttFileStartRequest();
  handleMqttFileChunkRequest();
  handleMqttFileFinishRequest();
}

bool AppController::toggleSpindle() {
  if (_job.isActive()) {
    showJobControlMessage("Job sedang aktif");
    return false;
  }

  if (!canSendMarlinCommand()) {
    showCncUnavailable(UiState::MachineStatus);
    return false;
  }

  const bool nextState = !_spindleOn;

  if (AppConfig::ENABLE_SPINDLE_CONTROL) {
    Serial1.println(nextState ? "M3" : "M5");
    _spindleOn = nextState;
    return true;
  }

  showInfoScreen(
    "SPINDLE",
    "Kontrol nonaktif",
    UiState::MachineStatus,
    AppConfig::CONFIRM_RESULT_MESSAGE_MS
  );
  return false;
}

bool AppController::applyMachineFeedrate() {
  if (_job.isActive()) {
    showJobControlMessage("Job sedang aktif");
    return false;
  }

  if (!canSendMarlinCommand()) {
    showCncUnavailable(UiState::MachineStatus);
    return false;
  }

  if (!AppConfig::ENABLE_MACHINE_FEEDRATE_CONTROL) {
    showInfoScreen(
      "FEEDRATE",
      "Kontrol nonaktif",
      UiState::MachineStatus,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return false;
  }

  Serial1.print("M203 X");
  Serial1.print(_machineFeedrateXY);
  Serial1.print(" Y");
  Serial1.print(_machineFeedrateXY);
  Serial1.print(" Z");
  Serial1.println(_machineFeedrateZ);
  requestMachineFeedrateStatus(true);
  return true;
}

void AppController::requestMachineFeedrateStatus(bool force) {
  if (!canSendMarlinCommand() || _job.ownsMarlinStream()) {
    return;
  }

  if (!force && millis() - _lastM203Request < AppConfig::MACHINE_STATUS_POLL_MS) {
    return;
  }

  logMarlinTx("M203");
  Serial1.println("M203");
  _lastM203Request = millis();
  _lastMarlinStatusPoll = _lastM203Request;
}

void AppController::parseMachineFeedrateStatus(const String &line) {
  if (line.indexOf("M203") == -1) {
    return;
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (!parseAxisValue(line, 'X', x) ||
      !parseAxisValue(line, 'Y', y) ||
      !parseAxisValue(line, 'Z', z)) {
    return;
  }

  _machineFeedrateXY = (unsigned int)((x + y) / 2.0f + 0.5f);
  _machineFeedrateZ = (unsigned int)(z + 0.5f);
  _machineFeedrateLoaded = true;

  if (_uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  }
}

void AppController::requestMachineGeometryStatus(bool force) {
  if (!canSendMarlinCommand() || _job.ownsMarlinStream()) {
    return;
  }

  if (!force && millis() - _lastM115Request < AppConfig::MACHINE_STATUS_POLL_MS) {
    return;
  }

  logMarlinTx("M115");
  Serial1.println("M115");
  _lastM115Request = millis();
  _lastMarlinStatusPoll = _lastM115Request;
}

void AppController::parseMachineGeometryStatus(const String &line) {
  if (line.indexOf("area:{") == -1 || line.indexOf("full:{") == -1) {
    return;
  }

  float minX = 0.0f;
  float minY = 0.0f;
  float minZ = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  float maxZ = 0.0f;

  if (!parseGeometryAxisValue(line, "full:{", "min:{", 'x', minX) ||
      !parseGeometryAxisValue(line, "full:{", "min:{", 'y', minY) ||
      !parseGeometryAxisValue(line, "full:{", "min:{", 'z', minZ) ||
      !parseGeometryAxisValue(line, "full:{", "max:{", 'x', maxX) ||
      !parseGeometryAxisValue(line, "full:{", "max:{", 'y', maxY) ||
      !parseGeometryAxisValue(line, "full:{", "max:{", 'z', maxZ)) {
    return;
  }

  _machineWorkAreaX = (unsigned int)(fabs(maxX - minX) + 0.5f);
  _machineWorkAreaY = (unsigned int)(fabs(maxY - minY) + 0.5f);
  _machineWorkAreaZ = (unsigned int)(fabs(maxZ - minZ) + 0.5f);
  _machineWorkAreaLoaded = true;
  _machineWorkAreaFromFallback = false;

  Serial.print("[MARLIN][M115] area X=");
  Serial.print(_machineWorkAreaX);
  Serial.print(" Y=");
  Serial.print(_machineWorkAreaY);
  Serial.print(" Z=");
  Serial.println(_machineWorkAreaZ);

  if (_uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  }
}

void AppController::requestHomeSensorStatus(bool force) {
  if (!canSendMarlinCommand() || _job.ownsMarlinStream()) {
    return;
  }

  if (!force && millis() - _lastM119Request < AppConfig::MACHINE_STATUS_POLL_MS) {
    return;
  }

  logMarlinTx("M119");
  Serial1.println("M119");
  _lastM119Request = millis();
  _lastMarlinStatusPoll = _lastM119Request;
}

void AppController::parseHomeSensorStatus(const String &line) {
  String lower = line;
  lower.toLowerCase();

  bool hasState = lower.indexOf("triggered") != -1 || lower.indexOf("open") != -1;
  if (!hasState) {
    return;
  }

  String status = (lower.indexOf("triggered") != -1) ? "TRIGGER" : "OPEN";
  bool changed = false;

  if (lower.startsWith("x_min:") || lower.startsWith("x_max:")) {
    changed = _homeXStatus != status;
    _homeXStatus = status;
  } else if (lower.startsWith("y_min:") || lower.startsWith("y_max:")) {
    changed = _homeYStatus != status;
    _homeYStatus = status;
  } else if (lower.startsWith("z_min:") || lower.startsWith("z_max:")) {
    changed = _homeZStatus != status;
    _homeZStatus = status;
  }

  if (changed && _uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  }
}

void AppController::requestSoftEndstopStatus(bool force) {
  if (!canSendMarlinCommand() || _job.ownsMarlinStream()) {
    return;
  }

  if (!force && millis() - _lastM211Request < AppConfig::MACHINE_STATUS_POLL_MS) {
    return;
  }

  logMarlinTx("M211");
  Serial1.println("M211");
  _lastM211Request = millis();
  _lastMarlinStatusPoll = _lastM211Request;
}

void AppController::parseSoftEndstopStatus(const String &line) {
  String lower = line;
  lower.trim();
  lower.toLowerCase();

  bool looksLikeSoftEndstop =
    lower.indexOf("m211") != -1 ||
    lower.indexOf("soft endstop") != -1;

  if (!looksLikeSoftEndstop) {
    return;
  }

  bool parsed = false;
  bool enabled = _softEndstopEnabled;

  if (lower.indexOf("s1") != -1 ||
      lower.indexOf(": on") != -1 ||
      lower.endsWith(" on") ||
      lower.endsWith(":on")) {
    enabled = true;
    parsed = true;
  } else if (lower.indexOf("s0") != -1 ||
             lower.indexOf(": off") != -1 ||
             lower.endsWith(" off") ||
             lower.endsWith(":off")) {
    enabled = false;
    parsed = true;
  }

  if (!parsed) {
    return;
  }

  bool changed = !_softEndstopLoaded || _softEndstopEnabled != enabled;
  _softEndstopEnabled = enabled;
  _softEndstopLoaded = true;

  if (changed && _uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  }
}

void AppController::prefetchMachineStatus() {
  if (!canSendMarlinCommand() || _job.ownsMarlinStream()) {
    return;
  }

  unsigned long now = millis();
  if (now - _lastMarlinStatusPoll < AppConfig::MACHINE_STATUS_QUERY_GAP_MS) {
    return;
  }

  bool homeStatusMissing =
    _homeXStatus == "?" ||
    _homeYStatus == "?" ||
    _homeZStatus == "?";

  for (uint8_t attempt = 0; attempt < 4; attempt++) {
    uint8_t step = _machineStatusPrefetchCursor;
    _machineStatusPrefetchCursor = (_machineStatusPrefetchCursor + 1) % 4;

    if (step == 0 && !_machineFeedrateLoaded) {
      requestMachineFeedrateStatus(true);
      return;
    }

    if (step == 1 && (!_machineWorkAreaLoaded || _machineWorkAreaFromFallback)) {
      requestMachineGeometryStatus(true);
      return;
    }

    if (step == 2 &&
        (homeStatusMissing || now - _lastM119Request >= AppConfig::MACHINE_STATUS_POLL_MS)) {
      requestHomeSensorStatus(true);
      return;
    }

    if (step == 3 && !_softEndstopLoaded) {
      requestSoftEndstopStatus(true);
      return;
    }
  }
}

bool AppController::loadSdFileList(const String &directory) {
  _currentSdDirectory = normalizeSdDirectory(directory);
  _fileList.clear();
  _fileListPaths.clear();
  _fileListIsDirectory.clear();
  _fileListSelected = 0;
  _fileListOffset = 0;

  if (!_sdCard.isReady()) {
    showInfoScreen("SD CARD", "Belum siap");
    return false;
  }

  std::vector<SdCardEntry> entries = _sdCard.listEntries(_currentSdDirectory.c_str(), 100, true);
  std::vector<SdCardEntry> parentEntries;
  std::vector<SdCardEntry> folderEntries;
  std::vector<SdCardEntry> fileEntries;

  for (const SdCardEntry &entry : entries) {
    if (isIgnoredSdBrowserEntry(entry.name)) {
      continue;
    }

    if (entry.isDirectory) {
      if (entry.name == "..") {
        parentEntries.push_back(entry);
      } else {
        folderEntries.push_back(entry);
      }
      continue;
    }

    if (!isSupportedJobFile(entry.path)) {
      continue;
    }

    fileEntries.push_back(entry);
  }

  std::sort(folderEntries.begin(), folderEntries.end(), entryNameLess);
  std::sort(fileEntries.begin(), fileEntries.end(), entryNameLess);

  auto appendEntry = [this](const SdCardEntry &entry) {
    String label = entry.isDirectory
      ? (entry.name == ".." ? ".." : "[" + entry.name + "]")
      : entry.name;

    _fileList.push_back(label);
    _fileListPaths.push_back(entry.path);
    _fileListIsDirectory.push_back(entry.isDirectory);
  };

  for (const SdCardEntry &entry : parentEntries) appendEntry(entry);
  for (const SdCardEntry &entry : folderEntries) appendEntry(entry);
  for (const SdCardEntry &entry : fileEntries) appendEntry(entry);

  return true;
}

void AppController::openSdFileList(const String &directory) {
  if (!loadSdFileList(directory)) {
    return;
  }

  showCurrentFileListPage();
}

void AppController::showCurrentFileListPage() {
  _uiState = UiState::FileList;

  if (!_displayEnabled) {
    return;
  }

  if (_fileList.empty()) {
    _lcd.showMessage(fileListTitle(_sdCard.volumeLabel(), _currentSdDirectory).c_str(), "Tidak ada file");
    return;
  }

  const size_t pageSize = 5;
  if (_fileListSelected >= _fileList.size()) {
    _fileListSelected = _fileList.size() - 1;
  }

  _fileListOffset = (_fileListSelected / pageSize) * pageSize;
  size_t end = min(_fileListOffset + pageSize, _fileList.size());
  std::vector<String> page;
  for (size_t i = _fileListOffset; i < end; ++i) {
    page.push_back(_fileList[i]);
  }

  String title = fileListTitle(_sdCard.volumeLabel(), _currentSdDirectory);
  _lcd.showMenu(title.c_str(), page, _fileListSelected - _fileListOffset, 0);
}

void AppController::enterSelectedFileListItem() {
  if (!_displayEnabled) {
    return;
  }

  if (_fileList.empty() || _fileListSelected >= _fileListPaths.size()) {
    _lcd.showMessage("Files", "Tidak ada file");
    return;
  }

  if (_fileListSelected < _fileListIsDirectory.size() &&
      _fileListIsDirectory[_fileListSelected]) {
    openSdFileList(_fileListPaths[_fileListSelected]);
    return;
  }

  _confirmSelectedIndex = 1;
  _confirmTitle = "PILIH JOB";
  _confirmMessage = basenameForLcd(_fileListPaths[_fileListSelected]);
  _uiState = UiState::ConfirmSelect;
  _lcd.showConfirm(_confirmTitle.c_str(), _confirmMessage.c_str(), _confirmSelectedIndex);
}

void AppController::showInfoScreen(
  const char *title,
  const char *message,
  UiState returnState,
  unsigned long autoCloseMs
) {
  _infoReturnState = returnState;
  _infoShownAt = millis();
  _infoAutoCloseMs = autoCloseMs;
  _uiState = UiState::Info;
  if (!_displayEnabled) {
    Serial.print("[LCD][HEADLESS] ");
    Serial.print(title);
    Serial.print(": ");
    Serial.println(message);
    return;
  }
  _lcd.showMessage(title, message);
}

void AppController::dismissInfoScreen() {
  _infoAutoCloseMs = 0;
  _uiState = _infoReturnState;

  if (!_displayEnabled) {
    if (_uiState == UiState::Info) {
      _uiState = UiState::Standby;
    }
    return;
  }

  if (_uiState == UiState::Standby) {
    showStandbyScreen();
  } else if (_uiState == UiState::SetOrigin) {
    updateJogDisplay();
  } else if (_uiState == UiState::FileList) {
    showCurrentFileListPage();
  } else if (_uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  } else if (_uiState == UiState::NetworkStatus) {
    showNetworkStatusScreen();
  } else if (_uiState == UiState::BrokerEditor) {
    showMqttBrokerEditor();
  } else {
    _uiState = UiState::Menu;
    _menu.redraw();
  }
}

void AppController::showActionConfirm(
  PendingAction action,
  const char *title,
  const char *message
) {
  _activeJogPin = 0;
  _pendingAction = action;
  _confirmReturnState = _uiState;
  _confirmTitle = title;
  _confirmMessage = message;
  _confirmSelectedIndex = 1;
  _uiState = UiState::ConfirmAction;
  if (!_displayEnabled) {
    Serial.print("[LCD][HEADLESS] confirm ");
    Serial.print(title);
    Serial.print(": ");
    Serial.println(message);
    return;
  }
  redrawActionConfirm();
}

void AppController::redrawActionConfirm() {
  if (!_displayEnabled) {
    return;
  }

  _lcd.showConfirm(
    _confirmTitle.c_str(),
    _confirmMessage.c_str(),
    _confirmSelectedIndex
  );
}

void AppController::cancelActionConfirm() {
  PendingAction cancelledAction = _pendingAction;
  _pendingAction = PendingAction::None;
  _uiState = _confirmReturnState;

  if (cancelledAction == PendingAction::SetOrigin) {
    updateJogDisplay();
  } else if (_uiState == UiState::Standby) {
    showStandbyScreen();
  } else if (_uiState == UiState::FileList) {
    showCurrentFileListPage();
  } else if (_uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  } else if (_uiState == UiState::NetworkStatus) {
    showNetworkStatusScreen();
  } else if (_uiState == UiState::BrokerEditor) {
    showMqttBrokerEditor();
  } else {
    _menu.redraw();
  }
}

void AppController::executeConfirmedAction() {
  PendingAction action = _pendingAction;
  UiState returnState = _confirmReturnState;
  _pendingAction = PendingAction::None;

  if (action == PendingAction::HomeAll) {
    if (!canSendMarlinCommand()) {
      showCncUnavailable(returnState);
      return;
    }

    if (AppConfig::ENABLE_HOME_CONTROL) {
      Serial1.println("G28");
      _homeCycleIssued = true;
      invalidateWorkOrigin();
      showInfoScreen(
        "HOME ALL",
        "G28 dikirim",
        returnState,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
    } else {
      showInfoScreen(
        "UI PREVIEW",
        "G28 belum dikirim",
        returnState,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
    }
    return;
  }

  if (action == PendingAction::SelectWorkspace) {
    selectWorkspace(_pendingWorkspaceIndex, returnState);
    return;
  }

  if (action == PendingAction::SetOrigin) {
    confirmSetOrigin(UiState::Menu);
    return;
  }

  if (action == PendingAction::RepeatJob) {
    repeatSelectedJob(UiState::Standby);
    return;
  }

  if (action == PendingAction::StartJob) {
    startSelectedJob(false, UiState::Standby);
    return;
  }

  if (action == PendingAction::SpindleToggle) {
    bool changed = toggleSpindle();
    if (changed) {
      showInfoScreen(
        "SPINDLE",
        _spindleOn ? "Spindle ON" : "Spindle OFF",
        returnState,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
    }
    return;
  }

  if (action == PendingAction::ToggleWifi) {
    bool nextState = !_wifiEnabled;
    setWifiEnabled(nextState);
    const bool wifiConnected = _wifi.isConnected();
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
      if (!nextState) {
        _buzzer.play(BuzzerHandler::Cue::Back);
      } else {
        _buzzer.play(wifiConnected ? BuzzerHandler::Cue::Success : BuzzerHandler::Cue::Warning);
      }
    }
    const char *wifiTitle = "WIFI";
    const char *wifiMessage = "WiFi OFF";
    if (nextState) {
      wifiTitle = wifiConnected ? "WIFI" : "WiFi gagal";
      wifiMessage = wifiConnected ? "WiFi terkoneksi !!!" : "terkoneksi !!!";
    }
    showInfoScreen(
      wifiTitle,
      wifiMessage,
      UiState::NetworkStatus,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (action == PendingAction::ToggleMqtt) {
    bool nextState = !_mqttEnabled;
    setMqttEnabled(nextState);
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
      if (!nextState) {
        _buzzer.play(BuzzerHandler::Cue::Back);
      } else {
        _buzzer.play(_cloud.isConnected() ? BuzzerHandler::Cue::Success : BuzzerHandler::Cue::Warning);
      }
    }
    String mqttMessage = "MQTT OFF";
    if (_mqttEnabled) {
      mqttMessage = _cloud.isConnected() ? "MQTT CONNECTED" : String("LINK ") + _cloud.statusText();
    }
    showInfoScreen(
      "MQTT",
      mqttMessage.c_str(),
      UiState::NetworkStatus,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (action == PendingAction::ResetWifi) {
    const bool keepMqttEnabled = _mqttEnabled;
    _cloud.disconnect();
    _wifi.resetSettings();
    _time.update(false);
    _wifiEnabled = true;
    _mqttEnabled = keepMqttEnabled;
    resetMqttSettingsToDefaults();
    saveNetworkSettings();
    logNetworkSettings("reset");
    _networkStarted = false;
    _mqttStarted = false;

    // Setelah credential dihapus, langsung buka kembali portal konfigurasi.
    startNetworkFromSettings(false);
    const bool wifiReady = _wifi.isConnected();
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
      _buzzer.play(wifiReady ? BuzzerHandler::Cue::Success : BuzzerHandler::Cue::Warning);
    }
    showInfoScreen(
      wifiReady ? "RESET WIFI" : "WIFI SETUP",
      wifiReady ? "WiFi tersimpan" : "Portal timeout",
      returnState,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (action == PendingAction::ChangeBroker) {
    applyMqttBrokerEditorValue(UiState::NetworkStatus);
    return;
  }

  cancelActionConfirm();
}

void AppController::repeatSelectedJob(UiState returnState) {
  startSelectedJob(AppConfig::ENABLE_JOB_REPEAT_RETURN, returnState);
}

bool AppController::startSelectedJob(bool returnToOrigin, UiState returnState) {
  if (!AppConfig::ENABLE_JOB_CONTROL) {
    showInfoScreen("START JOB", "Job control OFF", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (_selectedJobSource != JobSource::SdCard || !_sdCard.hasSelectedJobFile()) {
    showInfoScreen("START JOB", "Pilih file dulu", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (_job.state() == GcodeJobController::State::Analyzing) {
    showInfoScreen("START JOB", "Analisis belum selesai", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (_job.state() == GcodeJobController::State::Error) {
    showInfoScreen("START JOB", _job.errorMessage().c_str(), returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (!canSendMarlinCommand()) {
    showCncUnavailable(returnState);
    return false;
  }

  if (!_homeCycleIssued) {
    showInfoScreen("START JOB", "Home All dulu", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (!_originConfigured) {
    showInfoScreen("START JOB", "Pilih Work Origin", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (!_softEndstopLoaded || !_softEndstopEnabled) {
    showInfoScreen("START JOB", "SoftEnd harus ON", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (!_sdCard.fileExists(_sdCard.selectedJobFile().c_str())) {
    showInfoScreen("START JOB", "File SD hilang", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  if (!_job.start(returnToOrigin)) {
    showInfoScreen("START JOB", "Start ditolak", returnState, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return false;
  }

  showInfoScreen(
    returnToOrigin ? "REPEAT JOB" : "START JOB",
    returnToOrigin ? "Kembali lalu mulai" : "Job mulai",
    UiState::Standby,
    AppConfig::CONFIRM_RESULT_MESSAGE_MS
  );
  return true;
}

bool AppController::isConfirmationState() const {
  return _uiState == UiState::ConfirmAction ||
         _uiState == UiState::ConfirmSelect;
}

void AppController::acceptConfirmation() {
  if (_uiState == UiState::ConfirmAction) {
    executeConfirmedAction();
    return;
  }

  if (_uiState == UiState::ConfirmSelect) {
    selectFileFromList();
  }
}

void AppController::rejectConfirmation() {
  if (_uiState == UiState::ConfirmAction) {
    cancelActionConfirm();
    return;
  }

  if (_uiState == UiState::ConfirmSelect) {
    showCurrentFileListPage();
  }
}

void AppController::executeSelectedConfirmation() {
  if (_confirmSelectedIndex == 0) {
    acceptConfirmation();
  } else {
    rejectConfirmation();
  }
}

void AppController::redrawCurrentConfirmation() {
  if (_uiState == UiState::ConfirmAction) {
    redrawActionConfirm();
  } else if (_uiState == UiState::ConfirmSelect) {
    _lcd.showConfirm(_confirmTitle.c_str(), _confirmMessage.c_str(), _confirmSelectedIndex);
  }
}


// --------------------
// Siklus utama aplikasi
// --------------------

void AppController::begin() {
  activeApp = this;
  beginSerial();
  _time.begin();
  beginDisplay();
  beginInput();
  beginStorage();
  loadNetworkSettings();

  if (_wifiEnabled) {
    beginNetwork();
  }
}

void AppController::notifyJobFinished() {
  // Footer safe park melakukan G28. Setelah homing, workspace harus dipilih ulang
  // sebelum job berikutnya agar offset G54-G59.3 dimuat kembali secara eksplisit.
  _homeCycleIssued = false;
  invalidateWorkOrigin();

  if (_selectedJobSource != JobSource::SdCard || !_sdCard.hasSelectedJobFile()) {
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
      _buzzer.play(BuzzerHandler::Cue::Warning);
    }
    showInfoScreen(
      "JOB SELESAI",
      "Tidak ada file",
      UiState::Standby,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return;
  }

  if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
    _buzzer.play(BuzzerHandler::Cue::JobFinished);
  }
  showInfoScreen(
    "JOB SELESAI",
    "Home + pilih G54",
    UiState::Standby,
    AppConfig::CONFIRM_RESULT_MESSAGE_MS
  );
}


String AppController::computeJobName() const {
  if (_selectedJobSource == JobSource::SdCard && _sdCard.hasSelectedJobFile()) {
    return basenameForLcd(_sdCard.selectedJobFile());
  }
  return String();
}

String AppController::computeEta() const {
  if (_selectedJobSource != JobSource::SdCard || !_sdCard.hasSelectedJobFile()) {
    return String();
  }

  if (!_job.estimateReady()) {
    return String("--:--:--");
  }

  uint32_t seconds = _job.state() == GcodeJobController::State::Ready
    ? _job.totalEstimateSeconds()
    : _job.remainingEstimateSeconds();
  return formatDurationHms(seconds);
}

int AppController::computeProgress() const {
  if (_selectedJobSource != JobSource::SdCard || !_sdCard.hasSelectedJobFile()) {
    return -1;
  }
  return _job.progressPercent();
}

String AppController::getLocalTimeString() const {
  return _time.timeShortString();
}

void AppController::update() {
  _buzzer.update();

  if (_inputStarted) {
    updateInput();
  }

  if (_storageStarted && _sdCard.update()) {
    handleSdCardStateChanged();
  }

  updateStandbyJobButtons();
  if (_storageStarted && _sdCard.isReady()) {
    _job.updateAnalysis(AppConfig::JOB_ANALYSIS_LINES_PER_LOOP);
  }

  updateAboutScreen();
  updateMqttBrokerEditorRedraw();

  if (_uiState == UiState::Info &&
      _infoAutoCloseMs > 0 &&
      millis() - _infoShownAt >= _infoAutoCloseMs) {
    dismissInfoScreen();
  }

  updateMarlinCommunication();
  if (_marlinConnectionEnabled) {
    _job.update(Serial1);
  }
  updateJobControl();

  MarlinConnectionState cncState = marlinConnectionState();
  if (cncState == MarlinConnectionState::Off ||
      cncState == MarlinConnectionState::Disconnected ||
      cncState == MarlinConnectionState::Lost ||
      cncState == MarlinConnectionState::Error) {
    _homeCycleIssued = false;
    invalidateWorkOrigin();
  }
  _buzzer.setAlarmActive(
    AppConfig::ENABLE_BUZZER_MARLIN_ALARM &&
    isSystemAlarmActive()
  );

  // Logika Long Press Jogging
  if (_activeJogPin != 0 && _uiState == UiState::SetOrigin) {
    if (digitalRead(_activeJogPin) == HIGH) {
      // Tekan singkat baru dijalankan saat tombol dilepas. Dengan cara ini,
      // long-press tidak didahului oleh gerakan 1 mm.
      if (!_jogLongPressHandled) {
        handleJog(_activeJogPin, AppConfig::JOG_STEP_SHORT_MM);
      }
      _activeJogPin = 0;
    } else {
      uint32_t pressDuration = millis() - _jogStartTime;
      if (!_jogLongPressHandled &&
          pressDuration >= AppConfig::BUTTON_LONG_PRESS_MS) {
        handleJog(_activeJogPin, AppConfig::JOG_STEP_LONG_MM);
        _jogLongPressHandled = true;
        _lastJogRepeatTick = millis();
      } else if (_jogLongPressHandled &&
                 millis() - _lastJogRepeatTick >= AppConfig::BUTTON_JOG_REPEAT_MS) {
        handleJog(_activeJogPin, AppConfig::JOG_STEP_LONG_MM);
        _lastJogRepeatTick = millis();
      }
    }
  }

  updateNetwork();

  if (_uiState == UiState::Standby && !_standbyScreenDrawn) {
    showStandbyScreen();
  }

  refreshStandbyNetworkStatus();
  if (_storageStarted) {
    updateStorageFileDisplay();
  }
}

void AppController::updateJobControl() {
  if (_job.isActive()) {
    _spindleOn = _job.spindleOn();
  }

  if (_job.consumeCompletedEvent()) {
    _spindleOn = false;
    notifyJobFinished();
    return;
  }

  if (_job.consumeStoppedEvent()) {
    _spindleOn = false;
    _homeCycleIssued = false;
    invalidateWorkOrigin();
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
      _buzzer.play(BuzzerHandler::Cue::Warning);
    }
    showInfoScreen("JOB STOPPED", "Home & origin ulang", UiState::Standby, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return;
  }

  if (_job.consumeErrorEvent()) {
    _spindleOn = false;
    _homeCycleIssued = false;
    invalidateWorkOrigin();
    if (_marlinConnectionEnabled) {
      Serial1.println("M5");
    }
    if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
      _buzzer.play(BuzzerHandler::Cue::Warning);
    }
    showInfoScreen("JOB ERROR", _job.errorMessage().c_str(), UiState::Standby, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
    return;
  }

  if (_uiState == UiState::Standby &&
      _selectedJobSource == JobSource::SdCard &&
      millis() - _lastJobScreenRefresh >= AppConfig::JOB_UI_REFRESH_MS) {
    _lastJobScreenRefresh = millis();
    showStandbyScreen();
  }
}

void AppController::showJobControlMessage(const char *message) {
  showInfoScreen("JOB CONTROL", message, UiState::Standby, AppConfig::CONFIRM_RESULT_MESSAGE_MS);
}

void AppController::updateStandbyJobButtons() {
  if (_activeStandbyJobPin == 0) {
    return;
  }

  if (digitalRead(_activeStandbyJobPin) == HIGH) {
    if (_activeStandbyJobPin == PinConfig::BTN_X_PLUS &&
        !_standbyJobLongPressHandled &&
        _job.state() == GcodeJobController::State::Paused) {
      if (_job.resume()) {
        showJobControlMessage("RESUME");
      }
    }
    _activeStandbyJobPin = 0;
    return;
  }

  unsigned long heldMs = millis() - _standbyJobPressStartedAt;
  if (_standbyJobLongPressHandled) {
    return;
  }

  if (_activeStandbyJobPin == PinConfig::BTN_X_PLUS &&
      heldMs >= AppConfig::JOB_START_HOLD_MS) {
    _standbyJobLongPressHandled = true;
    _activeStandbyJobPin = 0;
    if (_job.state() == GcodeJobController::State::Ready) {
      showActionConfirm(PendingAction::StartJob, "START JOB", "Jalankan file?");
    } else if (_job.state() == GcodeJobController::State::Analyzing) {
      showJobControlMessage("Sedang analisis");
    } else if (_job.state() == GcodeJobController::State::Completed ||
               _job.state() == GcodeJobController::State::Stopped) {
      showActionConfirm(PendingAction::RepeatJob, "REPEAT JOB", "Ulangi file?");
    } else if (_job.state() == GcodeJobController::State::Error) {
      showJobControlMessage(_job.errorMessage().c_str());
    } else {
      showJobControlMessage("Job belum dipilih");
    }
    return;
  }

  if (_activeStandbyJobPin == PinConfig::BTN_Z_MINUS &&
      heldMs >= AppConfig::JOB_STOP_HOLD_MS) {
    _standbyJobLongPressHandled = true;
    _activeStandbyJobPin = 0;
    if (_job.requestStop()) {
      showJobControlMessage("STOP diminta");
    }
  }
}

// --------------------
// Komunikasi Marlin (Polling & Parsing)
// --------------------

void AppController::updateMarlinCommunication() {
  if (!_marlinConnectionEnabled) {
    return;
  }

  bool setOriginControlActive =
    _uiState == UiState::SetOrigin &&
    AppConfig::ENABLE_SET_ORIGIN_CONTROL;
  bool machineStatusActive = _uiState == UiState::MachineStatus;
  bool marlinMonitorActive = AppConfig::ENABLE_MARLIN_STATUS_MONITOR;

  // 1. Baca data masuk dari Marlin
  while (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      logMarlinRx(line);
      parseMarlinResponse(line);
    }
  }

  // Selama job aktif, hanya job sender yang boleh mengirim command baru.
  if (_job.ownsMarlinStream()) {
    return;
  }

  if (AppConfig::UI_DEVELOPMENT_MODE &&
      !setOriginControlActive &&
      !machineStatusActive &&
      !marlinMonitorActive) {
    return;
  }

  // 2. Kirim permintaan posisi (M114) berkala untuk monitoring posisi nyata Marlin.
  if (marlinMonitorActive || setOriginControlActive || machineStatusActive) {
    if (millis() - _lastM114Request > AppConfig::MARLIN_STATUS_POLL_MS) {
      logMarlinTx("M114");
      Serial1.println("M114");
      _lastM114Request = millis();
      if (_marlinMonitorStartedAt == 0) {
        _marlinMonitorStartedAt = _lastM114Request;
      }
    }
  }

  if (marlinMonitorActive && isMarlinResponding()) {
    prefetchMachineStatus();
  }

  if ((_uiState == UiState::Standby || _uiState == UiState::MachineStatus) &&
      millis() - _lastStatusScreenRefresh > AppConfig::MARLIN_STATUS_SCREEN_REFRESH_MS) {
    _lastStatusScreenRefresh = millis();
    if (_uiState == UiState::Standby) {
      showStandbyScreen();
    } else {
      showMachineStatusScreen();
    }
  }
}

void AppController::parseMarlinResponse(const String &line) {
  _job.handleMarlinResponse(line);

  bool wasResponding = isMarlinResponding();
  _lastMarlinResponseMs = millis();
  _marlinEverResponded = true;

  String lowerLine = line;
  lowerLine.toLowerCase();
  if (lowerLine.startsWith("error:") ||
      lowerLine.startsWith("!!") ||
      lowerLine.startsWith("alarm:")) {
    _marlinErrorActive = true;
  }

  parseMachineFeedrateStatus(line);
  parseMachineGeometryStatus(line);
  parseHomeSensorStatus(line);
  parseSoftEndstopStatus(line);

  if (!wasResponding && _uiState == UiState::MachineStatus) {
    showMachineStatusScreen();
  }

  // Format standar Marlin M114: "X:0.00 Y:0.00 Z:0.00 E:0.00 Count X:0 Y:0 Z:0"
  if (line.indexOf("X:") != -1 && line.indexOf("Y:") != -1 && line.indexOf("Z:") != -1) {
    _marlinErrorActive = false;
    int xPos = line.indexOf("X:") + 2;
    int yPos = line.indexOf("Y:") + 2;
    int zPos = line.indexOf("Z:") + 2;
    int ePos = line.indexOf("E:");

    String xStr = line.substring(xPos, line.indexOf(' ', xPos));
    String yStr = line.substring(yPos, line.indexOf(' ', yPos));
    String zStr = (ePos != -1)
                  ? line.substring(zPos, line.indexOf(' ', zPos))
                  : line.substring(zPos);

    _posX = xStr.toFloat();
    _posY = yStr.toFloat();
    _posZ = zStr.toFloat();
    _positionValid = true;

    // Update tampilan jika sedang standby agar angka sinkron
    if (_uiState == UiState::Standby) {
      showStandbyScreen();
    } else if (_uiState == UiState::SetOrigin) {
      updateJogDisplay();
    }
  }
}

// --------------------
// Event dari input
// --------------------

void AppController::onButtonPressed(uint8_t buttonNumber) {
  if (AppConfig::ENABLE_INPUT_EVENT_LOG) {
    Serial.print("Event tombol dikirim ke aplikasi: ");
    Serial.println(buttonNumber);
  }

  if (AppConfig::ENABLE_BUZZER_KEY_FEEDBACK) {
    if (buttonNumber == (uint8_t)PinConfig::BTN_ENTER) {
      _buzzer.play(BuzzerHandler::Cue::Select);
    } else if (buttonNumber == (uint8_t)PinConfig::BTN_BACK) {
      _buzzer.play(BuzzerHandler::Cue::Back);
    }
  }

  // Tombol job tetap dapat dipakai saat layar informasi sementara tampil.
  if (_job.isActive()) {
    if (buttonNumber == PinConfig::BTN_X_MINUS) {
      if (_job.requestPause()) {
        showJobControlMessage("PAUSE diminta");
      }
      return;
    }

    if (buttonNumber == PinConfig::BTN_Z_MINUS) {
      _activeStandbyJobPin = buttonNumber;
      _standbyJobPressStartedAt = millis();
      _standbyJobLongPressHandled = false;
      return;
    }

    if (buttonNumber == PinConfig::BTN_X_PLUS &&
        _job.state() == GcodeJobController::State::Paused) {
      _activeStandbyJobPin = buttonNumber;
      _standbyJobPressStartedAt = millis();
      _standbyJobLongPressHandled = false;
      return;
    }
  }

  if (_uiState == UiState::Info) {
    if (buttonNumber == (uint8_t)PinConfig::BTN7 ||
        buttonNumber == (uint8_t)PinConfig::BTN8) {
      dismissInfoScreen();
    }
    return;
  }

  if (_uiState == UiState::About) {
    if (buttonNumber == (uint8_t)PinConfig::BTN_ENTER ||
        buttonNumber == (uint8_t)PinConfig::BTN_BACK) {
      exitAboutScreen();
    }
    return;
  }

  if (_uiState == UiState::BrokerEditor) {
    if (buttonNumber == (uint8_t)PinConfig::BTN_ENTER) {
      moveMqttBrokerEditorRight();
    } else if (buttonNumber == (uint8_t)PinConfig::BTN_BACK) {
      moveMqttBrokerEditorLeft();
    }
    return;
  }

  if (_uiState == UiState::Standby && buttonNumber == PinConfig::BTN_X_PLUS) {
    _activeStandbyJobPin = buttonNumber;
    _standbyJobPressStartedAt = millis();
    _standbyJobLongPressHandled = false;
    return;
  }

  // Jika sedang standby dan tombol ENTER ditekan -> masuk menu
  if (_uiState == UiState::Standby && buttonNumber == (uint8_t)PinConfig::BTN7) {
    showMainMenuScreen();
    return;
  }

    // NOTE: tombol khusus jog sebelumnya hanya diizinkan saat mode Jogging.
    // Sekarang mode "Jogging" digantikan oleh menu "Home/G92" sehingga
    // penanganan per-axis harus diimplementasikan jika diperlukan.

  // If in FileList view, BTN6 previews the currently highlighted file on LCD
  if (_uiState == UiState::FileList && buttonNumber == (uint8_t)PinConfig::BTN6) {
    if (_fileListSelected < _fileListPaths.size() &&
        _fileListSelected < _fileListIsDirectory.size() &&
        !_fileListIsDirectory[_fileListSelected]) {
      std::vector<String> preview = _sdCard.previewFileLines(_fileListPaths[_fileListSelected].c_str(), 4, 18);
      if (preview.empty()) {
        _lcd.showMessage("Preview", "Tidak dapat membaca file");
      } else {
        _lcd.showList("Preview", preview);
      }
    }
    return;
  }

  // Jika sedang di menu dan tombol BACK ditekan -> kembali ke menu sebelumnya atau Standby
  if (_uiState == UiState::Menu && buttonNumber == (uint8_t)PinConfig::BTN8) {
    if (!popMenuState()) {
      showStandbyScreen();
    }
    return;
  }

  // BACK handling for submenus is covered by Menu history/pop behavior above

  // Jika sedang melihat daftar file dan BACK ditekan -> kembali ke Menu
  if (_uiState == UiState::FileList && buttonNumber == (uint8_t)PinConfig::BTN8) {
    if (_currentSdDirectory != "/") {
      openSdFileList(parentSdDirectory(_currentSdDirectory));
      return;
    }

    _uiState = UiState::Menu;
    if (!popMenuState()) {
      showMainMenuScreen(1);
    }
    return;
  }

  if (_uiState == UiState::MachineStatus && buttonNumber == (uint8_t)PinConfig::BTN8) {
    if (_machineFeedrateEditing || _machineWorkAreaEditing) {
      _machineFeedrateEditing = false;
      _machineWorkAreaEditing = false;
      showMachineStatusScreen();
      return;
    }

    _uiState = UiState::Menu;
    if (!popMenuState()) {
      showMainMenuScreen(2);
    }
    return;
  }

  if (_uiState == UiState::NetworkStatus && buttonNumber == (uint8_t)PinConfig::BTN8) {
    _uiState = UiState::Menu;
    if (!popMenuState()) {
      showMainMenuScreen(3);
    }
    return;
  }

  if (isConfirmationState()) {
    if (buttonNumber == (uint8_t)PinConfig::BTN7) {
      // Tombol ENTER fisik selalu berarti Yes.
      acceptConfirmation();
      return;
    }

    if (buttonNumber == (uint8_t)PinConfig::BTN8) {
      // Tombol BACK fisik selalu berarti No.
      rejectConfirmation();
      return;
    }

    return;
  }

  if (_uiState == UiState::SetOrigin) {
    if (buttonNumber == (uint8_t)PinConfig::BTN7) {
      String title = String(workspaceCode(_activeWorkspaceIndex)) + " ORIGIN";
      showActionConfirm(
        PendingAction::SetOrigin,
        title.c_str(),
        "Perbarui origin?"
      );
      return;
    }

    if (buttonNumber == (uint8_t)PinConfig::BTN8) {
      _activeJogPin = 0;
      _isJogAdjusting = false;
      _uiState = UiState::Menu;
      _menu.redraw();
      return;
    }

    bool isJogButton = (buttonNumber == PinConfig::BTN1 || buttonNumber == PinConfig::BTN2 ||
                        buttonNumber == PinConfig::BTN3 || buttonNumber == PinConfig::BTN4 ||
                        buttonNumber == PinConfig::BTN5 || buttonNumber == PinConfig::BTN6);

    if (isJogButton) {
      _activeJogPin = buttonNumber;
      _jogStartTime = millis();
      _jogLongPressHandled = false;
      _lastJogRepeatTick = 0;
    }

    updateJogDisplay();
    return;
  }

  // If button is ENTER (BTN7) treat as select/OK
  if (buttonNumber == (uint8_t)PinConfig::BTN7) {
    processSelectAction();
    return;
  }

}

void AppController::handleJog(uint8_t pin, float distance) {
  char axis = 'X';
  float direction = 1.0f;

  if (pin == PinConfig::BTN1) { axis = 'X'; direction = 1.0f; }
  else if (pin == PinConfig::BTN2) { axis = 'X'; direction = -1.0f; }
  else if (pin == PinConfig::BTN3) { axis = 'Y'; direction = 1.0f; }
  else if (pin == PinConfig::BTN4) { axis = 'Y'; direction = -1.0f; }
  else if (pin == PinConfig::BTN5) { axis = 'Z'; direction = 1.0f; }
  else if (pin == PinConfig::BTN6) { axis = 'Z'; direction = -1.0f; }
  else return;

  float moveVal = distance * direction;
  if (axis == 'X') _posX += moveVal;
  else if (axis == 'Y') _posY += moveVal;
  else if (axis == 'Z') _posZ += moveVal;

  if (AppConfig::ENABLE_SET_ORIGIN_CONTROL && canSendMarlinCommand()) {
    // Kirim G-Code ke SKR (G91 = Relative Positioning)
    Serial1.println("G91");
    Serial1.print("G1 ");
    Serial1.print(axis);
    Serial1.print(moveVal);
    Serial1.print(" F");
    Serial1.println(AppConfig::JOG_SPEED_NORMAL);
    Serial1.println("G90");
  }

  updateJogDisplay();
}

void AppController::updateJogDisplay() {
  if (_uiState != UiState::SetOrigin) return;

  char top[32], mid[32], bot[32];
  char axis = _jogAxisIndex == 0 ? 'X' : (_jogAxisIndex == 1 ? 'Y' : 'Z');
  snprintf(
    top,
    sizeof(top),
    _isJogAdjusting ? "%s ADJUST %c" : "%s SELECT %c",
    workspaceCode(_activeWorkspaceIndex),
    axis
  );

  // Tanda kursor: > untuk seleksi, * untuk mode ubah nilai
  char xMark = (_jogAxisIndex == 0) ? (_isJogAdjusting ? '*' : '>') : ' ';
  char yMark = (_jogAxisIndex == 1) ? (_isJogAdjusting ? '*' : '>') : ' ';
  char zMark = (_jogAxisIndex == 2) ? (_isJogAdjusting ? '*' : '>') : ' ';

  snprintf(mid, sizeof(mid), "%cX:%7.2f %cY:%7.2f", xMark, _posX, yMark, _posY);
  snprintf(bot, sizeof(bot), "%cZ:%7.2f", zMark, _posZ);

  _lcd.showStandbyLines(
    top,
    mid,
    bot,
    nullptr,
    AppConfig::ENABLE_SOFT_ENDSTOP_ON_SET_ORIGIN
      ? "ENT=SAVE+SE BACK=EXIT"
      : "ENTER=SAVE BACK=EXIT",
    -1,
    nullptr
  );
}

void AppController::enterSetOrigin() {
  _uiState = UiState::SetOrigin;
  _jogAxisIndex = 0;
  _isJogAdjusting = false;
  _encoderMovedWhilePressed = false;
  _activeJogPin = 0;
  _lastM114Request = 0;
  updateJogDisplay();
}

void AppController::confirmSetOrigin(UiState returnState) {
  _activeJogPin = 0;
  _isJogAdjusting = false;

  bool commandSent =
    AppConfig::ENABLE_SET_ORIGIN_CONTROL &&
    AppConfig::ENABLE_WORK_COORDINATE_CONTROL &&
    _workspaceSelected &&
    canSendMarlinCommand();

  if (commandSent) {
    Serial1.println("M400");
    Serial1.println("G92 X0 Y0 Z0");
    Serial1.println("M500");

    if (AppConfig::ENABLE_SOFT_ENDSTOP_ON_SET_ORIGIN) {
      Serial1.println("M211 S1");
      _softEndstopEnabled = true;
      _softEndstopLoaded = true;
      _lastM211Request = millis();
      _lastMarlinStatusPoll = _lastM211Request;
    }
    _originConfigured = true;
  }

  _posX = 0.0f;
  _posY = 0.0f;
  _posZ = 0.0f;

  String resultTitle = "Origin: simulasi";
  const char *resultMessage = "SoftEnd: OFF";
  if (commandSent) {
    resultTitle = String(workspaceCode(_activeWorkspaceIndex)) + " tersimpan";
    resultMessage = AppConfig::ENABLE_SOFT_ENDSTOP_ON_SET_ORIGIN
      ? "SoftEnd: ON"
      : "SoftEnd: OFF";
  }

  _infoReturnState = returnState;
  _infoShownAt = millis();
  _infoAutoCloseMs = AppConfig::CONFIRM_RESULT_MESSAGE_MS;
  _uiState = UiState::Info;
  _lcd.showCenteredMessage(resultTitle.c_str(), resultMessage);
}

void AppController::processSelectAction() {
  if (_uiState == UiState::About) {
    exitAboutScreen();
    return;
  }

  if (_uiState == UiState::Standby) {
    showMainMenuScreen();
    return;
  }

  // Klik rotary mengikuti pilihan Yes/No yang sedang disorot.
  if (isConfirmationState()) {
    executeSelectedConfirmation();
    return;
  }

  if (_uiState == UiState::Info) {
    dismissInfoScreen();
    return;
  }

  if (_uiState == UiState::BrokerEditor) {
    moveMqttBrokerEditorRight();
    return;
  }

  if (_uiState == UiState::Menu) {
    size_t sel = _menu.selectedIndex();
    const std::vector<String> &items = _menu.items();
    if (sel >= items.size()) {
      return;
    }

    String choice = items[sel];

    if (choice == "Move & Jog") {
      showSubMenu(
        "MOVE & JOG",
        {"Home All (G28)", "Work Origin"}
      );
      return;
    }

    if (choice == "Work Origin") {
      showSubMenu("WORK ORIGIN", workOriginItems());
      return;
    }

    if (choice == "Select Job") {
      if (_job.isActive()) {
        showJobControlMessage("Pause/Stop job dulu");
        return;
      }
      if (!_sdCard.isReady()) {
        if (AppConfig::ENABLE_BUZZER_NOTIFICATION) {
          _buzzer.play(BuzzerHandler::Cue::Warning);
        }
        showInfoScreen("SD CARD", "Belum siap");
        return;
      }

      pushMenuState(items, sel);
      openSdFileList("/");
      return;
    }

    if (choice == "Machine Ctrl&Status") {
      pushMenuState(items, sel);
      _machineFeedrateEditing = false;
      _machineWorkAreaEditing = false;
      _machineStatusSelected = 0;
      _machineStatusOffset = 0;
      showMachineStatusScreen();
      return;
    }

    if (choice == "Network") {
      pushMenuState(items, sel);
      _networkStatusSelected = 0;
      _networkStatusOffset = 0;
      showNetworkStatusScreen();
      return;
    }

    if (choice == "About") {
      showAboutScreen();
      return;
    }

    if (choice == "Home All (G28)") {
      if (_job.isActive()) {
        showJobControlMessage("Job sedang aktif");
        return;
      }
      showActionConfirm(
        PendingAction::HomeAll,
        "CONFIRM HOME",
        "Jalankan Home All?"
      );
      return;
    }

    if (choice == "Pilih Origin") {
      requestWorkspaceSelection(_pendingWorkspaceIndex);
      return;
    }

    if (choice == "Perbarui Origin") {
      beginWorkspaceOriginUpdate(_pendingWorkspaceIndex);
      return;
    }

    for (uint8_t index = 0; index < AppConfig::WORK_COORDINATE_COUNT; ++index) {
      if (choice == workspaceCode(index)) {
        _pendingWorkspaceIndex = index;
        String title = String(workspaceCode(index)) + " ORIGIN";
        showSubMenu(title.c_str(), {"Pilih Origin", "Perbarui Origin"});
        return;
      }
    }

    if (choice == "Spindle ON (M3)") {
      if (AppConfig::UI_DEVELOPMENT_MODE) {
        showInfoScreen("UI PREVIEW", "M3 belum dikirim");
      } else if (!canSendMarlinCommand()) {
        showCncUnavailable(UiState::Menu);
      } else {
        Serial1.println("M3");
        showInfoScreen("SPINDLE", "M3 dikirim");
      }
      return;
    }

    if (choice == "Spindle OFF (M5)") {
      if (AppConfig::UI_DEVELOPMENT_MODE) {
        showInfoScreen("UI PREVIEW", "M5 belum dikirim");
      } else if (!canSendMarlinCommand()) {
        showCncUnavailable(UiState::Menu);
      } else {
        Serial1.println("M5");
        showInfoScreen("SPINDLE", "M5 dikirim");
      }
      return;
    }

    if (choice == "Connection Status") {
      showInfoScreen(
        "NETWORK STATUS",
        _wifi.isConnected() ? "WiFi connected" : "WiFi disconnected"
      );
      return;
    }

    if (choice == "IP Address") {
      String ip = _wifi.isConnected() ? WiFi.localIP().toString() : "Wifi Belum terhubung";
      showInfoScreen("IP ADDRESS", ip.c_str());
      return;
    }

    if (choice == "Reset WiFi") {
      showInfoScreen("UI PREVIEW", "Reset WiFi tahap 2");
      return;
    }

    showInfoScreen("UI PREVIEW", "Belum diimplementasi");
    return;
  }

  // If currently browsing file list and ENTER pressed, show confirm dialog
  if (_uiState == UiState::FileList) {
    enterSelectedFileListItem();
    return;
  }

  if (_uiState == UiState::MachineStatus) {
    handleMachineStatusSelect();
    return;
  }

  if (_uiState == UiState::NetworkStatus) {
    handleNetworkStatusSelect();
    return;
  }

  // Klik encoder memilih mode axis. Tekan-putar tidak mengubah mode.
  if (_uiState == UiState::SetOrigin) {
    if (_encoderMovedWhilePressed) {
      _encoderMovedWhilePressed = false;
      return;
    }

    _isJogAdjusting = !_isJogAdjusting;
    updateJogDisplay();
    return;
  }
}

void AppController::onEncoderTurned(int8_t direction) {
  if (AppConfig::ENABLE_INPUT_EVENT_LOG) {
    Serial.print("Event encoder dikirim ke aplikasi: ");
    Serial.println(direction > 0 ? "Z+" : "Z-");
  }

  if (_uiState == UiState::Info) {
    return;
  }

  if (_uiState == UiState::About) {
    return;
  }

  if (_uiState == UiState::BrokerEditor) {
    changeMqttBrokerEditorDigit(direction);
    return;
  }

  if (isConfirmationState()) {
    _confirmSelectedIndex = (_confirmSelectedIndex == 0) ? 1 : 0;
    redrawCurrentConfirmation();
    return;
  }

  // Logika Set Origin untuk encoder.
  if (_uiState == UiState::SetOrigin) {
    bool swPressed = (digitalRead(PinConfig::ENCODER_SW) == LOW);

    if (!_isJogAdjusting && !swPressed) {
      // Mode Pilih Sumbu (ketika tombol tidak ditekan)
      if (direction > 0) _jogAxisIndex = (_jogAxisIndex + 1) % 3;
      else _jogAxisIndex = (_jogAxisIndex == 0) ? 2 : _jogAxisIndex - 1;
    } else {
      // Mode Gerakkan Sumbu (baik mode sticky atau mode tekan-putar)
      if (swPressed) _encoderMovedWhilePressed = true;

      float distance = swPressed
        ? AppConfig::JOG_STEP_LONG_MM
        : AppConfig::JOG_STEP_SHORT_MM;
      uint8_t targetPin = 0;
      if (_jogAxisIndex == 0) targetPin = (direction > 0) ? PinConfig::BTN1 : PinConfig::BTN2;
      else if (_jogAxisIndex == 1) targetPin = (direction > 0) ? PinConfig::BTN3 : PinConfig::BTN4;
      else if (_jogAxisIndex == 2) targetPin = (direction > 0) ? PinConfig::BTN5 : PinConfig::BTN6;

      if (targetPin != 0) handleJog(targetPin, distance);
    }
    updateJogDisplay();
    return;
  }

  // Jika sedang di menu, navigasi pilihan
  if (_uiState == UiState::Menu) {
    if (direction > 0) _menu.selectNext();
    else _menu.selectPrev();
    return;
  }

  // Jika sedang di daftar file, navigasi file list
  if (_uiState == UiState::FileList) {
    const size_t pageSize = 5;
    if (direction > 0) {
      // move down
      if (_fileListSelected + 1 < _fileList.size()) {
        _fileListSelected++;
        if (_fileListSelected >= _fileListOffset + pageSize) {
          _fileListOffset += pageSize;
        }
      }
    } else {
      // move up
      if (_fileListSelected > 0) {
        _fileListSelected--;
        if (_fileListSelected < _fileListOffset) {
          if (_fileListOffset >= pageSize) _fileListOffset -= pageSize;
          else _fileListOffset = 0;
        }
      }
    }

    showCurrentFileListPage();
    return;
  }

  if (_uiState == UiState::MachineStatus) {
    if (_machineFeedrateEditing) {
      bool fastStep = (digitalRead(PinConfig::ENCODER_SW) == LOW);
      int step = fastStep
        ? (int)AppConfig::MACHINE_FEEDRATE_FAST_STEP_MM_S
        : (int)AppConfig::MACHINE_FEEDRATE_STEP_MM_S;
      int delta = direction > 0 ? step : -step;

      if (_machineStatusSelected == MACHINE_ITEM_FEED_XY) {
        _machineFeedrateXY = clampFeedrate(
          (int)_machineFeedrateXY + delta,
          AppConfig::MACHINE_FEEDRATE_MAX_MM_S
        );
      } else if (_machineStatusSelected == MACHINE_ITEM_FEED_Z) {
        _machineFeedrateZ = clampFeedrate(
          (int)_machineFeedrateZ + delta,
          AppConfig::MACHINE_FEEDRATE_MAX_MM_S
        );
      }

      showMachineStatusScreen();
      return;
    }

    if (_machineWorkAreaEditing) {
      bool fastStep = (digitalRead(PinConfig::ENCODER_SW) == LOW);
      int step = fastStep
        ? (int)AppConfig::MACHINE_WORK_AREA_FAST_STEP_MM
        : (int)AppConfig::MACHINE_WORK_AREA_STEP_MM;
      int delta = direction > 0 ? step : -step;

      if (_machineStatusSelected == MACHINE_ITEM_AREA_X) {
        _machineWorkAreaX = clampUnsigned(
          (int)_machineWorkAreaX + delta,
          AppConfig::MACHINE_WORK_AREA_MIN_MM,
          AppConfig::MACHINE_WORK_AREA_MAX_MM
        );
      } else if (_machineStatusSelected == MACHINE_ITEM_AREA_Y) {
        _machineWorkAreaY = clampUnsigned(
          (int)_machineWorkAreaY + delta,
          AppConfig::MACHINE_WORK_AREA_MIN_MM,
          AppConfig::MACHINE_WORK_AREA_MAX_MM
        );
      } else if (_machineStatusSelected == MACHINE_ITEM_AREA_Z) {
        _machineWorkAreaZ = clampUnsigned(
          (int)_machineWorkAreaZ + delta,
          AppConfig::MACHINE_WORK_AREA_MIN_MM,
          AppConfig::MACHINE_WORK_AREA_MAX_MM
        );
      }

      showMachineStatusScreen();
      return;
    }

    const size_t itemCount = machineStatusItems().size();
    if (itemCount > 0) {
      if (direction > 0) {
        _machineStatusSelected = (_machineStatusSelected + 1) % itemCount;
      } else {
        _machineStatusSelected = (_machineStatusSelected == 0)
          ? itemCount - 1
          : _machineStatusSelected - 1;
      }
    }

    showMachineStatusScreen();
    return;
  }

  if (_uiState == UiState::NetworkStatus) {
    const size_t itemCount = networkStatusItems().size();
    if (itemCount > 0) {
      if (direction > 0) {
        _networkStatusSelected = (_networkStatusSelected + 1) % itemCount;
      } else {
        _networkStatusSelected = (_networkStatusSelected == 0)
          ? itemCount - 1
          : _networkStatusSelected - 1;
      }
    }

    showNetworkStatusScreen();
    return;
  }

}

void AppController::onEncoderPressed() {
  if (AppConfig::ENABLE_INPUT_EVENT_LOG) {
    Serial.println("Encoder digunakan sebagai OK / Select");
  }

  if (AppConfig::ENABLE_BUZZER_KEY_FEEDBACK) {
    _buzzer.play(BuzzerHandler::Cue::Select);
  }

  // Perform unified select action (same as BTN7)
  processSelectAction();
}

// --------------------
// File list selection handling
// --------------------
  // When user presses ENTER while in FileList state, select the highlighted
  // file and set it as the job source. This function uses helper method
  // selectSdCardJobFile to register the selected path.
// Comments are intentionally included for thesis documentation.

// Note: _fileList contains display names while _fileListPaths has full paths.
// Selecting a file changes internal state _selectedJobSource and returns
// user feedback on the LCD.
bool AppController::selectFileFromList() {
  if (_job.isActive()) {
    showJobControlMessage("Stop job lebih dulu");
    return false;
  }

  if (_fileListSelected >= _fileListPaths.size()) {
    showInfoScreen(
      "File",
      "Invalid selection",
      UiState::FileList,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return false;
  }

  const String &path = _fileListPaths[_fileListSelected];

  if (!_sdCard.isReady()) {
    showInfoScreen(
      "SD Card",
      "Belum siap",
      UiState::FileList,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return false;
  }

  SdJobFileValidation validation = _sdCard.validateJobFilePath(path);
  if (!validation.valid) {
    showInfoScreen(
      "SD Card",
      validation.reason.c_str(),
      UiState::FileList,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return false;
  }

  if (_sdCard.selectJobFile(validation.path.c_str())) {
    _selectedJobSource = JobSource::SdCard;
    // Pemilihan file hanya membaca dan menganalisis G-code. Karena tidak ada
    // gerakan mesin, status Home dan Work Origin yang sudah divalidasi tetap berlaku.
    if (!_job.prepare(_sdCard, validation.path)) {
      showInfoScreen(
        "ANALISIS JOB",
        _job.errorMessage().c_str(),
        UiState::FileList,
        AppConfig::CONFIRM_RESULT_MESSAGE_MS
      );
      return false;
    }
    showInfoScreen(
      "SD Card",
      "Tahan X+ = Start",
      UiState::Standby,
      AppConfig::CONFIRM_RESULT_MESSAGE_MS
    );
    return true;
  }

  showInfoScreen(
    "SD Card",
    "Pilih gagal",
    UiState::FileList,
    AppConfig::CONFIRM_RESULT_MESSAGE_MS
  );
  return false;
}

// Implementasi helper untuk memilih file job dari SD card
bool AppController::selectSdCardJobFile(const char *path) {
  if (!_sdCard.isReady()) {
    _lcd.showMessage("SD Card", "Belum siap");
    return false;
  }

  bool ok = _sdCard.selectJobFile(path);
  if (ok) {
    _selectedJobSource = JobSource::SdCard;
    // Helper pemilihan file juga tidak mengubah referensi posisi mesin.
    ok = _job.prepare(_sdCard, path);
    _lcd.showMessage("SD Card", ok ? "File dipilih" : _job.errorMessage().c_str());
  } else {
    _lcd.showMessage("SD Card", "Pilih gagal");
  }
  return ok;
}

void AppController::printSelectedJobFile() {
  if (_selectedJobSource == JobSource::SdCard && _sdCard.hasSelectedJobFile()) {
    _sdCard.printSelectedJobFile();
  } else {
    _lcd.showMessage("Job", "Tidak ada file");
  }
}

// --------------------
// Tahap inisialisasi
// --------------------

void AppController::beginSerial() {
  Serial.begin(AppConfig::SERIAL_BAUD_RATE);
  delay(200);
  Serial.println();
  Serial.print("[");
  Serial.print(AppConfig::FIRMWARE_NAME);
  Serial.print("] Firmware v");
  Serial.println(AppConfig::FIRMWARE_VERSION);
  Serial.println(AppConfig::FIRMWARE_DESCRIPTION);
  Serial.println("AppController: Serial monitor ready");
}

void AppController::setDisplayEnabled(bool enabled) {
  _displayEnabled = enabled;
  if (!_displayEnabled) {
    _inputStarted = false;
    _uiState = UiState::Standby;
    _standbyScreenDrawn = true;
  }
}

void AppController::beginDisplay(bool holdBootScreen) {
  Serial.println("AppController: beginDisplay()");

  if (!_displayEnabled) {
    _uiState = holdBootScreen ? UiState::Boot : UiState::Standby;
    _standbyScreenDrawn = !holdBootScreen;
    Serial.println("[LCD] Nonaktif sementara untuk pengembangan WiFi/MQTT");
    return;
  }

  _lcd.begin();
  Serial.println("AppController: LcdHandler.begin() returned");
  String bootSubtitle = String("Version ") + AppConfig::FIRMWARE_VERSION;
  _lcd.showBootSplash(
    AppConfig::BOOT_TITLE,
    bootSubtitle.c_str(),
    AppConfig::BOOT_FOOTER,
    AppConfig::BOOT_SPLASH_MS
  );
  Serial.println("AppController: showBootSplash() called");

  if (holdBootScreen) {
    _uiState = UiState::Boot;
    return;
  }

  _uiState = UiState::Standby;
  Serial.println("AppController: standby screen deferred");
}



void AppController::beginInput() {
  activeApp = this;
  _buttons.begin();
  _buttons.setCallback(handleButtonPressed);

  _encoder.begin();
  _encoder.setTurnCallback(handleEncoderTurned);
  _encoder.setPressCallback(handleEncoderPressed);
  _inputStarted = true;
}

void AppController::beginBuzzer() {
  _buzzer.begin();
}

void AppController::beginMarlinConnection() {
  // UART CNC hanya dibuka saat fitur Marlin aktif agar Serial Monitor tetap aman.
  Serial1.begin(AppConfig::BTT_UART_BAUD, SERIAL_8N1, PinConfig::CNC_UART_RX, PinConfig::CNC_UART_TX);
  _marlinConnectionEnabled = true;
  _marlinEverResponded = false;
  _marlinErrorActive = false;
  _positionValid = false;
  _lastMarlinResponseMs = 0;
  _marlinMonitorStartedAt = 0;
  _lastM114Request = 0;
  applyDefaultMachineWorkArea(_machineWorkAreaX, _machineWorkAreaY, _machineWorkAreaZ);
  _machineWorkAreaLoaded = true;
  _machineWorkAreaFromFallback = true;
  _machineStatusPrefetchCursor = 0;
  _homeCycleIssued = false;
  invalidateWorkOrigin();
  Serial.print("[CNC] Komunikasi Marlin aktif baud=");
  Serial.print(AppConfig::BTT_UART_BAUD);
  Serial.print(" RX=");
  Serial.print(PinConfig::CNC_UART_RX);
  Serial.print(" TX=");
  Serial.println(PinConfig::CNC_UART_TX);
}

void AppController::beginStorage() {
  // Inisialisasi SD card tanpa menimpa layar boot/standby.
  // Daftar file dimuat saat user membuka menu Select Job.
  _sdCard.begin();
  _storageStarted = true;
  _lastSdReady = _sdCard.isReady();
  _sdStatusPublishPending = true;
}



void AppController::beginNetwork() {
  loadNetworkSettings();
  startNetworkFromSettings();
}

void AppController::runSdCardDiagnostics() {
  if (_sdCard.isReady()) {
    _sdCard.runDiagnostics();
  } else {
    _lcd.showMessage("SD Card", "Belum siap");
  }
}

void AppController::updateInput() {
  _buttons.update();
  _encoder.update();
}

void AppController::updateNetwork() {
  if (!_wifiEnabled) {
    _time.update(false);
    return;
  }

  _wifi.update();

  if (!_wifi.isConnected()) {
    _time.update(false);
    _networkStarted = false;
    return;
  }

  _networkStarted = true;
  _time.update(true);

  if (_uiState == UiState::BrokerEditor) {
    // Hindari percobaan connect/reconnect MQTT yang dapat menahan loop input
    // saat user sedang memutar digit broker pada LCD.
    return;
  }

  if (_uiState == UiState::NetworkStatus &&
      millis() - _lastStatusScreenRefresh > AppConfig::MARLIN_STATUS_SCREEN_REFRESH_MS) {
    _lastStatusScreenRefresh = millis();
    showNetworkStatusScreen();
  }

  if (_mqttEnabled && !_mqttStarted) {
    _cloud.begin();
    _mqttStarted = true;
    _sdStatusPublishPending = true;
  }

  if (_mqttEnabled && _mqttStarted) {
    _cloud.update();
    bool mqttConnected = _cloud.isConnected();
    if (mqttConnected && !_lastMqttConnected) {
      _sdStatusPublishPending = true;
    }
    _lastMqttConnected = mqttConnected;
    if (_cloud.consumeFileListRequest()) {
      publishSdJobFileList();
    }
    handleMqttFileSelectRequest();
    handleMqttFileTransferRequests();
    handleMqttFileDeleteRequest();
    handleMqttSdStatusRequest();
    publishMqttMonitoring();
  }
}

void AppController::updateStorageFileDisplay() {
  // Browser SD dimuat ulang saat user membuka folder, bukan refresh berkala.
  // Ini mencegah kursor file meloncat saat encoder sedang diputar.
}
