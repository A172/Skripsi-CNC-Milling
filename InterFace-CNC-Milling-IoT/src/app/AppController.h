#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <Arduino.h>
#include <vector>
#include <utility>
#include <WString.h>

// Source of job files
enum class JobSource {
  None,
  SdCard
};

#include "../config/AppConfig.h"
#include "../config/MqttConfig.h"
#include "../config/PinConfig.h"
#include "../display/LcdHandler.h"
#include "../input/ButtonHandler.h"
#include "../input/EncoderHandler.h"
#include "../job/GcodeJobController.h"
#include "../output/BuzzerHandler.h"
#include "../network/CloudMqtt.h"
#include "../network/WifiPortal.h"
#include "../storage/SdCardReader.h"
#include "../time/TimeManager.h"
#include "../display/Menu.h"

class AppController {
  public:
    AppController();

    // Siklus utama aplikasi.
    void begin();
    void update();

    // Tahap inisialisasi yang bisa dipanggil dari main.cpp.
    void beginSerial();
    void setDisplayEnabled(bool enabled);
    void beginDisplay(bool holdBootScreen = false);
    void beginInput();
    void beginBuzzer();
    void beginMarlinConnection();
    void beginStorage();
    void beginNetwork();

    // Dipanggil oleh job sender saat file G-code selesai dieksekusi.
    void notifyJobFinished();

  // Fungsi pengujian hardware.
  void runSdCardDiagnostics();

    // Fungsi pemilihan file job dari media penyimpanan.
  bool selectSdCardJobFile(const char *path);
    void printSelectedJobFile();
  // Internal helper to finalize selection from file list (shows messages)
  bool selectFileFromList();

    // Fungsi update yang bisa dipanggil dari main.cpp.
    void updateInput();
    void updateNetwork();
    void updateStorageFileDisplay();

    // Penangan event dari input.
    void onButtonPressed(uint8_t buttonNumber);
    void onEncoderTurned(int8_t direction);
    void onEncoderPressed();
  // Helper for unified "select/enter" action (used by encoder press and BTN7)
  void processSelectAction();

  private:
    enum class MarlinConnectionState {
      Off,
      Waiting,
      Disconnected,
      Connected,
      Lost,
      Error
    };

    // Modul hardware lokal.
    ButtonHandler _buttons;
    EncoderHandler _encoder;
    BuzzerHandler _buzzer;
    LcdHandler _lcd;
    Menu _menu;
    SdCardReader _sdCard;
    GcodeJobController _job;

    // UI state
    enum class UiState {
      Boot,
      Standby,
      Menu,
      Info,
      FileList,
      ConfirmSelect,
      ConfirmAction,
      SetOrigin,
      BrokerEditor,
      MachineStatus,
      NetworkStatus,
      About
    };
    UiState _uiState = UiState::Boot;

    // Dummy posisi mesin (akan digantikan oleh pembacaan aktual dari MCU/driver)
    float _posX = 0.0f;
    float _posY = 0.0f;
    float _posZ = 0.0f;
    // Selected job source
    JobSource _selectedJobSource = JobSource::None;
  // File list state
  // _fileList contains display-friendly names (basenames) shown on LCD
  std::vector<String> _fileList;
  // _fileListPaths stores the full paths required for selecting a job
  std::vector<String> _fileListPaths;
  std::vector<bool> _fileListIsDirectory;
  size_t _fileListSelected = 0;
  size_t _fileListOffset = 0; // starting index shown on screen
  String _currentSdDirectory = "/";
  // Confirm dialog state: 0 = Yes, 1 = No
  size_t _confirmSelectedIndex = 0;

  enum class PendingAction {
    None,
    HomeAll,
    SetOrigin,
    RepeatJob,
    StartJob,
    SpindleToggle,
    ToggleWifi,
    ToggleMqtt,
    ChangeBroker,
    ResetWifi
  };
  PendingAction _pendingAction = PendingAction::None;
  UiState _confirmReturnState = UiState::Menu;
  UiState _infoReturnState = UiState::Menu;
  String _confirmTitle;
  String _confirmMessage;

  struct MenuState {
    String title;
    std::vector<String> items;
    size_t selected;
  };

  // Menu history stack: stores previous menu title, items, and selected index.
  std::vector<MenuState> _menuHistory;

  // Helper to push/pop menus
  void pushMenuState(const std::vector<String> &items, size_t selected);
  bool popMenuState();
  void showStandbyScreen();
  void showMainMenuScreen(size_t selected = 0);
  void showSubMenu(const char *title, const std::vector<String> &items);
  void showMachineStatusScreen();
  std::vector<String> machineStatusItems() const;
  void handleMachineStatusSelect();
  void showNetworkStatusScreen();
  std::vector<String> networkStatusItems();
  void handleNetworkStatusSelect();
  void showAboutScreen();
  void updateAboutScreen();
  void exitAboutScreen();
  void loadNetworkSettings();
  void saveNetworkSettings();
  void startNetworkFromSettings(bool tryStoredCredentials = true);
  void stopNetworkRuntime();
  void setWifiEnabled(bool enabled);
  void setMqttEnabled(bool enabled);
  void openMqttBrokerEditor();
  void showMqttBrokerEditor();
  void requestMqttBrokerEditorRedraw(bool immediate = false);
  void updateMqttBrokerEditorRedraw();
  void moveMqttBrokerEditorRight();
  void moveMqttBrokerEditorLeft();
  void changeMqttBrokerEditorDigit(int8_t direction);
  String mqttBrokerEditorValue() const;
  bool mqttBrokerEditorValueValid() const;
  void applyMqttBrokerEditorValue(UiState returnState);
  bool connectStoredWifiWithFeedback();
  void applyMqttSettings();
  void resetMqttSettingsToDefaults();
  void logNetworkSettings(const char *source) const;
  WifiPortalMqttSettings currentMqttPortalSettings() const;
  void updateMqttSettingsFromPortal(const WifiPortalMqttSettings &settings);
  String mqttBrokerInfo() const;
  void refreshMachineStatus();
  bool isMarlinResponding() const;
  MarlinConnectionState marlinConnectionState() const;
  bool canSendMarlinCommand() const;
  void showCncUnavailable(UiState returnState);
  String currentUiStateName() const;
  String softEndstopStatusText() const;
  bool isSystemAlarmActive() const;
  String alarmStatusText() const;
  String alarmMessageText() const;
  String marlinStatusText() const;
  String machineActivityText() const;
  String standbyMarlinStatusText() const;
  String standbyNetworkStatusText();
  void refreshStandbyNetworkStatus();
  void publishMqttMonitoring();
  void publishSdJobFileList();
  void handleMqttFileSelectRequest();
  void handleMqttSdStatusRequest(bool force = false);
  void handleMqttFileDeleteRequest();
  bool isSdPathInUse(const String &path) const;
  bool isPendingLocalSelectionPath(const String &path) const;
  void handleSdCardStateChanged();
  void handleMqttFileTransferRequests();
  void handleMqttFileStartRequest();
  void handleMqttFileChunkRequest();
  void handleMqttFileFinishRequest();
  void abortMqttUpload(const char *reason);
  void clearMqttUploadState(bool keepFinishedAck = false);
  bool toggleSpindle();
  bool applyMachineFeedrate();
  void requestMachineFeedrateStatus(bool force = false);
  void parseMachineFeedrateStatus(const String &line);
  void requestMachineGeometryStatus(bool force = false);
  void parseMachineGeometryStatus(const String &line);
  void requestHomeSensorStatus(bool force = false);
  void parseHomeSensorStatus(const String &line);
  void requestSoftEndstopStatus(bool force = false);
  void parseSoftEndstopStatus(const String &line);
  void prefetchMachineStatus();
  bool loadSdFileList(const String &directory);
  void openSdFileList(const String &directory = "/");
  void showCurrentFileListPage();
  void enterSelectedFileListItem();
  void showInfoScreen(
    const char *title,
    const char *message,
    UiState returnState = UiState::Menu,
    unsigned long autoCloseMs = 0
  );
  void dismissInfoScreen();
  void showActionConfirm(PendingAction action, const char *title, const char *message);
  void redrawActionConfirm();
  void cancelActionConfirm();
  void executeConfirmedAction();
  void repeatSelectedJob(UiState returnState);
  bool startSelectedJob(bool returnToOrigin, UiState returnState);
  void updateJobControl();
  void updateStandbyJobButtons();
  void showJobControlMessage(const char *message);
  bool isConfirmationState() const;
  void acceptConfirmation();
  void rejectConfirmation();
  void executeSelectedConfirmation();
  void redrawCurrentConfirmation();

    // Modul jaringan dan cloud.
    WifiPortal _wifi;
    CloudMqtt _cloud;
    TimeManager _time;

    // Status runtime aplikasi.
    unsigned long _lastMqttMonitorPublish = 0;
    unsigned long _infoShownAt = 0;
    unsigned long _infoAutoCloseMs = 0;
    unsigned long _aboutLastPageChange = 0;
    uint8_t _aboutPageIndex = 0;
    bool _inputStarted = false;
    bool _displayEnabled = true;
    bool _storageStarted = false;
    bool _networkStarted = false;
    bool _mqttStarted = false;
    bool _lastMqttConnected = false;
    bool _standbyScreenDrawn = false;
    bool _wifiEnabled = AppConfig::DEFAULT_WIFI_ENABLED;
    bool _mqttEnabled = MqttConfig::DEFAULT_ENABLED;
    String _mqttBroker = MqttConfig::BROKER;
    uint8_t _mqttBrokerEditorDigits[12] = {0};
    uint8_t _mqttBrokerEditorIndex = 0;
    bool _mqttBrokerEditorDirty = false;
    unsigned long _lastMqttBrokerEditorDraw = 0;
    String _pendingMqttBroker;
    uint16_t _mqttPort = MqttConfig::PORT;
    String _mqttTopicPrefix = MqttConfig::TOPIC_PREFIX;
    String _mqttUser = MqttConfig::USER;
    String _mqttPassword = MqttConfig::PASSWORD;
    String _mqttSelectedJobFile;
    size_t _mqttSelectedJobFileSize = 0;
    String _mqttUploadFileName;
    String _mqttUploadFinalPath;
    String _mqttUploadTemporaryPath;
    String _mqttUploadBackupPath;
    String _mqttUploadLastSavedFileName;
    String _mqttUploadLastSavedPath;
    size_t _mqttUploadExpectedSize = 0;
    size_t _mqttUploadChunkSize = 0;
    size_t _mqttUploadTotalChunks = 0;
    size_t _mqttUploadExpectedChunkIndex = 0;
    size_t _mqttUploadBytesWritten = 0;
    size_t _mqttUploadLastSavedSize = 0;
    size_t _mqttUploadLastSavedChunks = 0;
    int _mqttUploadLastAckedChunk = -1;
    bool _mqttUploadActive = false;
    bool _mqttUploadFinished = false;
    bool _mqttUploadOverwrite = false;
    bool _lastSdReady = false;
    bool _sdStatusPublishPending = false;
    String _lastStandbyNetworkStatus;
    bool _spindleOn = false;
    bool _machineFeedrateEditing = false;
    bool _machineFeedrateLoaded = false;
    bool _machineWorkAreaEditing = false;
    bool _machineWorkAreaLoaded = false;
    bool _softEndstopLoaded = false;
    bool _softEndstopEnabled = false;
    bool _marlinConnectionEnabled = false;
    bool _marlinEverResponded = false;
    bool _marlinErrorActive = false;
    bool _positionValid = false;
    size_t _machineStatusSelected = 0;
    size_t _machineStatusOffset = 0;
    size_t _networkStatusSelected = 0;
    size_t _networkStatusOffset = 0;
    unsigned int _machineFeedrateXY = 0;
    unsigned int _machineFeedrateZ = 0;
    unsigned int _machineWorkAreaX = 0;
    unsigned int _machineWorkAreaY = 0;
    unsigned int _machineWorkAreaZ = 0;
    bool _machineWorkAreaFromFallback = false;
    String _homeXStatus = "?";
    String _homeYStatus = "?";
    String _homeZStatus = "?";
  // Helper to compute ETA string for selected job (HH:MM:SS).
  String computeEta() const;
  // Return selected job filename or empty string if none
  String computeJobName() const;
  // Return progress percent (0-100) or -1 if no active job
  int computeProgress() const;
  // Return current local time for LCD as HH:MM or empty if not available.
  String getLocalTimeString() const;

  private:
    unsigned long _lastM114Request = 0;
    unsigned long _lastM119Request = 0;
    unsigned long _lastM203Request = 0;
    unsigned long _lastM115Request = 0;
    unsigned long _lastM211Request = 0;
    unsigned long _lastMarlinResponseMs = 0;
    unsigned long _marlinMonitorStartedAt = 0;
    unsigned long _lastMarlinStatusPoll = 0;
    unsigned long _lastStatusScreenRefresh = 0;
    unsigned long _lastJobScreenRefresh = 0;
    uint8_t _machineStatusPrefetchCursor = 0;
    void updateMarlinCommunication();
    void parseMarlinResponse(const String &line);

  private:
    uint8_t _jogAxisIndex = 0;      // 0:X, 1:Y, 2:Z
    bool _isJogAdjusting = false;   // Toggle antara pilih axis atau gerakkan nilai
    bool _encoderMovedWhilePressed = false;

  private:
    uint8_t _activeJogPin = 0;
    uint32_t _jogStartTime = 0;
    bool _jogLongPressHandled = false;
    uint32_t _lastJogRepeatTick = 0;
    uint8_t _activeStandbyJobPin = 0;
    uint32_t _standbyJobPressStartedAt = 0;
    bool _standbyJobLongPressHandled = false;
    bool _homeCycleIssued = false;
    bool _originConfigured = false;

    void enterSetOrigin();
    void confirmSetOrigin(UiState returnState);
    void handleJog(uint8_t pin, float distance);
    void updateJogDisplay();
};

#endif
