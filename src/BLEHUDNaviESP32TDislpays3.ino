#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Button2.h>
#include <TFT_eSPI.h>
#include "DataConstants.h"
#include "ImagesOther.h"
#include "ImagesDirections.h"
#include "ImagesLanes.h"
#include "VoltageMeasurement.h"

// ---- Config ----
#define SERVICE_UUID        "DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_INDICATE_UUID  "DD3F0AD2-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_WRITE_UUID     "DD3F0AD3-6239-4E1F-81F1-91F6C9F01D86"

#define COLOR_BLACK    TFT_BLACK
#define COLOR_WHITE    TFT_WHITE
#define COLOR_RED      TFT_RED
#define COLOR_MAGENTA  0xF81F
#define COLOR_BLUE     TFT_BLUE

#define ENABLE_VOLTAGE_MEASUREMENT false
#define TTGO_LEFT_BUTTON 0
#define TTGO_RIGHT_BUTTON 35

// ---- Globals ----
TFT_eSPI tft = TFT_eSPI();
BLEServer* g_pServer = nullptr;
BLECharacteristic* g_pCharIndicate = nullptr;
bool g_deviceConnected = false;
uint32_t g_lastActivityTime = 0;
bool g_isNaviDataUpdated = false;
std::string g_naviData;

Button2 btnDeepSleep(TTGO_LEFT_BUTTON);
Button2 btnVol(TTGO_RIGHT_BUTTON);
bool g_sleepRequested = false;
static VoltageMeasurement g_voltage(34, 14);
bool g_showVoltage = false;

// ---- NUOVA GESTIONE STATI ----
enum DisplayState {
  STATE_STARTING,
  STATE_DISCONNECTED,
  STATE_NO_ROUTE,
  STATE_NAVIGATING,
  STATE_RECALCULATING
};
DisplayState g_currentState = STATE_STARTING;
DisplayState g_lastState = STATE_STARTING;
DisplayState g_previousState = STATE_STARTING;

// Variabili per delta-update
uint8_t lastSpeed = 0;
uint8_t lastDir   = DirectionNone;

// ---- NUOVE FUNZIONI GRAFICHE ----

void playStartupAnimation() {
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Navigatore BLE", tft.width() / 2, tft.height() / 2 - 40);
  tft.drawString("Inizializzazione...", tft.width() / 2, tft.height() / 2 - 20);

  int barWidth = 150;
  int barHeight = 10;
  int barX = (tft.width() - barWidth) / 2;
  int barY = tft.height() / 2 + 20;

  tft.drawRect(barX, barY, barWidth, barHeight, COLOR_WHITE);

  for (int i = 0; i < barWidth - 4; i++) {
    tft.fillRect(barX + 2, barY + 2, i, barHeight - 4, COLOR_BLUE);
    delay(15);
  }
  delay(500);
  tft.setTextDatum(TL_DATUM);
}

void showDisconnectedScreen() {
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_RED, COLOR_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Disconnesso", tft.width() / 2, tft.height() / 2 - 15);
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  tft.setTextSize(1);
  tft.drawString("In attesa di connessione...", tft.width() / 2, tft.height() / 2 + 15);
  tft.setTextDatum(TL_DATUM);
}

void showNoRouteScreen() {
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Connesso", tft.width() / 2, tft.height() / 2 - 15);
  tft.setTextSize(1);
  tft.drawString("In attesa di un percorso...", tft.width() / 2, tft.height() / 2 + 15);
  tft.setTextDatum(TL_DATUM);
}

void showRecalculatingScreen() {
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_MAGENTA, COLOR_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Ricalcolo...", tft.width() / 2, tft.height() / 2);
}

// ---- Helpers ----
uint16_t Color4To16bit(uint8_t c4) {
  uint16_t r = (c4 & 0x0F) * 0x1F / 0x0F;
  uint16_t g = (c4 & 0x0F) * 0x3F / 0x0F;
  uint16_t b = (c4 & 0x0F) * 0x1F / 0x0F;
  return (r << 11) | (g << 5) | b;
}

void Draw4bitImageProgmem(int x, int y, int w, int h, const uint8_t* img) {
  int pixels = w * h;
  for (int i = 0; i < pixels; i += 2) {
    uint8_t data = pgm_read_byte(img++);
    uint8_t p1 = data & 0x0F, p2 = data >> 4;
    int x1 = x + (i % w), y1 = y + (i / w);
    int x2 = x1 + 1;
    tft.drawPixel(x1, y1, Color4To16bit(p1));
    if ((i + 1) < pixels)
      tft.drawPixel(x2, y1, Color4To16bit(p2));
  }
}

void Draw4bitImageProgmemScaled(int x, int y, int w, int h, const uint8_t* img, int scale) {
  int pixels = w * h;
  for (int i = 0; i < pixels; i += 2) {
    uint8_t data = pgm_read_byte(img++);
    uint8_t p1 = data & 0x0F, p2 = data >> 4;
    int origX = i % w;
    int origY = i / w;
    tft.fillRect(x + origX * scale, y + origY * scale, scale, scale, Color4To16bit(p1));
    if (i + 1 < pixels) {
      origX = (i + 1) % w;
      origY = (i + 1) / w;
      tft.fillRect(x + origX * scale, y + origY * scale, scale, scale, Color4To16bit(p2));
    }
  }
}

const uint8_t* ImageFromDirection(uint8_t direction) {
  switch (direction) {
    case DirectionNone: return nullptr;
    case DirectionStart:
    case DirectionEnd:
    case DirectionVia: return IMG_directionWaypoint;
    case DirectionEasyLeft: return IMG_directionEasyLeft;
    case DirectionEasyRight: return IMG_directionEasyRight;
    case DirectionKeepLeft: return IMG_directionKeepLeft;
    case DirectionKeepRight: return IMG_directionKeepRight;
    case DirectionLeft: return IMG_directionLeft;
    case DirectionOutOfRoute: return IMG_directionOutOfRoute;
    case DirectionRight: return IMG_directionRight;
    case DirectionSharpLeft: return IMG_directionSharpLeft;
    case DirectionSharpRight: return IMG_directionSharpRight;
    case DirectionStraight: return IMG_directionStraight;
    case DirectionUTurnLeft: return IMG_directionUTurnLeft;
    case DirectionUTurnRight: return IMG_directionUTurnRight;
    case DirectionFerry: return IMG_directionFerry;
    case DirectionStateBoundary: return IMG_directionStateBoundary;
    case DirectionFollow: return IMG_directionFollow;
    case DirectionMotorway: return IMG_directionMotorway;
    case DirectionTunnel: return IMG_directionTunnel;
    case DirectionExitLeft: return IMG_directionExitLeft;
    case DirectionExitRight: return IMG_directionExitRight;
    case DirectionRoundaboutRSE: return IMG_directionRoundaboutRSE;
    case DirectionRoundaboutRE: return IMG_directionRoundaboutRE;
    case DirectionRoundaboutRNE: return IMG_directionRoundaboutRNE;
    case DirectionRoundaboutRN: return IMG_directionRoundaboutRN;
    case DirectionRoundaboutRNW: return IMG_directionRoundaboutRNW;
    case DirectionRoundaboutRW: return IMG_directionRoundaboutRW;
    case DirectionRoundaboutRSW: return IMG_directionRoundaboutRSW;
    case DirectionRoundaboutRS: return IMG_directionRoundaboutRS;
    case DirectionRoundaboutLSE: return IMG_directionRoundaboutLSE;
    case DirectionRoundaboutLE: return IMG_directionRoundaboutLE;
    case DirectionRoundaboutLNE: return IMG_directionRoundaboutLNE;
    case DirectionRoundaboutLN: return IMG_directionRoundaboutLN;
    case DirectionRoundaboutLNW: return IMG_directionRoundaboutLNW;
    case DirectionRoundaboutLW: return IMG_directionRoundaboutLW;
    case DirectionRoundaboutLSW: return IMG_directionRoundaboutLSW;
    case DirectionRoundaboutLS: return IMG_directionRoundaboutLS;
    default: return IMG_directionError;
  }
}

void DrawMessage(const char* msg, int x, int y, int sz, uint16_t col) {
  tft.setTextSize(sz);
  tft.setTextColor(col, COLOR_BLACK);
  int line = 0;
  char buf[64]; strcpy(buf, msg);
  char* token = strtok(buf, "\n");
  while (token) {
    tft.drawString(token, x, y + line * (8 * sz + 4));
    token = strtok(nullptr, "\n");
    line++;
  }
}

void DrawBottomMessage(const char* m, uint16_t col){
  int th = 16;
  tft.fillRect(0, tft.height() - th, tft.width(), th, COLOR_BLACK);
  DrawMessage(m, 0, tft.height() - th + 2, 2, col);
}

// ---- Funzioni di aggiornamento (delta-update) ----
void updateSpeed(uint8_t speed) {
  if (speed == lastSpeed && g_currentState == g_lastState) return;
  lastSpeed = speed;
  int cx = tft.width()/2;
  int cy = 55 + 8;
  int r  = 55;
  // Pulisci solo il cerchio
  tft.fillCircle(cx, cy, r, COLOR_BLACK);
  if (speed) {
    char s[4]; sprintf(s, "%u", speed);
    tft.fillCircle(cx, cy, r, COLOR_RED);
    tft.fillCircle(cx, cy, r - 6, COLOR_WHITE);
    tft.setTextSize(6);
    tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(s, cx + 3, cy + 4);
    tft.setTextDatum(TL_DATUM);
  }
}

void updateDirection(uint8_t dir) {
  if (dir == lastDir && g_currentState == g_lastState) return;
  lastDir = dir;
  int scale = 2;
  int w = 64 * scale, h = 64 * scale;
  int x = (tft.width() - w) / 2, y = 118 + 10;
  // Pulisci solo l’area della freccia
  tft.fillRect(x, y, w, h, COLOR_BLACK);
  // Disegna nuova freccia
  const uint8_t* img = ImageFromDirection(dir);
  if (img) Draw4bitImageProgmemScaled(x, y, 64, 64, img, scale);
}

// ---- BLE Callbacks ----
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    g_deviceConnected = true;
    g_lastActivityTime = millis();
    g_previousState = g_currentState;
    g_currentState = STATE_NO_ROUTE;
  }
  void onDisconnect(BLEServer* pServer) override {
    g_deviceConnected = false;
    g_previousState = g_currentState;
    g_currentState = STATE_DISCONNECTED;
    BLEDevice::startAdvertising();
  }
};

class MyCharWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    g_lastActivityTime = millis();
    auto value = pCharacteristic->getValue();
    if (value.empty()) return;

    g_naviData = value;
    g_previousState = g_currentState;

    if (g_naviData.find("No route") != std::string::npos) {
      // Ricevuto No route in qualsiasi posizione
      if (g_previousState == STATE_NAVIGATING) {
        g_currentState = STATE_RECALCULATING;
      } else {
        g_currentState = STATE_NO_ROUTE;
      }
      g_isNaviDataUpdated = false;

    } else if ((uint8_t)g_naviData[0] == 1) {
      g_currentState = STATE_NAVIGATING;
      g_isNaviDataUpdated = true;

    } else {
      Serial.println("Dati BLE non riconosciuti");
    }
  }
};

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);

  playStartupAnimation();

  BLEDevice::init("ESP32 HUD");
  g_pServer = BLEDevice::createServer();
  g_pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = g_pServer->createService(SERVICE_UUID);

  g_pCharIndicate = pService->createCharacteristic(CHAR_INDICATE_UUID, BLECharacteristic::PROPERTY_INDICATE);
  g_pCharIndicate->addDescriptor(new BLE2902());
  g_pCharIndicate->setValue("");

  BLECharacteristic* pCharWrite = pService->createCharacteristic(CHAR_WRITE_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCharWrite->setCallbacks(new MyCharWriteCallbacks());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

    // Configurazione pulsante Deep Sleep: long click -> deep sleep
  btnDeepSleep.setLongClickTime(1000);
  btnDeepSleep.setLongClickDetectedHandler([](Button2& b) {
    DrawBottomMessage("SLEEP", COLOR_MAGENTA);
    // Entra subito in deep sleep
    esp_deep_sleep_start();
  });
  btnDeepSleep.setReleasedHandler([](Button2& b) {
    if (g_sleepRequested) {
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
      esp_deep_sleep_start();
    }
  });

  if (ENABLE_VOLTAGE_MEASUREMENT) {
    g_voltage.begin();
    btnVol.setPressedHandler([](Button2&) { g_showVoltage = true; });
    btnVol.setReleasedHandler([](Button2&) { g_showVoltage = false; });
  }

  g_currentState = STATE_DISCONNECTED;
}

void loop() {
  btnDeepSleep.loop();
  btnVol.loop();

  if (g_currentState != g_lastState) {
    g_sleepRequested = false;
    switch (g_currentState) {
      case STATE_DISCONNECTED:
        showDisconnectedScreen();
        break;
      case STATE_NO_ROUTE:
        showNoRouteScreen();
        break;
      case STATE_RECALCULATING:
        showRecalculatingScreen();
        break;
      case STATE_NAVIGATING:
        tft.fillScreen(COLOR_BLACK);
        lastSpeed = 0xFF;
        lastDir = 0xFF;
        break;
      case STATE_STARTING:
        break;
    }
    g_lastState = g_currentState;
  }

  if (g_currentState == STATE_NAVIGATING && g_isNaviDataUpdated) {
    g_isNaviDataUpdated = false;
    auto &d = g_naviData;
    if (!d.empty() && (uint8_t)d[0] == 1) {
      updateSpeed(d.size() > 1 ? (uint8_t)d[1] : 0);
      updateDirection(d.size() > 2 ? (uint8_t)d[2] : DirectionNone);
      if (d.size() > 3) {
        const char* msg = d.c_str() + 3;
        int sz = (strlen(msg) > 8) ? 2 : 4;
        int charH = 8 * sz + 4;
        int my = tft.height() - charH - 10;
        tft.fillRect(0, my, tft.width(), charH, COLOR_BLACK);
        int msgW = strlen(msg) * 6 * sz;
        int mx = (tft.width() - msgW) / 2;
        DrawMessage(msg, mx, my, sz, COLOR_WHITE);
      }
    }
  }

  if (g_showVoltage) {
    static uint32_t vt = 0;
    if (millis() - vt > 1000) {
      vt = millis();
      char vs[16]; sprintf(vs, "%.2f V", g_voltage.measureVolts());
      DrawBottomMessage(vs, COLOR_WHITE);
    }
  } else if (!g_sleepRequested && g_deviceConnected) {
    if (millis() - g_lastActivityTime > 4000) {
      g_lastActivityTime = millis();
      g_pCharIndicate->indicate();
    }
  }

  delay(10);
}
