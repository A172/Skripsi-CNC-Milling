#include "CloudMqtt.h"

#include <WiFi.h>
#include "../config/MqttConfig.h"

namespace {
  CloudMqtt *activeMqtt = nullptr;

  String escapeJson(const String &value) {
    String escaped;
    escaped.reserve(value.length());

    for (size_t i = 0; i < value.length(); i++) {
      char current = value.charAt(i);

      if (current == '"' || current == '\\') {
        escaped += '\\';
      }

      if (current == '\n') {
        escaped += "\\n";
      } else if (current == '\r') {
        escaped += "\\r";
      } else {
        escaped += current;
      }
    }

    return escaped;
  }

  String macSuffix() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();
    return mac;
  }

  String floatJson(float value) {
    return String(value, 2);
  }

  String statusOrUnknown(const String &value) {
    return value.length() == 0 || value == "?" ? String("UNKNOWN") : value;
  }

  String homeStatusForMqtt(const String &value) {
    String status = statusOrUnknown(value);
    if (status == "TRIGGER") {
      return String("TRIGGERED");
    }
    return status;
  }

  bool homeStatusValid(const String &status) {
    return status == "OPEN" || status == "TRIGGERED";
  }

  String homeStatusNote(bool connected, const String &status) {
    if (!connected) {
      return String("not_connected");
    }
    if (homeStatusValid(status)) {
      return String("ok");
    }
    return String("not_loaded");
  }

  String homeUnknownAxes(const String &homeX, const String &homeY, const String &homeZ) {
    String axes;
    if (!homeStatusValid(homeX)) {
      axes += "X";
    }
    if (!homeStatusValid(homeY)) {
      if (axes.length() > 0) axes += ",";
      axes += "Y";
    }
    if (!homeStatusValid(homeZ)) {
      if (axes.length() > 0) axes += ",";
      axes += "Z";
    }
    return axes;
  }

  String labelFromPath(const String &path) {
    if (path.startsWith("/") && path.length() > 1) {
      return path.substring(1);
    }

    return path;
  }

  bool parseJsonIntField(const String &payload, const char *fieldName, int &value) {
    String key = String("\"") + fieldName + "\"";
    int keyIndex = payload.indexOf(key);
    if (keyIndex < 0) {
      return false;
    }

    int colonIndex = payload.indexOf(':', keyIndex + key.length());
    if (colonIndex < 0) {
      return false;
    }

    int valueStart = colonIndex + 1;
    while (valueStart < (int)payload.length() && isspace(payload[valueStart])) {
      valueStart++;
    }

    int valueEnd = valueStart;
    if (valueEnd < (int)payload.length() && payload[valueEnd] == '-') {
      valueEnd++;
    }

    while (valueEnd < (int)payload.length() && isDigit(payload[valueEnd])) {
      valueEnd++;
    }

    if (valueEnd == valueStart ||
        (valueEnd == valueStart + 1 && payload[valueStart] == '-')) {
      return false;
    }

    value = payload.substring(valueStart, valueEnd).toInt();
    return true;
  }

  bool parseJsonSizeField(const String &payload, const char *fieldName, size_t &value) {
    int parsed = -1;
    if (!parseJsonIntField(payload, fieldName, parsed) || parsed < 0) {
      return false;
    }

    value = static_cast<size_t>(parsed);
    return true;
  }

  bool parseJsonBoolField(const String &payload, const char *fieldName, bool &value) {
    String key = String("\"") + fieldName + "\"";
    int keyIndex = payload.indexOf(key);
    if (keyIndex < 0) {
      return false;
    }

    int colonIndex = payload.indexOf(':', keyIndex + key.length());
    if (colonIndex < 0) {
      return false;
    }

    int valueStart = colonIndex + 1;
    while (valueStart < (int)payload.length() && isspace(payload[valueStart])) {
      valueStart++;
    }

    if (payload.substring(valueStart, valueStart + 4) == "true") {
      value = true;
      return true;
    }

    if (payload.substring(valueStart, valueStart + 5) == "false") {
      value = false;
      return true;
    }

    return false;
  }

  bool parseJsonStringField(const String &payload, const char *fieldName, String &value) {
    String key = String("\"") + fieldName + "\"";
    int keyIndex = payload.indexOf(key);
    if (keyIndex < 0) {
      return false;
    }

    int colonIndex = payload.indexOf(':', keyIndex + key.length());
    if (colonIndex < 0) {
      return false;
    }

    int valueStart = colonIndex + 1;
    while (valueStart < (int)payload.length() && isspace(payload[valueStart])) {
      valueStart++;
    }

    if (valueStart >= (int)payload.length() || payload[valueStart] != '"') {
      return false;
    }

    value = "";
    bool escaping = false;
    for (int i = valueStart + 1; i < (int)payload.length(); i++) {
      char current = payload[i];
      if (escaping) {
        if (current == 'n') {
          value += '\n';
        } else if (current == 'r') {
          value += '\r';
        } else if (current == 't') {
          value += '\t';
        } else {
          value += current;
        }
        escaping = false;
        continue;
      }

      if (current == '\\') {
        escaping = true;
        continue;
      }

      if (current == '"') {
        return true;
      }

      value += current;
    }

    return false;
  }

  void stringToBytes(const String &value, std::vector<uint8_t> &bytes) {
    bytes.clear();
    bytes.reserve(value.length());
    for (size_t i = 0; i < value.length(); i++) {
      bytes.push_back(static_cast<uint8_t>(value.charAt(i)));
    }
  }

  void mqttCallback(char *topic, byte *payload, unsigned int length) {
    if (activeMqtt != nullptr) {
      activeMqtt->handleMessage(topic, payload, length);
    }
  }
}

bool CloudMqtt::begin() {
  ensureConfigured();
  _clientId = String(MqttConfig::CLIENT_ID) + "-" + macSuffix();
  _baseTopic = _topicPrefix;

  _mqttClient.setClient(_wifiClient);
  _mqttClient.setServer(_broker.c_str(), _port);
  _mqttClient.setCallback(mqttCallback);
  _mqttClient.setBufferSize(2048);
  _mqttClient.setSocketTimeout(MqttConfig::SOCKET_TIMEOUT_SEC);
  activeMqtt = this;

  return connect();
}

void CloudMqtt::configure(
  const String &broker,
  uint16_t port,
  const String &topicPrefix,
  const String &user,
  const String &password
) {
  _broker = broker;
  _broker.trim();
  if (_broker.length() == 0) {
    _broker = MqttConfig::BROKER;
  }

  _port = port == 0 ? MqttConfig::PORT : port;

  _topicPrefix = topicPrefix;
  _topicPrefix.trim();
  if (_topicPrefix.length() == 0) {
    _topicPrefix = MqttConfig::TOPIC_PREFIX;
  }
  while (_topicPrefix.endsWith("/")) {
    _topicPrefix.remove(_topicPrefix.length() - 1);
  }

  _user = user;
  _user.trim();
  _password = password;
}

void CloudMqtt::update() {
  if (_mqttClient.connected()) {
    _lastState = MQTT_CONNECTED;
    _mqttClient.loop();
    return;
  }

  if (_lastState == MQTT_CONNECTED) {
    _lastState = _mqttClient.state();
  }

  if (millis() - _lastReconnectAttempt < MqttConfig::RECONNECT_INTERVAL_MS) {
    return;
  }

  _lastReconnectAttempt = millis();
  connect();
}

bool CloudMqtt::isConnected() {
  return _mqttClient.connected();
}

int CloudMqtt::state() {
  return _lastState;
}

String CloudMqtt::statusText() {
  if (_mqttClient.connected()) {
    _lastState = MQTT_CONNECTED;
    return String("CONNECTED");
  }

  if (WiFi.status() != WL_CONNECTED) {
    return String("NO WIFI");
  }

  int currentState = state();
  switch (currentState) {
    case MQTT_CONNECTION_TIMEOUT:
      return String("TIMEOUT");
    case MQTT_CONNECTION_LOST:
      return String("LOST");
    case MQTT_CONNECT_FAILED:
      return String("CONN FAIL");
    case MQTT_DISCONNECTED:
      return String("DISCONNECTED");
    case MQTT_CONNECT_BAD_PROTOCOL:
      return String("BAD PROTO");
    case MQTT_CONNECT_BAD_CLIENT_ID:
      return String("BAD ID");
    case MQTT_CONNECT_UNAVAILABLE:
      return String("BROKER OFF");
    case MQTT_CONNECT_BAD_CREDENTIALS:
      return String("BAD LOGIN");
    case MQTT_CONNECT_UNAUTHORIZED:
      return String("DENIED");
    default:
      return String("WAIT ") + String(currentState);
  }
}

String CloudMqtt::broker() const {
  return _broker.length() ? _broker : String(MqttConfig::BROKER);
}

uint16_t CloudMqtt::port() const {
  return _port == 0 ? MqttConfig::PORT : _port;
}

String CloudMqtt::topicPrefix() const {
  return _topicPrefix.length() ? _topicPrefix : String(MqttConfig::TOPIC_PREFIX);
}

void CloudMqtt::disconnect() {
  if (!_mqttClient.connected()) {
    return;
  }

  publishNetworkLink("DISCONNECTED");
  publishConnectionStatus(false, "OFFLINE");
  _mqttClient.disconnect();
  _lastState = MQTT_DISCONNECTED;
}

void CloudMqtt::publishMonitoring(const MqttMonitoringSnapshot &snapshot) {
  if (!_mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();

  publishNetwork(snapshot, !_networkPublished);
  publishMachine(snapshot, now);
  publishPosition(snapshot, now);
  publishProgress(snapshot, now);
  publishTime(snapshot, now);
  publishAlarm(snapshot);
  publishError(snapshot);
}

bool CloudMqtt::connect() {
  ensureConfigured();

  if (WiFi.status() != WL_CONNECTED) {
    _lastState = MQTT_CONNECT_FAILED;
    return false;
  }

  if (!brokerReachable()) {
    return false;
  }

  String statusTopic = topicName(MqttConfig::TOPIC_STATUS);
  const char *offlinePayload = "{\"connected\":false,\"state\":\"OFFLINE\"}";

  Serial.print("Menghubungkan MQTT ke ");
  Serial.print(_broker);
  Serial.print(":");
  Serial.println(_port);

  bool connected = false;
  if (_user.length() > 0) {
    connected = _mqttClient.connect(
      _clientId.c_str(),
      _user.c_str(),
      _password.c_str(),
      statusTopic.c_str(),
      1,
      true,
      offlinePayload
    );
  } else {
    connected = _mqttClient.connect(
      _clientId.c_str(),
      statusTopic.c_str(),
      1,
      true,
      offlinePayload
    );
  }

  if (!connected) {
    _lastState = _mqttClient.state();
    Serial.print("MQTT gagal, state: ");
    Serial.println(_lastState);
    return false;
  }

  _lastState = MQTT_CONNECTED;
  _networkPublished = false;
  _lastPositionPublish = 0;
  _lastMachinePublish = 0;
  _lastProgressPublish = 0;
  _lastTimePublish = 0;
  _lastProgressPayload = "";
  _lastMachinePayload = "";
  _lastNetworkPayload = "";
  _lastTimePayload = "";
  _lastAlarmPayload = "";
  _lastErrorPayload = "";

  subscribeInboundTopics();
  publishConnectionStatus(true, "ONLINE");

  Serial.print("MQTT terhubung. Base topic: ");
  Serial.println(_baseTopic);
  return true;
}

bool CloudMqtt::subscribeInboundTopics() {
  String commandTopic = topicName(MqttConfig::TOPIC_COMMAND);
  String gcodeTopic = topicName(MqttConfig::TOPIC_GCODE);
  String fileStartTopic = topicName(MqttConfig::TOPIC_FILE_START);
  String fileChunkTopic = topicName(MqttConfig::TOPIC_FILE_CHUNK);
  String fileFinishTopic = topicName(MqttConfig::TOPIC_FILE_FINISH);
  String fileListRequestTopic = topicName(MqttConfig::TOPIC_FILE_LIST_REQUEST);
  String fileSelectTopic = topicName(MqttConfig::TOPIC_FILE_SELECT);
  String fileDeleteTopic = topicName(MqttConfig::TOPIC_FILE_DELETE);
  String sdStatusRequestTopic = topicName(MqttConfig::TOPIC_SD_STATUS_REQUEST);
  bool commandReady = _mqttClient.subscribe(commandTopic.c_str(), 1);
  bool gcodeReady = _mqttClient.subscribe(gcodeTopic.c_str(), 1);
  bool fileStartReady = _mqttClient.subscribe(fileStartTopic.c_str(), 1);
  bool fileChunkReady = _mqttClient.subscribe(fileChunkTopic.c_str(), 1);
  bool fileFinishReady = _mqttClient.subscribe(fileFinishTopic.c_str(), 1);
  bool fileListRequestReady = _mqttClient.subscribe(fileListRequestTopic.c_str(), 1);
  bool fileSelectReady = _mqttClient.subscribe(fileSelectTopic.c_str(), 1);
  bool fileDeleteReady = _mqttClient.subscribe(fileDeleteTopic.c_str(), 1);
  bool sdStatusRequestReady = _mqttClient.subscribe(sdStatusRequestTopic.c_str(), 1);

  Serial.print("MQTT subscribe ");
  Serial.print(commandTopic);
  Serial.println(commandReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(gcodeTopic);
  Serial.println(gcodeReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(fileStartTopic);
  Serial.println(fileStartReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(fileChunkTopic);
  Serial.println(fileChunkReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(fileFinishTopic);
  Serial.println(fileFinishReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(fileListRequestTopic);
  Serial.println(fileListRequestReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(fileSelectTopic);
  Serial.println(fileSelectReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(fileDeleteTopic);
  Serial.println(fileDeleteReady ? " OK" : " GAGAL");
  Serial.print("MQTT subscribe ");
  Serial.print(sdStatusRequestTopic);
  Serial.println(sdStatusRequestReady ? " OK" : " GAGAL");

  return commandReady &&
         gcodeReady &&
         fileStartReady &&
         fileChunkReady &&
         fileFinishReady &&
         fileListRequestReady &&
         fileSelectReady &&
         fileDeleteReady &&
         sdStatusRequestReady;
}

String CloudMqtt::topicName(const char *suffix) const {
  return _baseTopic + "/" + suffix;
}

bool CloudMqtt::publishJson(const String &topicSuffix, const String &payload, bool retained) {
  if (!_mqttClient.connected()) {
    return false;
  }

  String topic = topicName(topicSuffix.c_str());
  return _mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

void CloudMqtt::publishConnectionStatus(bool connected, const char *state) {
  String payload = String("{\"connected\":") + (connected ? "true" : "false") +
                   ",\"state\":\"" + escapeJson(state) + "\"}";
  publishJson(MqttConfig::TOPIC_STATUS, payload, true);
}

void CloudMqtt::publishNetworkLink(const char *mqttLink) {
  String ssid = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("-");
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("-");
  String payload = String("{\"ssid\":\"") + escapeJson(ssid) +
                   "\",\"ip\":\"" + escapeJson(ip) +
                   "\",\"mqtt\":\"" + escapeJson(mqttLink) +
                   "\"}";

  publishJson(MqttConfig::TOPIC_NETWORK, payload, true);
  _lastNetworkPayload = payload;
  _networkPublished = true;
}

void CloudMqtt::publishNetwork(const MqttMonitoringSnapshot &snapshot, bool force) {
  String payload = String("{\"ssid\":\"") + escapeJson(snapshot.ssid) +
                   "\",\"ip\":\"" + escapeJson(snapshot.ipAddress) +
                   "\",\"mqtt\":\"" + escapeJson(snapshot.mqttLink) +
                   "\"}";

  if (!force && payload == _lastNetworkPayload) {
    return;
  }

  publishJson(MqttConfig::TOPIC_NETWORK, payload, true);
  _lastNetworkPayload = payload;
  _networkPublished = true;
}

void CloudMqtt::publishPosition(const MqttMonitoringSnapshot &snapshot, unsigned long now) {
  if (now - _lastPositionPublish < MqttConfig::POSITION_INTERVAL_MS) {
    return;
  }

  String payload;
  if (snapshot.positionValid) {
    String x = floatJson(snapshot.posX);
    String y = floatJson(snapshot.posY);
    String z = floatJson(snapshot.posZ);
    payload = String("{\"x\":") + x +
              ",\"y\":" + y +
              ",\"z\":" + z +
              ",\"display\":{\"x\":\"" + x +
              "\",\"y\":\"" + y +
              "\",\"z\":\"" + z +
              "\"},\"unit\":\"mm\",\"valid\":true,\"source\":\"" +
              escapeJson(snapshot.positionSource) + "\"}";
  } else {
    payload = String("{\"x\":null,\"y\":null,\"z\":null,") +
              "\"display\":{\"x\":\"?\",\"y\":\"?\",\"z\":\"?\"}," +
              "\"unit\":\"mm\",\"valid\":false,\"source\":\"" +
              escapeJson(snapshot.positionSource) + "\"}";
  }

  publishJson(MqttConfig::TOPIC_POSITION, payload, true);
  _lastPositionPublish = now;
}

void CloudMqtt::publishMachine(const MqttMonitoringSnapshot &snapshot, unsigned long now) {
  bool connected = snapshot.marlinStatus == "CONNECTED";
  bool feedrateValid = connected && snapshot.feedrateValid;
  String spindle = connected ? (snapshot.spindleOn ? "ON" : "OFF") : "UNKNOWN";
  String softEndstop = connected
    ? statusOrUnknown(snapshot.softEndstop)
    : String("UNKNOWN");
  String homeX = connected ? homeStatusForMqtt(snapshot.homeX) : String("UNKNOWN");
  String homeY = connected ? homeStatusForMqtt(snapshot.homeY) : String("UNKNOWN");
  String homeZ = connected ? homeStatusForMqtt(snapshot.homeZ) : String("UNKNOWN");
  bool homeXValid = homeStatusValid(homeX);
  bool homeYValid = homeStatusValid(homeY);
  bool homeZValid = homeStatusValid(homeZ);
  String homeXNote = homeStatusNote(connected, homeX);
  String homeYNote = homeStatusNote(connected, homeY);
  String homeZNote = homeStatusNote(connected, homeZ);
  String feedXY = feedrateValid ? String(snapshot.feedrateXY) : String("null");
  String feedZ = feedrateValid ? String(snapshot.feedrateZ) : String("null");
  bool workAreaValid = snapshot.workAreaValid;
  String areaX = workAreaValid ? String(snapshot.workAreaX) : String("null");
  String areaY = workAreaValid ? String(snapshot.workAreaY) : String("null");
  String areaZ = workAreaValid ? String(snapshot.workAreaZ) : String("null");
  String areaSource = workAreaValid ? snapshot.workAreaSource : String("unknown");
  String activity = snapshot.machineActivity.length() > 0
    ? snapshot.machineActivity
    : String("OFF");
  String payload = String("{\"state\":\"") + escapeJson(snapshot.marlinStatus) +
                   "\",\"connected\":" + (connected ? "true" : "false") +
                   ",\"activity\":\"" + escapeJson(activity) +
                   "\",\"spindle\":\"" + escapeJson(spindle) +
                   "\",\"soft_endstop\":\"" + escapeJson(softEndstop) +
                   "\",\"feed_xy\":" + feedXY +
                   ",\"feed_z\":" + feedZ +
                   ",\"area_x\":" + areaX +
                   ",\"area_y\":" + areaY +
                   ",\"area_z\":" + areaZ +
                   ",\"area_source\":\"" + escapeJson(areaSource) +
                   "\",\"work_origin\":\"" + escapeJson(snapshot.workOrigin) +
                   "\",\"work_origin_valid\":" + (snapshot.workOriginValid ? "true" : "false") +
                   ",\"x_home\":\"" + escapeJson(homeX) +
                   "\",\"y_home\":\"" + escapeJson(homeY) +
                   "\",\"z_home\":\"" + escapeJson(homeZ) +
                   "\",\"work_area\":{\"x\":" + areaX +
                   ",\"y\":" + areaY +
                   ",\"z\":" + areaZ +
                   ",\"valid\":" + (workAreaValid ? "true" : "false") +
                   ",\"source\":\"" + escapeJson(areaSource) +
                   "\"},\"home\":{\"x\":\"" + escapeJson(homeX) +
                   "\",\"y\":\"" + escapeJson(homeY) +
                   "\",\"z\":\"" + escapeJson(homeZ) +
                   "\"},\"home_valid\":{\"x\":" + (homeXValid ? "true" : "false") +
                   ",\"y\":" + (homeYValid ? "true" : "false") +
                   ",\"z\":" + (homeZValid ? "true" : "false") +
                   "},\"home_note\":{\"x\":\"" + escapeJson(homeXNote) +
                   "\",\"y\":\"" + escapeJson(homeYNote) +
                   "\",\"z\":\"" + escapeJson(homeZNote) + "\"}}";

  if (payload == _lastMachinePayload &&
      now - _lastMachinePublish < MqttConfig::MACHINE_INTERVAL_MS) {
    return;
  }

  publishJson(MqttConfig::TOPIC_MACHINE, payload, true);
  _lastMachinePayload = payload;
  _lastMachinePublish = now;
}

void CloudMqtt::publishProgress(const MqttMonitoringSnapshot &snapshot, unsigned long now) {
  String fileName = snapshot.jobFile.length() > 0
    ? snapshot.jobFile
    : String("Belum dipilih");
  int progress = snapshot.progress < 0
    ? 0
    : constrain(snapshot.progress, 0, 100);
  String state = snapshot.jobState.length() > 0
    ? snapshot.jobState
    : String("IDLE");

  String payload = String("{\"file\":\"") + escapeJson(fileName) +
                   "\",\"progress\":" + String(progress) +
                   ",\"state\":\"" + escapeJson(state) + "\"}";

  if (payload == _lastProgressPayload &&
      now - _lastProgressPublish < MqttConfig::PROGRESS_INTERVAL_MS) {
    return;
  }

  publishJson(MqttConfig::TOPIC_PROGRESS, payload, true);
  _lastProgressPayload = payload;
  _lastProgressPublish = now;
}

void CloudMqtt::publishTime(const MqttMonitoringSnapshot &snapshot, unsigned long now) {
  String payload = String("{\"sync\":\"") + escapeJson(snapshot.timeStatus) +
                   "\",\"time\":\"" + escapeJson(snapshot.time) +
                   "\",\"date\":\"" + escapeJson(snapshot.date) +
                   "\"}";

  if (payload == _lastTimePayload &&
      now - _lastTimePublish < MqttConfig::TIME_INTERVAL_MS) {
    return;
  }

  publishJson(MqttConfig::TOPIC_TIME, payload, true);
  _lastTimePayload = payload;
  _lastTimePublish = now;
}

void CloudMqtt::publishAlarm(const MqttMonitoringSnapshot &snapshot) {
  bool connected = snapshot.marlinStatus == "CONNECTED";
  String homeX = connected ? homeStatusForMqtt(snapshot.homeX) : String("UNKNOWN");
  String homeY = connected ? homeStatusForMqtt(snapshot.homeY) : String("UNKNOWN");
  String homeZ = connected ? homeStatusForMqtt(snapshot.homeZ) : String("UNKNOWN");
  String unknownHomeAxes = connected ? homeUnknownAxes(homeX, homeY, homeZ) : String();
  String level = "INFO";
  String message = "OK";
  String source = "system";
  String code = "OK";

  if (snapshot.marlinStatus == "LOST") {
    level = "ERROR";
    message = "Marlin connection lost";
    source = "marlin";
    code = "MARLIN_CONNECTION_LOST";
  } else if (snapshot.marlinStatus == "ERROR") {
    level = "ERROR";
    message = "Marlin error";
    source = "marlin";
    code = "MARLIN_ERROR";
  } else if (snapshot.jobState == "ERROR") {
    level = "ERROR";
    message = snapshot.jobError.length() > 0
      ? snapshot.jobError
      : String("G-code job error");
    source = "job";
    code = "JOB_ERROR";
  } else if (unknownHomeAxes.length() > 0) {
    level = "WARNING";
    message = String("Sensor home belum terbaca: ") + unknownHomeAxes;
    source = "home_sensor";
    code = "HOME_SENSOR_UNKNOWN";
  }

  bool active = level != "INFO";
  String payload = String("{\"level\":\"") + level +
                   "\",\"message\":\"" + escapeJson(message) +
                   "\",\"active\":" + (active ? "true" : "false") +
                   ",\"popup\":" + (active ? "true" : "false") +
                   ",\"source\":\"" + escapeJson(source) +
                   "\",\"code\":\"" + escapeJson(code) +
                   "\"}";

  if (payload == _lastAlarmPayload) {
    return;
  }

  publishJson(MqttConfig::TOPIC_ALARM, payload, true);
  _lastAlarmPayload = payload;
}

void CloudMqtt::publishError(const MqttMonitoringSnapshot &snapshot) {
  bool connectionLost = snapshot.marlinStatus == "LOST";
  bool marlinError = snapshot.marlinStatus == "ERROR";
  bool jobError = snapshot.jobState == "ERROR";
  bool active = connectionLost || marlinError || jobError;
  String source = connectionLost || marlinError
    ? String("marlin")
    : (jobError ? String("job") : String("system"));
  String code = connectionLost
    ? String("MARLIN_CONNECTION_LOST")
    : (marlinError
        ? String("MARLIN_ERROR")
        : (jobError ? String("JOB_ERROR") : String("OK")));
  String message = connectionLost
    ? String("Marlin connection lost")
    : (marlinError
        ? String("Marlin error")
        : (jobError ? snapshot.jobError : String()));
  String payload = String("{\"active\":") + (active ? "true" : "false") +
                   ",\"source\":\"" + escapeJson(source) +
                   "\",\"code\":\"" + escapeJson(code) +
                   "\",\"message\":\"" +
                   escapeJson(message) + "\"}";

  if (payload == _lastErrorPayload) {
    return;
  }

  publishJson(MqttConfig::TOPIC_ERROR, payload, true);
  _lastErrorPayload = payload;
}

bool CloudMqtt::consumeFileListRequest() {
  if (!_fileListRequested) {
    return false;
  }

  _fileListRequested = false;
  return true;
}

void CloudMqtt::publishFileListError(const char *status) {
  String payload = String("{\"type\":\"file_list_error\",\"status\":\"") +
                   escapeJson(status) +
                   "\",\"count\":0}";
  publishJson(MqttConfig::TOPIC_FILE_LIST, payload, false);
}

void CloudMqtt::publishFileListBegin(size_t count) {
  String payload = String("{\"type\":\"file_list_begin\",\"status\":\"ok\",\"count\":") +
                   String(count) + "}";
  publishJson(MqttConfig::TOPIC_FILE_LIST, payload, false);
}

void CloudMqtt::publishFileListItem(size_t index, const SdCardEntry &entry) {
  String label = labelFromPath(entry.path);
  String payload = String("{\"type\":\"file_list_item\",\"index\":") +
                   String(index) +
                   ",\"label\":\"" + escapeJson(label) +
                   "\",\"path\":\"" + escapeJson(entry.path) +
                   "\",\"size\":" + String((unsigned long)entry.size) +
                   "}";
  publishJson(MqttConfig::TOPIC_FILE_LIST, payload, false);
}

void CloudMqtt::publishFileListEnd(size_t count) {
  String payload = String("{\"type\":\"file_list_end\",\"count\":") +
                   String(count) + "}";
  publishJson(MqttConfig::TOPIC_FILE_LIST, payload, false);
}

bool CloudMqtt::consumeFileSelectRequest(MqttFileSelectRequest &request) {
  if (!_fileSelectRequested) {
    return false;
  }

  request = _pendingFileSelectRequest;
  _pendingFileSelectRequest = MqttFileSelectRequest();
  _fileSelectRequested = false;
  return true;
}

void CloudMqtt::publishFileSelectOk(const String &label, const String &path, size_t size) {
  String payload = String("{\"type\":\"file_select_ack\",\"status\":\"ok\"") +
                   ",\"label\":\"" + escapeJson(label) +
                   "\",\"path\":\"" + escapeJson(path) +
                   "\",\"size\":" + String((unsigned long)size) +
                   "}";
  publishJson(MqttConfig::TOPIC_FILE_SELECT_ACK, payload, false);
  Serial.print("[MQTT][FILE SELECT] ack status=ok path=");
  Serial.println(path);
}

void CloudMqtt::publishFileSelectError(const char *reason, const String &path) {
  String payload = String("{\"type\":\"file_select_ack\",\"status\":\"error\"") +
                   ",\"reason\":\"" + escapeJson(reason) +
                   "\",\"path\":\"" + escapeJson(path) +
                   "\"}";
  publishJson(MqttConfig::TOPIC_FILE_SELECT_ACK, payload, false);
  Serial.print("[MQTT][FILE SELECT] ack status=error reason=");
  Serial.println(reason);
}

bool CloudMqtt::consumeSdStatusRequest() {
  if (!_sdStatusRequested) {
    return false;
  }

  _sdStatusRequested = false;
  return true;
}

void CloudMqtt::publishSdStatus(const char *status, bool mounted, size_t fileCount, const char *reason) {
  String payload = String("{\"type\":\"sd_status\",\"status\":\"") +
                   escapeJson(status) +
                   "\",\"mounted\":" + (mounted ? "true" : "false") +
                   ",\"fileCount\":" + String((unsigned long)fileCount);
  if (reason != nullptr && reason[0] != '\0') {
    payload += ",\"reason\":\"";
    payload += escapeJson(reason);
    payload += "\"";
  }
  payload += "}";
  publishJson(MqttConfig::TOPIC_SD_STATUS, payload, false);
}

bool CloudMqtt::consumeFileDeleteRequest(MqttFileDeleteRequest &request) {
  if (!_fileDeleteRequested) {
    return false;
  }

  request = _pendingFileDeleteRequest;
  _pendingFileDeleteRequest = MqttFileDeleteRequest();
  _fileDeleteRequested = false;
  return true;
}

void CloudMqtt::publishFileDeleteDeleted(const String &filePath) {
  String payload = String("{\"type\":\"file_delete_ack\",\"status\":\"deleted\"") +
                   ",\"filePath\":\"" + escapeJson(filePath) +
                   "\"}";
  publishJson(MqttConfig::TOPIC_FILE_DELETE_ACK, payload, false);
  Serial.println("[MQTT][FILE DELETE] ack status=deleted");
}

void CloudMqtt::publishFileDeleteError(const char *reason, const String &filePath) {
  String payload = String("{\"type\":\"file_delete_ack\",\"status\":\"error\"") +
                   ",\"filePath\":\"" + escapeJson(filePath) +
                   "\",\"reason\":\"" + escapeJson(reason) +
                   "\"}";
  publishJson(MqttConfig::TOPIC_FILE_DELETE_ACK, payload, false);
  Serial.print("[MQTT][FILE DELETE] ack status=error reason=");
  Serial.println(reason);
}

bool CloudMqtt::consumeFileStartRequest(MqttFileStartRequest &request) {
  if (!_fileStartRequested) {
    return false;
  }

  request = _pendingFileStartRequest;
  _pendingFileStartRequest = MqttFileStartRequest();
  _fileStartRequested = false;
  return true;
}

bool CloudMqtt::consumeFileChunkRequest(MqttFileChunkRequest &request) {
  if (!_fileChunkRequested) {
    return false;
  }

  request = _pendingFileChunkRequest;
  _pendingFileChunkRequest = MqttFileChunkRequest();
  _fileChunkRequested = false;
  return true;
}

bool CloudMqtt::consumeFileFinishRequest() {
  if (!_fileFinishRequested) {
    return false;
  }

  _fileFinishRequested = false;
  return true;
}

void CloudMqtt::publishFileStartReady(const String &fileName, size_t fileSize, size_t totalChunks) {
  String payload = String("{\"type\":\"start\",\"status\":\"ready\"") +
                   ",\"fileName\":\"" + escapeJson(fileName) +
                   "\",\"fileSize\":" + String((unsigned long)fileSize) +
                   ",\"totalChunks\":" + String((unsigned long)totalChunks) +
                   "}";
  publishFileAck(payload);
  Serial.println("[MQTT][FILE START] ack status=ready");
}

void CloudMqtt::publishFileStartError(const char *reason, const String &fileName, const String &path) {
  String payload = String("{\"type\":\"start\",\"status\":\"error\"") +
                   ",\"reason\":\"" + escapeJson(reason) +
                   "\",\"fileName\":\"" + escapeJson(fileName) + "\"";
  if (path.length() > 0) {
    payload += ",\"path\":\"";
    payload += escapeJson(path);
    payload += "\"";
  }
  payload += "}";
  publishFileAck(payload);
  Serial.print("[MQTT][FILE START] ack status=error reason=");
  Serial.println(reason);
}

void CloudMqtt::publishFileChunkReceived(int chunkIndex) {
  String payload = String("{\"type\":\"chunk\",\"status\":\"received\",\"chunkIndex\":") +
                   String(chunkIndex) + "}";
  publishFileAck(payload);
  Serial.print("[MQTT][FILE CHUNK] ack index=");
  Serial.println(chunkIndex);
}

void CloudMqtt::publishFileChunkError(const char *reason, int chunkIndex, const String &fileName) {
  String payload = String("{\"type\":\"chunk\",\"status\":\"error\"") +
                   ",\"reason\":\"" + escapeJson(reason) +
                   "\",\"chunkIndex\":" + String(chunkIndex) +
                   ",\"fileName\":\"" + escapeJson(fileName) +
                   "\"}";
  publishFileAck(payload);
  Serial.print("[MQTT][FILE CHUNK] ack status=error reason=");
  Serial.println(reason);
}

void CloudMqtt::publishFileFinishSaved(const String &fileName, const String &path, size_t size, size_t totalChunks) {
  String payload = String("{\"type\":\"finish\",\"status\":\"saved\"") +
                   ",\"fileName\":\"" + escapeJson(fileName) +
                   "\",\"path\":\"" + escapeJson(path) +
                   "\",\"size\":" + String((unsigned long)size) +
                   ",\"totalChunks\":" + String((unsigned long)totalChunks) +
                   "}";
  publishFileAck(payload);
  Serial.println("[MQTT][FILE FINISH] ack status=saved");
}

void CloudMqtt::publishFileFinishError(const char *reason, const String &fileName) {
  String payload = String("{\"type\":\"finish\",\"status\":\"error\"") +
                   ",\"reason\":\"" + escapeJson(reason) +
                   "\",\"fileName\":\"" + escapeJson(fileName) +
                   "\"}";
  publishFileAck(payload);
  Serial.print("[MQTT][FILE FINISH] ack status=error reason=");
  Serial.println(reason);
}

bool CloudMqtt::publishFileAck(const String &payload) {
  bool sent = publishJson(MqttConfig::TOPIC_FILE_ACK, payload, false);
  Serial.print("[MQTT][FILE][ACK] ");
  Serial.println(sent ? payload : "gagal publish");
  return sent;
}

void CloudMqtt::handleFileStart(const String &message) {
  MqttFileStartRequest request;
  bool hasName =
    parseJsonStringField(message, "fileName", request.fileName) ||
    parseJsonStringField(message, "filename", request.fileName);
  bool hasSize =
    parseJsonSizeField(message, "fileSize", request.fileSize) ||
    parseJsonSizeField(message, "size", request.fileSize);
  bool hasChunkSize = parseJsonSizeField(message, "chunkSize", request.chunkSize);
  bool hasTotalChunks = parseJsonSizeField(message, "totalChunks", request.totalChunks);
  bool overwrite = false;
  if (parseJsonBoolField(message, "overwrite", overwrite)) {
    request.overwrite = overwrite;
  }
  request.fileName.trim();
  request.validPayload =
    hasName &&
    hasSize &&
    hasChunkSize &&
    hasTotalChunks &&
    request.fileSize > 0 &&
    request.chunkSize > 0 &&
    request.totalChunks > 0;
  request.reason = request.validPayload ? "" : "invalid_payload";

  Serial.print("[MQTT][FILE START] request file=");
  Serial.print(request.fileName);
  Serial.print(" size=");
  Serial.print(request.fileSize);
  Serial.print(" chunks=");
  Serial.println(request.totalChunks);

  if (_fileStartRequested) {
    publishFileStartError("transfer_busy", request.fileName);
    return;
  }

  _pendingFileStartRequest = request;
  _fileStartRequested = true;
}

void CloudMqtt::handleFileChunk(const String &message) {
  MqttFileChunkRequest request;
  String data;
  bool hasIndex =
    parseJsonIntField(message, "chunkIndex", request.chunkIndex) ||
    parseJsonIntField(message, "index", request.chunkIndex);
  bool hasData =
    parseJsonStringField(message, "data", data) ||
    parseJsonStringField(message, "content", data) ||
    parseJsonStringField(message, "chunk", data) ||
    parseJsonStringField(message, "payload", data);
  request.validPayload = hasIndex && hasData && request.chunkIndex >= 0;
  request.reason = request.validPayload ? "" : "invalid_payload";
  stringToBytes(data, request.data);

  Serial.print("[MQTT][FILE CHUNK] request index=");
  Serial.print(request.chunkIndex);
  Serial.print(" bytes=");
  Serial.println(request.data.size());

  if (_fileChunkRequested) {
    publishFileChunkError("transfer_busy", request.chunkIndex, "");
    return;
  }

  _pendingFileChunkRequest = request;
  _fileChunkRequested = true;
}

void CloudMqtt::handleFileFinish(const String &message) {
  (void)message;

  if (_fileFinishRequested) {
    publishFileFinishError("transfer_busy", "");
    return;
  }

  _fileFinishRequested = true;
  Serial.println("[MQTT][FILE FINISH] request received");
}

void CloudMqtt::handleFileListRequest(const String &message) {
  if (message.indexOf("list_files") < 0) {
    Serial.print("[MQTT][FILE LIST] request ignored: ");
    Serial.println(message);
    return;
  }

  _fileListRequested = true;
  Serial.println("[MQTT][FILE LIST] request received");
}

void CloudMqtt::handleFileSelectRequest(const String &message) {
  String type;
  String path;
  bool validPayload =
    parseJsonStringField(message, "type", type) &&
    type == "select_file" &&
    parseJsonStringField(message, "path", path);
  path.trim();

  Serial.print("[MQTT][FILE SELECT] request received path=");
  Serial.println(path);

  (void)validPayload;
  publishFileSelectError("remote_selection_disabled", path);
}

void CloudMqtt::handleSdStatusRequest(const String &message) {
  (void)message;
  _sdStatusRequested = true;
}

void CloudMqtt::handleFileDeleteRequest(const String &message) {
  MqttFileDeleteRequest request;
  request.validPayload = parseJsonStringField(message, "filePath", request.filePath);
  request.filePath.trim();
  request.reason = request.validPayload ? "" : "invalid_payload";

  Serial.print("[MQTT][FILE DELETE] request path=");
  Serial.println(request.filePath);

  if (_fileDeleteRequested) {
    publishFileDeleteError("transfer_busy", request.filePath);
    return;
  }

  _pendingFileDeleteRequest = request;
  _fileDeleteRequested = true;
}

void CloudMqtt::ensureConfigured() {
  if (_broker.length() == 0 ||
      _port == 0 ||
      _topicPrefix.length() == 0) {
    configure(
      _broker.length() ? _broker : String(MqttConfig::BROKER),
      _port == 0 ? MqttConfig::PORT : _port,
      _topicPrefix.length() ? _topicPrefix : String(MqttConfig::TOPIC_PREFIX),
      _user,
      _password
    );
  }
}

bool CloudMqtt::brokerReachable() {
  WiFiClient probe;
  probe.setTimeout(MqttConfig::SOCKET_TIMEOUT_SEC);

  bool reachable = probe.connect(
    _broker.c_str(),
    _port,
    MqttConfig::CONNECT_TIMEOUT_MS
  );
  probe.stop();

  if (!reachable) {
    _lastState = MQTT_CONNECTION_TIMEOUT;
    Serial.print("MQTT broker tidak terjangkau: ");
    Serial.print(_broker);
    Serial.print(":");
    Serial.println(_port);
  }

  return reachable;
}

void CloudMqtt::handleMessage(char *topic, byte *payload, unsigned int length) {
  String message;
  message.reserve(length + 1);

  for (unsigned int i = 0; i < length; i++) {
    message += static_cast<char>(payload[i]);
  }

  String incomingTopic(topic);
  if (incomingTopic == topicName(MqttConfig::TOPIC_COMMAND)) {
    Serial.print("[MQTT][COMMAND] ");
    Serial.println(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_GCODE)) {
    Serial.print("[MQTT][GCODE] ");
    Serial.println(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_FILE_START)) {
    handleFileStart(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_FILE_CHUNK)) {
    handleFileChunk(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_FILE_FINISH)) {
    handleFileFinish(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_FILE_LIST_REQUEST)) {
    handleFileListRequest(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_FILE_SELECT)) {
    handleFileSelectRequest(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_FILE_DELETE)) {
    handleFileDeleteRequest(message);
    return;
  }

  if (incomingTopic == topicName(MqttConfig::TOPIC_SD_STATUS_REQUEST)) {
    handleSdStatusRequest(message);
    return;
  }

  Serial.print("[MQTT][IGNORED] ");
  Serial.print(incomingTopic);
  Serial.print(" = ");
  Serial.println(message);
}
