// Kamay BLE Demo - firmware principal.
//
// Periferico BLE con perfiles GATT estandar del Bluetooth SIG:
//   Environmental Sensing (0x181A)  temperatura y humedad, READ + NOTIFY
//   Battery Service       (0x180F)  nivel de bateria, READ + NOTIFY
//   Device Information    (0x180A)  fabricante, modelo, firmware, hardware
//
// Pensado para la demo Web Bluetooth de https://kamaytech.com.co/lab/ y para
// verificarse con nRF Connect. Sin emparejamiento ni bonding: lectura y
// notificacion planas. Los valores se refrescan cada segundo.
//
// Stack: Arduino (ESP32 core 2.0.x) + NimBLE-Arduino 2.x.

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "sensors.h"
#include "battery.h"

#ifndef KAMAY_FW_VERSION
#define KAMAY_FW_VERSION "0.0.0-dev"
#endif
#ifndef KAMAY_BOARD
#define KAMAY_BOARD "unknown"
#endif

// Servicio GAP del host NimBLE: fija la apariencia (caracteristica 0x2A01).
// Se declara aqui para no depender de la ruta interna del header.
extern "C" int ble_svc_gap_device_appearance_set(uint16_t appearance);

// --- UUIDs de 16 bits asignados por el Bluetooth SIG ---------------------
static const uint16_t kSvcEnvironmental = 0x181A;
static const uint16_t kChrTemperature   = 0x2A6E;  // sint16 LE, 0.01 C, 0x8000 = desconocido
static const uint16_t kChrHumidity      = 0x2A6F;  // uint16 LE, 0.01 %, 0xFFFF = desconocido
static const uint16_t kSvcBattery       = 0x180F;
static const uint16_t kChrBatteryLevel  = 0x2A19;  // uint8, %
static const uint16_t kSvcDeviceInfo    = 0x180A;
static const uint16_t kChrManufacturer  = 0x2A29;
static const uint16_t kChrModelNumber   = 0x2A24;
static const uint16_t kChrFirmwareRev   = 0x2A26;
static const uint16_t kChrHardwareRev   = 0x2A27;
// A proposito NO se expone Serial Number String (0x2A25): Web Bluetooth la bloquea.

static const uint16_t kAppearance     = 0x0540;  // Generic Sensor
static const uint16_t kAdvMinInterval = 160;     // 100 ms (unidades de 0.625 ms)
static const uint16_t kAdvMaxInterval = 240;     // 150 ms
static const uint32_t kTickMs         = 1000;

static const int16_t  kTempUnknown = (int16_t)0x8000;
static const uint16_t kHumUnknown  = 0xFFFF;

// --- Estado ---------------------------------------------------------------
static NimBLEServer*         s_server  = nullptr;
static NimBLECharacteristic* s_chrTemp = nullptr;
static NimBLECharacteristic* s_chrHum  = nullptr;
static NimBLECharacteristic* s_chrBat  = nullptr;
static std::string           s_name;
static std::string           s_mac;
static uint32_t              s_tick    = 0;

// --- Callbacks de conexion ------------------------------------------------
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.printf("[ble] conectado    peer=%s handle=%u clientes=%u\n",
                      connInfo.getAddress().toString().c_str(),
                      connInfo.getConnHandle(), pServer->getConnectedCount());
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("[ble] desconectado peer=%s razon=0x%02X clientes=%u; sigo anunciando\n",
                      connInfo.getAddress().toString().c_str(), reason,
                      pServer->getConnectedCount());
        // advertiseOnDisconnect(true) reinicia el advertising solo; loop() lo vigila.
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo& connInfo) override {
        Serial.printf("[ble] MTU=%u handle=%u\n", mtu, connInfo.getConnHandle());
    }
};

// --- Utilidades -----------------------------------------------------------
static void printHex(const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); i++) {
        Serial.printf("%s%02X", i ? " " : "", bytes[i]);
    }
}

static void setU16(NimBLECharacteristic* chr, uint16_t raw) {
    uint8_t b[2] = { (uint8_t)(raw & 0xFF), (uint8_t)(raw >> 8) };  // little-endian
    chr->setValue(b, sizeof(b));
}

static void setU8(NimBLECharacteristic* chr, uint8_t raw) {
    chr->setValue(&raw, 1);
}

// --- GATT -----------------------------------------------------------------
static void setupGatt() {
    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(new ServerCallbacks());
    s_server->advertiseOnDisconnect(true);

    // Environmental Sensing
    NimBLEService* ess = s_server->createService(NimBLEUUID(kSvcEnvironmental));
    s_chrTemp = ess->createCharacteristic(NimBLEUUID(kChrTemperature),
                                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_chrHum  = ess->createCharacteristic(NimBLEUUID(kChrHumidity),
                                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    setU16(s_chrTemp, (uint16_t)kTempUnknown);
    setU16(s_chrHum, kHumUnknown);
    ess->start();

    // Battery
    NimBLEService* bas = s_server->createService(NimBLEUUID(kSvcBattery));
    s_chrBat = bas->createCharacteristic(NimBLEUUID(kChrBatteryLevel),
                                         NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    setU8(s_chrBat, kamayBatteryRead());
    bas->start();

    // Device Information (solo lectura, valores fijos)
    NimBLEService* dis = s_server->createService(NimBLEUUID(kSvcDeviceInfo));
    dis->createCharacteristic(NimBLEUUID(kChrManufacturer), NIMBLE_PROPERTY::READ)->setValue("KamayTech");
    dis->createCharacteristic(NimBLEUUID(kChrModelNumber),  NIMBLE_PROPERTY::READ)->setValue("BLE-DEMO");
    dis->createCharacteristic(NimBLEUUID(kChrFirmwareRev),  NIMBLE_PROPERTY::READ)->setValue(KAMAY_FW_VERSION);
    dis->createCharacteristic(NimBLEUUID(kChrHardwareRev),  NIMBLE_PROPERTY::READ)->setValue(KAMAY_BOARD);
    dis->start();
}

// --- Advertising ----------------------------------------------------------
// Todo va en el paquete de advertising (28 de 31 bytes), sin scan response:
//   02 01 06              Flags: LE General Discoverable, BR/EDR no soportado
//   05 03 1A 18 0F 18     Lista completa de UUIDs de 16 bits: 0x181A, 0x180F
//   03 19 40 05           Appearance: 0x0540 Generic Sensor
//   0E 09 4B..58 58       Nombre completo: "Kamay-Demo-XX"
static void setupAdvertising() {
    NimBLEAdvertisementData advData;
    bool ok = true;
    ok &= advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    ok &= advData.setCompleteServices16({ NimBLEUUID(kSvcEnvironmental), NimBLEUUID(kSvcBattery) });
    ok &= advData.setAppearance(kAppearance);
    ok &= advData.setName(s_name);

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    ok &= adv->setAdvertisementData(advData);
    adv->enableScanResponse(false);
    adv->setMinInterval(kAdvMinInterval);
    adv->setMaxInterval(kAdvMaxInterval);
    ok &= adv->start();

    std::vector<uint8_t> payload = advData.getPayload();
    Serial.printf("[ble] adv (%u bytes): ", (unsigned)payload.size());
    printHex(payload);
    Serial.println();
    Serial.printf("[ble] anunciando cada %u-%u ms%s\n",
                  (unsigned)(kAdvMinInterval * 625 / 1000), (unsigned)(kAdvMaxInterval * 625 / 1000),
                  ok ? "" : " (ADVERTENCIA: algun paso del advertising fallo)");
}

// --- Tick de 1 s ----------------------------------------------------------
static void tick() {
    s_tick++;

    KamayReading r   = kamaySensorRead();
    int16_t      tRaw = r.tempValid ? (int16_t)lroundf(r.tempC * 100.0f)  : kTempUnknown;
    uint16_t     hRaw = r.humValid  ? (uint16_t)lroundf(r.humPct * 100.0f) : kHumUnknown;
    uint8_t      bat  = kamayBatteryRead();

    // setValue siempre: un cliente que haga readValue() ve datos frescos aunque
    // no este suscrito a notificaciones.
    setU16(s_chrTemp, (uint16_t)tRaw);
    setU16(s_chrHum, hRaw);
    setU8(s_chrBat, bat);

    bool connected = s_server->getConnectedCount() > 0;
    if (connected) {
        // notify() solo envia a los clientes que activaron el CCCD.
        s_chrTemp->notify();
        s_chrHum->notify();
        s_chrBat->notify();
    }

    char tStr[16], hStr[16];
    if (r.tempValid) snprintf(tStr, sizeof(tStr), "%.2fC", r.tempC);  else strcpy(tStr, "?");
    if (r.humValid)  snprintf(hStr, sizeof(hStr), "%.2f%%", r.humPct); else strcpy(hStr, "?");
    Serial.printf("[tick %lu] temp=%s hum=%s bat=%u%%", (unsigned long)s_tick, tStr, hStr, bat);
    if (kamayBatteryMilliVolts() >= 0) Serial.printf(" (%d mV)", kamayBatteryMilliVolts());
    Serial.printf(" clientes=%u%s\n", s_server->getConnectedCount(), connected ? " notify" : "");
}

// --- Arduino --------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("[boot] Kamay BLE Demo v" KAMAY_FW_VERSION " placa=" KAMAY_BOARD);

    kamaySensorBegin();
    kamayBatteryBegin();
    Serial.printf("[boot] sensor=%s bateria=%s\n", kamaySensorName(), kamayBatteryMode());

    // Nombre provisional; el definitivo lleva el ultimo byte de la MAC.
    NimBLEDevice::init("Kamay-Demo");
    NimBLEDevice::setSecurityAuth(false, false, false);  // sin bonding, sin MITM, sin SC

    s_mac = NimBLEDevice::getAddress().toString();       // "aa:bb:cc:dd:ee:ff"
    for (auto& c : s_mac) c = (char)toupper((unsigned char)c);
    s_name = "Kamay-Demo-" + s_mac.substr(s_mac.size() - 2);
    NimBLEDevice::setDeviceName(s_name);
    ble_svc_gap_device_appearance_set(kAppearance);
    Serial.printf("[ble] nombre=%s mac=%s\n", s_name.c_str(), s_mac.c_str());

    setupGatt();
    setupAdvertising();
}

void loop() {
    static uint32_t lastTick = 0;
    uint32_t now = millis();
    if (now - lastTick >= kTickMs) {
        lastTick = now;
        tick();

        // Red de seguridad: si no hay clientes y no se esta anunciando, reanudar.
        if (s_server->getConnectedCount() == 0 && !NimBLEDevice::getAdvertising()->isAdvertising()) {
            Serial.println("[ble] advertising detenido; reanudando");
            NimBLEDevice::startAdvertising();
        }
    }
    delay(10);
}
