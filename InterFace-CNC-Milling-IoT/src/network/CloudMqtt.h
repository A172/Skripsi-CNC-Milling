#ifndef CLOUD_MQTT_H
#define CLOUD_MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <vector>
#include "../storage/SdCardReader.h"

struct MqttMonitoringSnapshot {
  String marlinStatus;
  String timeStatus;
  String time;
  String date;
  String ssid;
  String ipAddress;
  String mqttLink;
  String homeX;
  String homeY;
  String homeZ;
  String softEndstop;
  String jobFile = "Belum dipilih";
  String jobState = "IDLE";
  String machineActivity = "OFF";
  String jobError;
  String positionSource = "unavailable";
  float posX = 0.0f;
  float posY = 0.0f;
  float posZ = 0.0f;
  unsigned int feedrateXY = 0;
  unsigned int feedrateZ = 0;
  unsigned int workAreaX = 0;
  unsigned int workAreaY = 0;
  unsigned int workAreaZ = 0;
  int progress = -1;
  bool sdReady = false;
  bool spindleOn = false;
  bool feedrateValid = false;
  bool workAreaValid = false;
  bool positionValid = false;
  String workAreaSource = "unknown";
};

struct MqttFileSelectRequest {
  bool validPayload = false;
  String reason;
  String path;
};

struct MqttFileStartRequest {
  bool validPayload = false;
  String reason;
  String fileName;
  size_t fileSize = 0;
  size_t chunkSize = 0;
  size_t totalChunks = 0;
  bool overwrite = false;
};

struct MqttFileChunkRequest {
  bool validPayload = false;
  String reason;
  int chunkIndex = -1;
  std::vector<uint8_t> data;
};

struct MqttFileDeleteRequest {
  bool validPayload = false;
  String reason;
  String filePath;
};

class CloudMqtt {
  public:
    bool begin();
    void configure(
      const String &broker,
      uint16_t port,
      const String &topicPrefix,
      const String &user,
      const String &password
    );
    void update();
    bool isConnected();
    int state();
    String statusText();
    void disconnect();
    String broker() const;
    uint16_t port() const;
    String topicPrefix() const;

    void publishMonitoring(const MqttMonitoringSnapshot &snapshot);
    void handleMessage(char *topic, byte *payload, unsigned int length);
    bool consumeFileListRequest();
    void publishFileListError(const char *status);
    void publishFileListBegin(size_t count);
    void publishFileListItem(size_t index, const SdCardEntry &entry);
    void publishFileListEnd(size_t count);
    bool consumeFileSelectRequest(MqttFileSelectRequest &request);
    void publishFileSelectOk(const String &label, const String &path, size_t size);
    void publishFileSelectError(const char *reason, const String &path);
    bool consumeSdStatusRequest();
    void publishSdStatus(const char *status, bool mounted, size_t fileCount, const char *reason = nullptr);
    bool consumeFileDeleteRequest(MqttFileDeleteRequest &request);
    void publishFileDeleteDeleted(const String &filePath);
    void publishFileDeleteError(const char *reason, const String &filePath);
    bool consumeFileStartRequest(MqttFileStartRequest &request);
    bool consumeFileChunkRequest(MqttFileChunkRequest &request);
    bool consumeFileFinishRequest();
    void publishFileStartReady(const String &fileName, size_t fileSize, size_t totalChunks);
    void publishFileStartError(const char *reason, const String &fileName, const String &path = String());
    void publishFileChunkReceived(int chunkIndex);
    void publishFileChunkError(const char *reason, int chunkIndex, const String &fileName);
    void publishFileFinishSaved(const String &fileName, const String &path, size_t size, size_t totalChunks);
    void publishFileFinishError(const char *reason, const String &fileName);

  private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    String _clientId;
    String _baseTopic;
    String _broker;
    String _topicPrefix;
    String _user;
    String _password;
    uint16_t _port = 0;
    int _lastState = MQTT_DISCONNECTED;
    unsigned long _lastReconnectAttempt = 0;
    unsigned long _lastPositionPublish = 0;
    unsigned long _lastMachinePublish = 0;
    unsigned long _lastProgressPublish = 0;
    unsigned long _lastTimePublish = 0;
    bool _networkPublished = false;
    String _lastProgressPayload;
    String _lastMachinePayload;
    String _lastNetworkPayload;
    String _lastTimePayload;
    String _lastAlarmPayload;
    String _lastErrorPayload;
    bool _fileListRequested = false;
    bool _fileSelectRequested = false;
    bool _sdStatusRequested = false;
    bool _fileDeleteRequested = false;
    bool _fileStartRequested = false;
    bool _fileChunkRequested = false;
    bool _fileFinishRequested = false;
    MqttFileSelectRequest _pendingFileSelectRequest;
    MqttFileDeleteRequest _pendingFileDeleteRequest;
    MqttFileStartRequest _pendingFileStartRequest;
    MqttFileChunkRequest _pendingFileChunkRequest;

    void ensureConfigured();
    bool brokerReachable();
    bool connect();
    bool subscribeInboundTopics();
    String topicName(const char *suffix) const;
    bool publishJson(const String &topicSuffix, const String &payload, bool retained = false);
    void publishConnectionStatus(bool connected, const char *state);
    void publishNetworkLink(const char *mqttLink);
    void publishNetwork(const MqttMonitoringSnapshot &snapshot, bool force = false);
    void publishPosition(const MqttMonitoringSnapshot &snapshot, unsigned long now);
    void publishMachine(const MqttMonitoringSnapshot &snapshot, unsigned long now);
    void publishProgress(const MqttMonitoringSnapshot &snapshot, unsigned long now);
    void publishTime(const MqttMonitoringSnapshot &snapshot, unsigned long now);
    void publishAlarm(const MqttMonitoringSnapshot &snapshot);
    void publishError(const MqttMonitoringSnapshot &snapshot);
    bool publishFileAck(const String &payload);
    void handleFileStart(const String &message);
    void handleFileChunk(const String &message);
    void handleFileFinish(const String &message);
    void handleFileListRequest(const String &message);
    void handleFileSelectRequest(const String &message);
    void handleSdStatusRequest(const String &message);
    void handleFileDeleteRequest(const String &message);
};

#endif
