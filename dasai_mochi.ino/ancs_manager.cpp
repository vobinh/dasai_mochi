#include "ancs_manager.h"

#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <NimBLEClient.h>
#include <NimBLERemoteService.h>
#include <NimBLERemoteCharacteristic.h>

#define DEBUG_ANCS 1
#define ENABLE_PERIPHERAL_ADVERTISING 0  // Bật = 1 nếu muốn iPhone nhìn thấy "Mochi Watch" trong danh sách

// ======================
// UUIDs Apple ANCS
// ======================
static NimBLEUUID kANCSServiceUUID        ("7905f431-b5ce-4e99-a40f-4b1e122d00d0");
static NimBLEUUID kNotificationSourceUUID ("9fbf120d-6301-42d9-8c58-25e699a21dbd");
static NimBLEUUID kControlPointUUID       ("69d1d8f3-45e1-49a8-9821-9bbdfdaad9d9");
static NimBLEUUID kDataSourceUUID         ("22eac6e9-24d6-4bb5-be44-b36ace7c742b");

// ======================
// Trạng thái module
// ======================
static AppSettings*     g_settings = nullptr;
static BleSharedState*  g_state    = nullptr;

static NimBLEClient*            g_client        = nullptr;
static NimBLEAdvertisedDevice*  g_targetAdv     = nullptr;

static BLERemoteCharacteristic* g_charNotifSrc  = nullptr;
static BLERemoteCharacteristic* g_charCtrlPoint = nullptr;
static BLERemoteCharacteristic* g_charDataSrc   = nullptr;

static bool g_isConnecting = false;

#if ENABLE_PERIPHERAL_ADVERTISING
static NimBLEServer*       g_server = nullptr;
static NimBLEAdvertising*  g_adv    = nullptr;
#endif

// ======================
// Helpers
// ======================
static inline void safeSetBool(bool* p, bool v) { if (p) *p = v; }
static inline void safeSetString(String* p, const String& s) { if (p) *p = s; }

static void startScanning(uint32_t seconds = 0 /*0 = không giới hạn*/) {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!scan) return;

  if (!scan->isScanning()) {
#if DEBUG_ANCS
    Serial.printf("[ANCS] Start scanning (%us)...\n", (unsigned)seconds);
#endif
    scan->start(seconds, false);
  }
}

static bool isAppleManufacturer(const std::string& mf) {
  // Company ID Apple = 0x004C; một số stack đảo byte, nên check cả 0x4C 0x00 và 0x00 0x4C
  if (mf.size() < 2) return false;
  uint8_t b0 = (uint8_t)mf[0], b1 = (uint8_t)mf[1];
  return (b0 == 0x4C && b1 == 0x00) || (b0 == 0x00 && b1 == 0x4C);
}

#if ENABLE_PERIPHERAL_ADVERTISING
static void maybeStartAdvertising() {
  if (g_client && g_client->isConnected()) return;
  if (g_isConnecting) return;

  if (!g_server) g_server = NimBLEDevice::createServer();
  if (!g_adv)    g_adv    = NimBLEDevice::getAdvertising();

  g_adv->setName("Mochi Watch");
  g_adv->setScanResponse(true);
  g_adv->start();
# if DEBUG_ANCS
  Serial.println("[ANCS] Peripheral advertising started (optional)");
# endif
}

static void stopAdvertising() {
  if (g_adv) g_adv->stop();
# if DEBUG_ANCS
  Serial.println("[ANCS] Peripheral advertising stopped");
# endif
}
#endif

// ======================
// Notify callbacks
// ======================

// Notification Source payload ≥ 8 byte:
// 0: EventID (0=Added,1=Modified,2=Removed)
// 1: EventFlags
// 2: CategoryID (1=Incoming Call,...)
// 3: CategoryCount
// 4..7: NotificationUID (LE)
static void onNotificationSource(BLERemoteCharacteristic* ch, uint8_t* pData, size_t len, bool isNotify) {
  if (len < 8) return;

  uint8_t  eventID    = pData[0];
  uint8_t  categoryID = pData[2];
  uint32_t notiUID    = (uint32_t)pData[4] |
                        ((uint32_t)pData[5] << 8) |
                        ((uint32_t)pData[6] << 16) |
                        ((uint32_t)pData[7] << 24);

#if DEBUG_ANCS
  Serial.printf("[ANCS] NS event=%u category=%u uid=%lu\n", eventID, categoryID, (unsigned long)notiUID);
#endif

  if (eventID == 0) { // Added
    if (categoryID == 1) {
      safeSetBool(g_state ? g_state->isRinging : nullptr, true);
      safeSetString(g_state ? g_state->callerInfo : nullptr, "Cuộc gọi đến");
    } else {
      safeSetBool(g_state ? g_state->hasNewNotification : nullptr, true);
    }
  } else if (eventID == 2) { // Removed
    if (categoryID == 1) {
      safeSetBool(g_state ? g_state->isRinging : nullptr, false);
    }
  }
}

static void onDataSource(BLERemoteCharacteristic* ch, uint8_t* pData, size_t len, bool isNotify) {
#if DEBUG_ANCS
  Serial.printf("[ANCS] DS len=%u\n", (unsigned)len);
#endif
  // TODO: Parse nếu gửi lệnh Control Point để lấy title/message/app...
}

// ======================
// Client Callbacks
// ======================
class ClientCallbacks : public NimBLEClientCallbacks {
public:
  void onConnect(NimBLEClient* client) {
#if DEBUG_ANCS
    Serial.println("[ANCS] Connected to iOS");
#endif
    safeSetBool(g_state ? g_state->isConnected : nullptr, true);

#if ENABLE_PERIPHERAL_ADVERTISING
    stopAdvertising();
#endif

    // Khám phá ANCS
    BLERemoteService* svc = client->getService(kANCSServiceUUID);
    if (!svc) {
#if DEBUG_ANCS
      Serial.println("[ANCS] ANCS service NOT found on device!");
#endif
      return;
    }

    g_charNotifSrc  = svc->getCharacteristic(kNotificationSourceUUID);
    g_charCtrlPoint = svc->getCharacteristic(kControlPointUUID);
    g_charDataSrc   = svc->getCharacteristic(kDataSourceUUID);

    if (g_charNotifSrc && g_charNotifSrc->canNotify()) {
      if (g_charNotifSrc->subscribe(true, onNotificationSource)) {
#if DEBUG_ANCS
        Serial.println("[ANCS] Subscribed Notification Source");
#endif
      } else {
#if DEBUG_ANCS
        Serial.println("[ANCS] Subscribe NS FAILED");
#endif
      }
    } else {
#if DEBUG_ANCS
      Serial.println("[ANCS] NS char missing/cannot notify");
#endif
    }

    if (g_charDataSrc && g_charDataSrc->canNotify()) {
      if (g_charDataSrc->subscribe(true, onDataSource)) {
#if DEBUG_ANCS
        Serial.println("[ANCS] Subscribed Data Source");
#endif
      } else {
#if DEBUG_ANCS
        Serial.println("[ANCS] Subscribe DS FAILED");
#endif
      }
    } else {
#if DEBUG_ANCS
      Serial.println("[ANCS] DS char missing/cannot notify");
#endif
    }

    if (g_charCtrlPoint) {
#if DEBUG_ANCS
      Serial.println("[ANCS] Control Point ready");
#endif
    } else {
#if DEBUG_ANCS
      Serial.println("[ANCS] Control Point missing");
#endif
    }
  }

  void onDisconnect(NimBLEClient* client) {
#if DEBUG_ANCS
    Serial.println("[ANCS] Disconnected");
#endif
    safeSetBool(g_state ? g_state->isConnected : nullptr, false);

    g_charNotifSrc  = nullptr;
    g_charCtrlPoint = nullptr;
    g_charDataSrc   = nullptr;

    NimBLEDevice::deleteClient(client);
    g_client = nullptr;

    // Cho phép quét/advertise lại
    startScanning(0);
#if ENABLE_PERIPHERAL_ADVERTISING
    maybeStartAdvertising();
#endif
  }

  void onDisconnect(NimBLEClient* client, int reason) {
#if DEBUG_ANCS
    Serial.printf("[ANCS] Disconnected (reason=%d)\n", reason);
#endif
    onDisconnect(client);
  }
};

// ======================
// Scan Callbacks (NimBLEScanCallbacks)
// ======================
class ScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(NimBLEAdvertisedDevice* dev) {
    // Chỉ lấy thiết bị connectable (nếu API này có; nếu không có, bỏ điều kiện này)
#if defined(NIMBLE_FEATURE_EXTENDED_SCANNING)
    // no-op
#endif

    bool isApple = false;

    // 1) Ưu tiên theo tên khi iPhone đang mở Settings > Bluetooth
    std::string name = dev->getName();
    if (!name.empty() &&
        (name.find("iPhone") != std::string::npos ||
         name.find("iPad")   != std::string::npos)) {
      isApple = true;
    }

    // 2) Không có tên? Thử Manufacturer Data (Apple = 0x004C)
    if (!isApple) {
      std::string mf = dev->getManufacturerData();
      if (!mf.empty() && isAppleManufacturer(mf)) {
        isApple = true;
      }
    }

    if (!isApple) return;

#if DEBUG_ANCS
    Serial.printf("[ANCS] Found iOS candidate: name='%s' RSSI=%d\n", name.c_str(), dev->getRSSI());
#endif

    NimBLEDevice::getScan()->stop();

    if (g_targetAdv) { delete g_targetAdv; g_targetAdv = nullptr; }
    g_targetAdv = new NimBLEAdvertisedDevice(*dev);
    g_isConnecting = true;
  }
};

// ======================
// Public API
// ======================
void ancs_init(AppSettings* settings, BleSharedState* state) {
  g_settings = settings;
  g_state    = state;

#if DEBUG_ANCS
  Serial.println("[ANCS] Init NimBLE (Mochi Watch)");
#endif

  // Bảo mật/bonding cho ANCS
  NimBLEDevice::setSecurityAuth(true, true, true);              // bonding + MITM + LESC
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);    // không có IO
  NimBLEDevice::init("Mochi Watch");

  // Tối ưu tầm bắt + throughput
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // công suất cao nhất
  NimBLEDevice::setMTU(247);

  if (g_state) {
    safeSetBool(g_state->isConnected, false);
    safeSetBool(g_state->isRinging, false);
    safeSetBool(g_state->hasNewNotification, false);
    safeSetString(g_state->callerInfo, "");
  }

  // Scanner
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new ScanCallbacks(), false /*deleteOnFinish*/);
  scan->setActiveScan(true);       // cần để có scan response (tên thiết bị)
  scan->setDuplicateFilter(true);
  scan->setInterval(80);           // ~50 ms
  scan->setWindow(60);             // ~37.5 ms (window < interval)

  // Bắt đầu quét liên tục
  startScanning(0);

#if ENABLE_PERIPHERAL_ADVERTISING
  maybeStartAdvertising(); // tuỳ chọn – chỉ để iPhone thấy "Mochi Watch" trong danh sách
#endif
}

void ancs_loop() {
  // Nếu vừa “nhặt” được iOS thì connect
  if (g_isConnecting && g_targetAdv != nullptr) {
#if DEBUG_ANCS
    Serial.println("[ANCS] Connecting...");
#endif
    if (!g_client) {
      g_client = NimBLEDevice::createClient();
      g_client->setClientCallbacks(new ClientCallbacks(), false /*deleteOnFinish*/);
    }

#if ENABLE_PERIPHERAL_ADVERTISING
    stopAdvertising();
#endif

    g_client->setConnectTimeout(6);
    bool ok = g_client->connect(g_targetAdv);

    if (!ok) {
#if DEBUG_ANCS
      Serial.println("[ANCS] Connect FAILED");
#endif
      if (g_client) {
        if (g_client->isConnected()) g_client->disconnect();
        NimBLEDevice::deleteClient(g_client);
        g_client = nullptr;
      }
      // Quét lại
      startScanning(0);
#if ENABLE_PERIPHERAL_ADVERTISING
      maybeStartAdvertising();
#endif
    } else {
#if DEBUG_ANCS
      Serial.println("[ANCS] Connected OK (service discovery in onConnect)");
#endif
    }

    delete g_targetAdv; g_targetAdv = nullptr;
    g_isConnecting = false;
  }

  // Nếu không kết nối và không đang connect → đảm bảo đang quét
  if (!g_isConnecting && (g_client == nullptr || !g_client->isConnected())) {
    startScanning(0);
#if ENABLE_PERIPHERAL_ADVERTISING
    maybeStartAdvertising();
#endif
  }
}
