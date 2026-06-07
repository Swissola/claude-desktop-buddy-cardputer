#include "ble_bridge.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <string.h>

// Nordic UART Service UUIDs — every BLE serial example uses these, so
// existing tools (nRF Connect, bluefy, Web Bluetooth examples) can talk to
// us without custom UUIDs.
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// Incoming bytes are buffered in a simple ring for bleRead()/bleAvailable().
// Sized to hold a transcript snapshot JSON plus headroom; the GATT layer
// will flow-control if we fall behind.
static const size_t RX_CAP = 2048;
static uint8_t  rxBuf[RX_CAP];
static volatile size_t rxHead = 0;
static volatile size_t rxTail = 0;

static BLEServer*         server = nullptr;
static BLECharacteristic* txChar = nullptr;
static BLECharacteristic* rxChar = nullptr;
static volatile bool      connected = false;
static volatile bool      secure = false;
static volatile uint32_t  passkey = 0;
static volatile uint16_t  mtu = 23;

// BD address of the currently connected peer. Set on connect, cleared on
// disconnect. Used by bleRemoveCurrentBond() so "unpair" only removes the
// host that sent the command rather than wiping all stored bonds.
static esp_bd_addr_t      peerAddr;
static volatile bool      hasPeer = false;

// Set true while the device is in deep idle-sleep: suppresses the automatic
// advertising restart in onDisconnect so the radio goes fully quiet during
// sleep (advertising is a continuous idle drain). Cleared and advertising is
// explicitly restarted by bleStartAdvertising() on wake.
static volatile bool      idleSilenced = false;

// Set by onDisconnect, actioned by bleService() from the main loop. Restarting
// advertising from INSIDE the onDisconnect callback context can crash the BLE
// stack when the disconnect is abrupt (e.g. the central vanished / a radio
// reset tore the link mid-teardown) — the stack is still unwinding. Deferring
// the startAdvertising() call to the next main-loop tick avoids that.
static volatile bool      wantAdvRestart = false;

static void rxPush(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    size_t next = (rxHead + 1) % RX_CAP;
    if (next == rxTail) return;  // full — drop (upstream should keep up)
    rxBuf[rxHead] = p[i];
    rxHead = next;
  }
}

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    std::string v = c->getValue();
    if (!v.empty()) rxPush((const uint8_t*)v.data(), v.size());
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  // Use the extended overload to capture the peer BD address.
  void onConnect(BLEServer* s, esp_ble_gatts_cb_param_t* param) override {
    connected = true;
    memcpy((void*)peerAddr, param->connect.remote_bda, sizeof(esp_bd_addr_t));
    hasPeer = true;
    Serial.printf("[ble] connected %02x:%02x:%02x:%02x:%02x:%02x\n",
      peerAddr[0], peerAddr[1], peerAddr[2],
      peerAddr[3], peerAddr[4], peerAddr[5]);
  }
  void onDisconnect(BLEServer* s) override {
    connected = false;
    secure = false;
    passkey = 0;
    mtu = 23;
    hasPeer = false;
    Serial.println("[ble] disconnected");
    // Restart advertising so the next client can find us — UNLESS we're entering
    // deep idle-sleep, where we deliberately keep the radio silent to save power
    // (bleStartAdvertising() re-enables it on wake). Defer the actual restart to
    // bleService() on the next main-loop tick: calling startAdvertising() here,
    // inside the disconnect callback, can crash on an abrupt disconnect.
    if (!idleSilenced) {
      wantAdvRestart = true;
    }
  }
  void onMtuChanged(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
    mtu = param->mtu.mtu;
    Serial.printf("[ble] mtu=%u\n", mtu);
  }
};

// LE Secure Connections, passkey-entry: we are DisplayOnly, the central
// is KeyboardOnly. The stack picks a random 6-digit passkey, calls
// onPassKeyNotify here, and the user types it on the desktop. main.cpp
// polls blePasskey() to render it.
class SecCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 0; }
  bool onConfirmPIN(uint32_t) override { return false; }
  bool onSecurityRequest() override { return true; }
  void onPassKeyNotify(uint32_t pk) override {
    passkey = pk;
    Serial.printf("[ble] passkey %06lu\n", (unsigned long)pk);
  }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    passkey = 0;
    secure = cmpl.success;
    Serial.printf("[ble] auth %s\n", cmpl.success ? "ok" : "FAIL");
    if (!cmpl.success && server) server->disconnect(server->getConnId());
  }
};

void bleInit(const char* deviceName) {
  BLEDevice::init(deviceName);
  // Request the biggest MTU we can get. macOS negotiates to 185 typically.
  BLEDevice::setMTU(517);

  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new SecCallbacks());

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* svc = server->createService(NUS_SERVICE_UUID);

  txChar = svc->createCharacteristic(
    NUS_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  txChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  BLE2902* cccd = new BLE2902();
  cccd->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  txChar->addDescriptor(cccd);

  rxChar = svc->createCharacteristic(
    NUS_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  rxChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  rxChar->setCallbacks(new RxCallbacks());

  svc->start();

  BLESecurity* sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  sec->setCapability(ESP_IO_CAP_NONE);
  sec->setKeySize(16);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  // Connection interval (units of 1.25ms). We advertise a LONG preferred
  // interval to cut idle radio wakeups — this is a wearable on battery, and at
  // the old iOS-friendly 7.5-22.5ms the radio woke ~44-130x/sec even when idle,
  // a major drain. 200-500ms means the central polls us far less often. Set via
  // advertised preferred params only (the central honours these at connect
  // time) — NOT a runtime updateConnParams() call, which crashed the BLE stack.
  // Tradeoff: up to ~0.5s extra latency delivering a tool-approval prompt,
  // imperceptible for this use. 0xA0=160=200ms, 0x190=400=500ms.
  adv->setMinPreferred(0xA0);   // 200ms
  adv->setMaxPreferred(0x190);  // 500ms
  BLEDevice::startAdvertising();
  Serial.printf("[ble] advertising as '%s'\n", deviceName);
}

void bleDisconnect() {
  if (server && connected) server->disconnect(server->getConnId());
}

// Stop advertising for deep idle-sleep. Sets idleSilenced so the onDisconnect
// callback won't auto-restart it. The radio goes quiet — no advertisement
// packets — which removes the continuous idle drain.
void bleStopAdvertising() {
  idleSilenced = true;
  BLEDevice::stopAdvertising();
  Serial.println("[ble] advertising stopped (idle sleep)");
}

// Restart advertising on wake. Clears idleSilenced first so a subsequent
// disconnect behaves normally again. NOTE: Arduino-Bluedroid advertising
// restart has historically been unreliable on this device — if reconnect
// proves flaky after sleep, the fallback is a reboot-on-wake (guaranteed fresh
// advertising). Trying the clean restart first.
void bleStartAdvertising() {
  idleSilenced = false;
  wantAdvRestart = false;   // explicit restart supersedes any pending deferred one
  BLEDevice::startAdvertising();
  Serial.println("[ble] advertising restarted (wake)");
}

// Call once per main-loop iteration. Performs the deferred advertising restart
// requested by onDisconnect, safely outside the disconnect-callback context.
void bleService() {
  if (wantAdvRestart && !idleSilenced) {
    wantAdvRestart = false;
    BLEDevice::startAdvertising();
    Serial.println("[ble] advertising restarted (post-disconnect)");
  }
}

bool bleConnected() { return connected; }
bool bleSecure()    { return secure; }
uint32_t blePasskey() { return passkey; }

void bleRemoveCurrentBond() {
  if (!hasPeer) {
    Serial.println("[ble] unpair: no peer to remove");
    return;
  }
  esp_ble_remove_bond_device(peerAddr);
  Serial.printf("[ble] removed bond %02x:%02x:%02x:%02x:%02x:%02x\n",
    peerAddr[0], peerAddr[1], peerAddr[2],
    peerAddr[3], peerAddr[4], peerAddr[5]);
}

void bleClearBonds() {
  int n = esp_ble_get_bond_device_num();
  if (n <= 0) return;
  esp_ble_bond_dev_t* list = (esp_ble_bond_dev_t*)malloc(n * sizeof(esp_ble_bond_dev_t));
  if (!list) return;
  esp_ble_get_bond_device_list(&n, list);
  for (int i = 0; i < n; i++) esp_ble_remove_bond_device(list[i].bd_addr);
  free(list);
  Serial.printf("[ble] cleared %d bond(s)\n", n);
}

size_t bleAvailable() {
  return (rxHead + RX_CAP - rxTail) % RX_CAP;
}

int bleRead() {
  if (rxHead == rxTail) return -1;
  int b = rxBuf[rxTail];
  rxTail = (rxTail + 1) % RX_CAP;
  return b;
}

size_t bleWrite(const uint8_t* data, size_t len) {
  if (!connected || !txChar) return 0;
  // ATT notify payload is limited to (MTU - 3). macOS negotiates 185, so
  // the 182-byte chunk works there; use the live mtu so a peer that caps
  // at the 23-byte default doesn't get truncated notifies.
  size_t chunk = mtu > 3 ? mtu - 3 : 20;
  if (chunk > 180) chunk = 180;
  size_t sent = 0;
  while (sent < len) {
    size_t n = len - sent;
    if (n > chunk) n = chunk;
    txChar->setValue((uint8_t*)(data + sent), n);
    txChar->notify();
    sent += n;
    // Small yield so the BLE stack flushes before the next chunk.
    delay(4);
  }
  return sent;
}
