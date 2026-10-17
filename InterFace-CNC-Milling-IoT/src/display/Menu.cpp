#include "Menu.h"
#include "../config/AppConfig.h"

namespace {
  String standbyActivityLabel(const char *activityState, int progress) {
    if (activityState == nullptr || activityState[0] == '\0') {
      return (progress >= 0) ? String("RUN") : String("IDLE");
    }

    String state(activityState);
    if (state == "RUNNING") return String("RUN");
    if (state == "PAUSED") return String("PAUS");
    if (state == "STOPPING") return String("STOP");
    if (state == "ERROR") return String("ERR");

    return state;
  }
}

Menu::Menu(LcdHandler &lcd)
  : _lcd(lcd) {
}

void Menu::showStandby(float x, float y, float z, const char *eta, const char *jobName, int progress, const char *timeStr, const char *machineStatus, const char *networkStatus, bool positionValid, const char *activityState) {
  char top[32];
  char mid[32];
  char bottom[32];

  // Baris 1: status CNC lebih dulu, lalu status pekerjaan.
  String activity = standbyActivityLabel(activityState, progress);
  String topLine = activity;
  if (machineStatus != nullptr && machineStatus[0] != '\0') {
    topLine = String(machineStatus) + " | " + activity;
  }
  snprintf(top, sizeof(top), "%s", topLine.c_str());

  // Baris 2: Koordinat X dan Y (Gunakan spasi tetap agar angka sejajar)
  if (positionValid) {
    snprintf(mid, sizeof(mid), "X:%6.2f Y:%6.2f", x, y);
  } else {
    snprintf(mid, sizeof(mid), "X:%6s Y:%6s", "?", "?");
  }

  // Baris 3: Koordinat Z (Feedrate dihapus untuk memberi ruang Progress Bar)
  if (positionValid) {
    snprintf(bottom, sizeof(bottom), "Z:%6.2f", z);
  } else {
    snprintf(bottom, sizeof(bottom), "Z:%6s", "?");
  }

  _lcd.showStandbyLines(top, mid, bottom, eta, jobName, progress, timeStr, networkStatus);
}

void Menu::showMainMenu(const std::vector<String> &items, size_t selected) {
  showMenu("MAIN MENU", items, selected);
}

void Menu::showMenu(const char *title, const std::vector<String> &items, size_t selected) {
  _title = title;
  _items = items;
  _selected = _items.empty() ? 0 : min(selected, _items.size() - 1);
  redraw();
}

void Menu::redraw() {
  _lcd.showMenu(_title.c_str(), _items, _selected);
}

void Menu::selectNext() {
  if (_items.empty()) return;
  _selected = (_selected + 1) % _items.size();
  redraw();
}

void Menu::selectPrev() {
  if (_items.empty()) return;
  if (_selected == 0) _selected = _items.size() - 1;
  else --_selected;
  redraw();
}

size_t Menu::selectedIndex() const {
  return _selected;
}

const std::vector<String> &Menu::items() const {
  return _items;
}

const String &Menu::title() const {
  return _title;
}
