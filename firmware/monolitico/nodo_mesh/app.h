// GENERADO: editar el rol original y regenerar.
/* IC IV: dos DS18B20 -> ESP-NOW -> sala -> gateway -> AURA.
 * Cola persistente, doble ACK final, primera lectura de corte protegida.
 * Basado en el encadenamiento fijo de aura-firmware; protocolo IC_IV v2.
 * Arduino ESP32 3.x, XIAO_ESP32S3, USB CDC Enabled.
 */
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <Wire.h>
#ifndef ICIV_MESH_RADIO_H
#define ICIV_MESH_RADIO_H
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#ifndef ICIV_MESH_CORE_H
#define ICIV_MESH_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Extension IC_IV del encadenamiento ESP-NOW de aura-firmware. Los tres roles
// deben usar esta version; no confundir sus ACK finales con el ACK por salto v1.
const uint8_t MESH_VERSION = 2;
const size_t MESH_FIFO_CAP = 30;
const int16_t TEMP_INVALIDA = 32767;
enum MeshTipo : uint8_t { TELEMETRIA=1, COMANDO=2, ACK_SALTO=3,
  ACK_GATEWAY=4, ACK_CENTRAL=5, PROBE=6, PROBE_ACK=7, CONFIG_RESULT=8, CONFIG_SNAPSHOT=9 };
enum SampleFlags : uint8_t { VALID=1, BATTERY=2, POWER_FIRST=4, CLOCK_VALID=8, POWER_KNOWN=16 };
#pragma pack(push, 1)
struct Muestra {
  uint8_t id[16];
  uint32_t measuredAt;
  uint32_t cutAt;
  int16_t temp100;
  uint8_t sensor;
  uint8_t flags;
};
struct SensorConfig { uint32_t intervalS; int16_t min100; int16_t max100; };
struct MeshConfig { SensorConfig sensors[2]; uint32_t recoveryS; };
struct ConfigCommand { uint8_t id[16]; uint8_t sensor; SensorConfig config; uint32_t recoveryS; };
struct ConfigResult { ConfigCommand command; uint8_t applied; };
struct NodeSnapshot {
  MeshConfig config;
  uint8_t pending[2];
  uint32_t dropped[2];
  uint8_t powerKnown,onBattery,sensorKnown[2],sensorValid[2];
  uint32_t cutAt;
};
struct TramaMesh {
  uint8_t version, tipo;
  uint8_t origen[6], destino[6];
  uint16_t seq;
  uint8_t largo;
  uint8_t payload[180];
};
struct Journal {
  uint32_t generation;
  MeshConfig config;
  Muestra fifo[2][MESH_FIFO_CAP];
  Muestra first[2];
  uint8_t head[2], count[2], hasFirst[2], firstAcked[2];
  uint32_t dropped[2], missedCuts;
  uint8_t powerKnown, onBattery, capturePending;
  uint32_t cutAt;
  uint8_t lastConfigId[16];
};
#pragma pack(pop)
static_assert(sizeof(Muestra)==28, "Formato de muestra");
static_assert(offsetof(TramaMesh,payload)==17, "Cabecera AURA de 17 bytes");
static_assert(sizeof(Journal)<2048, "Journal compacto: menos de 2 KB");
inline constexpr MeshConfig meshDefaults() { return {{{60,200,600},{300,-2500,-1500}},300}; }
inline constexpr bool validSensorConfig(const SensorConfig& c) {
  return c.intervalS>=5 && c.intervalS<=86400 && c.min100>=-5500 &&
    c.max100<=12500 && c.min100<c.max100;
}
inline constexpr bool validConfig(const MeshConfig& c) {
  return validSensorConfig(c.sensors[0]) && validSensorConfig(c.sensors[1]) &&
    c.recoveryS>=60 && c.recoveryS<=86400;
}
inline constexpr bool sameId(const uint8_t a[16],const uint8_t b[16]) {for(int i=0;i<16;++i)if(a[i]!=b[i])return false;return true;}
inline constexpr bool validSample(const Muestra& s) {
  return s.sensor<2 && !(s.flags & ~31) && ((s.flags&VALID)
    ? s.temp100>=-5500 && s.temp100<=12500 : s.temp100==TEMP_INVALIDA) &&
    ((s.flags&CLOCK_VALID) ? s.measuredAt>=1700000000UL : s.measuredAt==0);
}
inline constexpr void journalInit(Journal& j) {j=Journal{};j.config=meshDefaults();}
inline constexpr bool validJournal(const Journal& j) {
  if (!validConfig(j.config) || j.powerKnown>1 || j.onBattery>1 || j.capturePending>1) return false;
  for (int s=0;s<2;++s) {
    if (j.head[s]>=MESH_FIFO_CAP || j.count[s]>MESH_FIFO_CAP || j.hasFirst[s]>1 || j.firstAcked[s]>1) return false;
    if (j.hasFirst[s] && (!validSample(j.first[s]) || j.first[s].sensor!=s)) return false;
    for (int i=0;i<j.count[s];++i) if (!validSample(j.fifo[s][(j.head[s]+i)%MESH_FIFO_CAP]) || j.fifo[s][(j.head[s]+i)%MESH_FIFO_CAP].sensor!=s) return false;
  }
  return true;
}
inline constexpr void pushSample(Journal& j,const Muestra& m) {
  const int s=m.sensor;
  if (j.count[s]==MESH_FIFO_CAP) { j.head[s]=(j.head[s]+1)%MESH_FIFO_CAP; --j.count[s]; ++j.dropped[s]; }
  j.fifo[s][(j.head[s]+j.count[s])%MESH_FIFO_CAP]=m; ++j.count[s];
}
inline constexpr bool protectFirst(Journal& j,const Muestra& m) {
  const int s=m.sensor;
  if (j.hasFirst[s] && !j.firstAcked[s]) { ++j.missedCuts; return false; }
  j.first[s]=m; j.hasFirst[s]=1; j.firstAcked[s]=0; return true;
}
inline constexpr bool nextSample(const Journal& j,Muestra& out) {
  bool found=false;
  for(int s=0;s<2;++s) {
    const Muestra* m=nullptr;
    if(j.hasFirst[s] && !j.firstAcked[s]) m=&j.first[s];
    else if(j.count[s]) m=&j.fifo[s][j.head[s]];
    if(m && (!found || (m->measuredAt && (!out.measuredAt || m->measuredAt<out.measuredAt)))) {out=*m;found=true;}
  }
  return found;
}
inline constexpr bool acknowledge(Journal& j,const uint8_t id[16]) {
  for(int s=0;s<2;++s) {
    if(j.hasFirst[s] && sameId(j.first[s].id,id)) {j.firstAcked[s]=1;return true;}
    if(j.count[s] && sameId(j.fifo[s][j.head[s]].id,id)) {j.head[s]=(j.head[s]+1)%MESH_FIFO_CAP;--j.count[s];return true;}
  }
  return false; // un ACK atrasado no elimina otra muestra
}
inline constexpr uint32_t crc32(const uint8_t* p,size_t n) {
  uint32_t crc=0xFFFFFFFF;
  for(size_t i=0;i<n;++i) {crc^=p[i];for(int b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xEDB88320:0);}
  return ~crc;
}
inline constexpr bool centralAckAllowed(int http,bool explicitIds,bool idAccepted,bool rejected,bool agreedDurable) {
  return (http==200||http==201) && !rejected && (explicitIds?idAccepted:agreedDurable);
}
inline size_t frameBytes(const TramaMesh& t) {return 17+t.largo;}
inline bool validFrame(const uint8_t* p,int n) {
  if(!p || n<17) return false;
  const TramaMesh* t=reinterpret_cast<const TramaMesh*>(p);
  return t->version==MESH_VERSION && t->tipo>=1 && t->tipo<=9 && t->largo<=180 && n==17+t->largo;
}
inline TramaMesh frame(uint8_t type,const uint8_t origin[6],const uint8_t dest[6],uint16_t seq,const void* payload,size_t size) {
  TramaMesh t={};t.version=MESH_VERSION;t.tipo=type;memcpy(t.origen,origin,6);memcpy(t.destino,dest,6);t.seq=seq;
  if(size<=180){t.largo=size;if(payload && size)memcpy(t.payload,payload,size);}return t;
}
#endif

struct RadioPacket {TramaMesh trama; uint8_t sender[6];};
static QueueHandle_t meshRx=nullptr;
static uint8_t ownMac[6];
static void meshReceive(const esp_now_recv_info_t* info,const uint8_t* data,int n) {
  if(!info || !validFrame(data,n))return;
  RadioPacket p={};memcpy(&p.trama,data,n);memcpy(p.sender,info->src_addr,6);
  // El callback WiFi solo copia a una cola sincronizada; no hace NVS/HTTP/esperas.
  xQueueSend(meshRx,&p,0);
}
inline bool macSet(const uint8_t mac[6]) {for(int i=0;i<6;++i)if(mac[i])return true;return false;}
inline bool addPeer(const uint8_t mac[6]) {
  if(!macSet(mac))return false;if(esp_now_is_peer_exist(mac))return true;
  esp_now_peer_info_t p={};memcpy(p.peer_addr,mac,6);p.channel=0;p.ifidx=WIFI_IF_STA;
  return esp_now_add_peer(&p)==ESP_OK;
}
inline bool radioInit(uint8_t channel,bool hasWifi=false) {
  if(!hasWifi)WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);esp_wifi_get_mac(WIFI_IF_STA,ownMac);
  if(!hasWifi && esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK)return false;
  meshRx=xQueueCreate(16,sizeof(RadioPacket));
  return meshRx && esp_now_init()==ESP_OK && esp_now_register_recv_cb(meshReceive)==ESP_OK;
}
inline bool radioSend(const uint8_t peer[6],const TramaMesh& t) {
  return addPeer(peer) && esp_now_send(peer,reinterpret_cast<const uint8_t*>(&t),frameBytes(t))==ESP_OK;
}
inline bool radioRead(RadioPacket& p) {return xQueueReceive(meshRx,&p,0)==pdTRUE;}
#endif

#ifndef ICIV_MESH_STORAGE_H
#define ICIV_MESH_STORAGE_H
#include <Preferences.h>
#ifndef ICIV_MESH_CORE_H
#define ICIV_MESH_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Extension IC_IV del encadenamiento ESP-NOW de aura-firmware. Los tres roles
// deben usar esta version; no confundir sus ACK finales con el ACK por salto v1.
const uint8_t MESH_VERSION = 2;
const size_t MESH_FIFO_CAP = 30;
const int16_t TEMP_INVALIDA = 32767;
enum MeshTipo : uint8_t { TELEMETRIA=1, COMANDO=2, ACK_SALTO=3,
  ACK_GATEWAY=4, ACK_CENTRAL=5, PROBE=6, PROBE_ACK=7, CONFIG_RESULT=8, CONFIG_SNAPSHOT=9 };
enum SampleFlags : uint8_t { VALID=1, BATTERY=2, POWER_FIRST=4, CLOCK_VALID=8, POWER_KNOWN=16 };
#pragma pack(push, 1)
struct Muestra {
  uint8_t id[16];
  uint32_t measuredAt;
  uint32_t cutAt;
  int16_t temp100;
  uint8_t sensor;
  uint8_t flags;
};
struct SensorConfig { uint32_t intervalS; int16_t min100; int16_t max100; };
struct MeshConfig { SensorConfig sensors[2]; uint32_t recoveryS; };
struct ConfigCommand { uint8_t id[16]; uint8_t sensor; SensorConfig config; uint32_t recoveryS; };
struct ConfigResult { ConfigCommand command; uint8_t applied; };
struct NodeSnapshot {
  MeshConfig config;
  uint8_t pending[2];
  uint32_t dropped[2];
  uint8_t powerKnown,onBattery,sensorKnown[2],sensorValid[2];
  uint32_t cutAt;
};
struct TramaMesh {
  uint8_t version, tipo;
  uint8_t origen[6], destino[6];
  uint16_t seq;
  uint8_t largo;
  uint8_t payload[180];
};
struct Journal {
  uint32_t generation;
  MeshConfig config;
  Muestra fifo[2][MESH_FIFO_CAP];
  Muestra first[2];
  uint8_t head[2], count[2], hasFirst[2], firstAcked[2];
  uint32_t dropped[2], missedCuts;
  uint8_t powerKnown, onBattery, capturePending;
  uint32_t cutAt;
  uint8_t lastConfigId[16];
};
#pragma pack(pop)
static_assert(sizeof(Muestra)==28, "Formato de muestra");
static_assert(offsetof(TramaMesh,payload)==17, "Cabecera AURA de 17 bytes");
static_assert(sizeof(Journal)<2048, "Journal compacto: menos de 2 KB");
inline constexpr MeshConfig meshDefaults() { return {{{60,200,600},{300,-2500,-1500}},300}; }
inline constexpr bool validSensorConfig(const SensorConfig& c) {
  return c.intervalS>=5 && c.intervalS<=86400 && c.min100>=-5500 &&
    c.max100<=12500 && c.min100<c.max100;
}
inline constexpr bool validConfig(const MeshConfig& c) {
  return validSensorConfig(c.sensors[0]) && validSensorConfig(c.sensors[1]) &&
    c.recoveryS>=60 && c.recoveryS<=86400;
}
inline constexpr bool sameId(const uint8_t a[16],const uint8_t b[16]) {for(int i=0;i<16;++i)if(a[i]!=b[i])return false;return true;}
inline constexpr bool validSample(const Muestra& s) {
  return s.sensor<2 && !(s.flags & ~31) && ((s.flags&VALID)
    ? s.temp100>=-5500 && s.temp100<=12500 : s.temp100==TEMP_INVALIDA) &&
    ((s.flags&CLOCK_VALID) ? s.measuredAt>=1700000000UL : s.measuredAt==0);
}
inline constexpr void journalInit(Journal& j) {j=Journal{};j.config=meshDefaults();}
inline constexpr bool validJournal(const Journal& j) {
  if (!validConfig(j.config) || j.powerKnown>1 || j.onBattery>1 || j.capturePending>1) return false;
  for (int s=0;s<2;++s) {
    if (j.head[s]>=MESH_FIFO_CAP || j.count[s]>MESH_FIFO_CAP || j.hasFirst[s]>1 || j.firstAcked[s]>1) return false;
    if (j.hasFirst[s] && (!validSample(j.first[s]) || j.first[s].sensor!=s)) return false;
    for (int i=0;i<j.count[s];++i) if (!validSample(j.fifo[s][(j.head[s]+i)%MESH_FIFO_CAP]) || j.fifo[s][(j.head[s]+i)%MESH_FIFO_CAP].sensor!=s) return false;
  }
  return true;
}
inline constexpr void pushSample(Journal& j,const Muestra& m) {
  const int s=m.sensor;
  if (j.count[s]==MESH_FIFO_CAP) { j.head[s]=(j.head[s]+1)%MESH_FIFO_CAP; --j.count[s]; ++j.dropped[s]; }
  j.fifo[s][(j.head[s]+j.count[s])%MESH_FIFO_CAP]=m; ++j.count[s];
}
inline constexpr bool protectFirst(Journal& j,const Muestra& m) {
  const int s=m.sensor;
  if (j.hasFirst[s] && !j.firstAcked[s]) { ++j.missedCuts; return false; }
  j.first[s]=m; j.hasFirst[s]=1; j.firstAcked[s]=0; return true;
}
inline constexpr bool nextSample(const Journal& j,Muestra& out) {
  bool found=false;
  for(int s=0;s<2;++s) {
    const Muestra* m=nullptr;
    if(j.hasFirst[s] && !j.firstAcked[s]) m=&j.first[s];
    else if(j.count[s]) m=&j.fifo[s][j.head[s]];
    if(m && (!found || (m->measuredAt && (!out.measuredAt || m->measuredAt<out.measuredAt)))) {out=*m;found=true;}
  }
  return found;
}
inline constexpr bool acknowledge(Journal& j,const uint8_t id[16]) {
  for(int s=0;s<2;++s) {
    if(j.hasFirst[s] && sameId(j.first[s].id,id)) {j.firstAcked[s]=1;return true;}
    if(j.count[s] && sameId(j.fifo[s][j.head[s]].id,id)) {j.head[s]=(j.head[s]+1)%MESH_FIFO_CAP;--j.count[s];return true;}
  }
  return false; // un ACK atrasado no elimina otra muestra
}
inline constexpr uint32_t crc32(const uint8_t* p,size_t n) {
  uint32_t crc=0xFFFFFFFF;
  for(size_t i=0;i<n;++i) {crc^=p[i];for(int b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xEDB88320:0);}
  return ~crc;
}
inline constexpr bool centralAckAllowed(int http,bool explicitIds,bool idAccepted,bool rejected,bool agreedDurable) {
  return (http==200||http==201) && !rejected && (explicitIds?idAccepted:agreedDurable);
}
inline size_t frameBytes(const TramaMesh& t) {return 17+t.largo;}
inline bool validFrame(const uint8_t* p,int n) {
  if(!p || n<17) return false;
  const TramaMesh* t=reinterpret_cast<const TramaMesh*>(p);
  return t->version==MESH_VERSION && t->tipo>=1 && t->tipo<=9 && t->largo<=180 && n==17+t->largo;
}
inline TramaMesh frame(uint8_t type,const uint8_t origin[6],const uint8_t dest[6],uint16_t seq,const void* payload,size_t size) {
  TramaMesh t={};t.version=MESH_VERSION;t.tipo=type;memcpy(t.origen,origin,6);memcpy(t.destino,dest,6);t.seq=seq;
  if(size<=180){t.largo=size;if(payload && size)memcpy(t.payload,payload,size);}return t;
}
#endif

struct StoredJournal { uint32_t magic; Journal journal; uint32_t crc; };
class MeshStorage {
  Preferences prefs;
  StoredJournal writeBuffer={}, readBuffer={}; // no ocupar el stack del loop con blobs
  uint8_t slot=0;
  bool readSlot(const char* key,StoredJournal& b) {
    if(prefs.getBytesLength(key)!=sizeof(b) || prefs.getBytes(key,&b,sizeof(b))!=sizeof(b))return false;
    return b.magic==0x49433432 && b.crc==crc32(reinterpret_cast<const uint8_t*>(&b.journal),sizeof(Journal)) && validJournal(b.journal);
  }
public:
  bool begin(Journal& j) {
    if(!prefs.begin("iciv_mesh",false))return false;
    StoredJournal& a=writeBuffer;StoredJournal& b=readBuffer;
    bool va=readSlot("q0",a),vb=readSlot("q1",b);
    if(va || vb) {slot=vb && (!va || int32_t(b.journal.generation-a.journal.generation)>0);j=slot?b.journal:a.journal;return true;}
    // No borrar silenciosamente una cola corrupta ni asumir perdida de datos como normal.
    if(prefs.isKey("q0") || prefs.isKey("q1"))return false;
    journalInit(j);return save(j);
  }
  bool save(Journal& j) {
    if(!validJournal(j))return false;
    StoredJournal& b=writeBuffer;b=StoredJournal{};b.magic=0x49433432;b.journal=j;++b.journal.generation;
    b.crc=crc32(reinterpret_cast<const uint8_t*>(&b.journal),sizeof(Journal));
    uint8_t target=1-slot;const char* key=target?"q1":"q0";
    if(prefs.putBytes(key,&b,sizeof(b))!=sizeof(b))return false;
    StoredJournal& check=readBuffer;if(!readSlot(key,check) || check.journal.generation!=b.journal.generation)return false;
    j.generation=b.journal.generation;slot=target;return true;
  }
};
#endif

#if __has_include("config_local.h")
#include "config_local.h"
#endif
#ifndef ICIV_PARENT_MAC
#define ICIV_PARENT_MAC {0,0,0,0,0,0}
#endif
#ifndef ICIV_GATEWAY_MAC
#define ICIV_GATEWAY_MAC {0,0,0,0,0,0}
#endif
#ifndef ICIV_CHANNEL
#define ICIV_CHANNEL 1
#endif
#ifndef ICIV_FRIDGE_PIN
#define ICIV_FRIDGE_PIN D2
#endif
#ifndef ICIV_FREEZER_PIN
#define ICIV_FREEZER_PIN D3
#endif
#ifndef ICIV_ALARM_LED_PIN
#define ICIV_ALARM_LED_PIN D0
#endif
#ifndef ICIV_OFFLINE_LED_PIN
#define ICIV_OFFLINE_LED_PIN D1
#endif
#ifndef ICIV_POWER_PIN
#define ICIV_POWER_PIN -1
#endif
#ifndef ICIV_POWER_PRESENT_LEVEL
#define ICIV_POWER_PRESENT_LEVEL HIGH
#endif
#ifndef ICIV_POWER_PIN_MODE
#define ICIV_POWER_PIN_MODE INPUT
#endif
#ifndef ICIV_SET_RTC_FROM_BUILD
#define ICIV_SET_RTC_FROM_BUILD 0
#endif
#ifndef ICIV_RTC_UTC_OFFSET_SECONDS
#define ICIV_RTC_UTC_OFFSET_SECONDS 0
#endif
const uint8_t parentMac[6]=ICIV_PARENT_MAC, gatewayMac[6]=ICIV_GATEWAY_MAC;
OneWire wireFridge(ICIV_FRIDGE_PIN), wireFreezer(ICIV_FREEZER_PIN);
DallasTemperature fridge(&wireFridge), freezer(&wireFreezer);
DallasTemperature* probes[2]={&fridge,&freezer};
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0,U8X8_PIN_NONE);
RTC_DS3231 rtc;
bool rtcOk=false,clockOk=false,storageOk=false,radioOk=false,radioRunning=false;
Journal journal;
MeshStorage store;
float latest[2]={NAN,NAN};
bool sensorKnown[2]={false,false},snapshotPending=true;
uint32_t lastRadioSend=0;
uint32_t lastMeasure[2]={0,0}, conversionStart=0,conversionEpoch=0;
uint32_t displayAt=0, nextRecovery=0, cutEpoch=0;
uint8_t dueMask=0;
bool conversion=false, captureCut=false, needsFirst=false, sampleClock=false;
bool battery=false,powerCandidate=false,powerSeen=false;
uint32_t powerChangedAt=0;
enum LinkState {UNKNOWN,ONLINE,GATEWAY_DOWN,CENTRAL_DOWN};
LinkState linkState=UNKNOWN;
Muestra flight;
bool waiting=false, gatewayReceived=false,everGateway=false,scanning=false;
uint8_t attempts=0,scanChannel=1;
uint32_t sentAt=0,scanAt=0;
uint16_t seq=0;

bool commit() {
  if(store.save(journal))return true;
  storageOk=false;waiting=false;linkState=GATEWAY_DOWN;
  Serial.println("[NVS] Fallo: no confirmar ni enviar datos que no quedaron guardados");return false;
}
uint32_t readEpoch() {
  if(!rtcOk || rtc.lostPower()) {clockOk=false;return 0;}
  DateTime t=rtc.now();clockOk=t.isValid() && t.unixtime()>=1700000000UL;
  return clockOk?uint32_t(int64_t(t.unixtime())-ICIV_RTC_UTC_OFFSET_SECONDS):0;
}
void newId(uint8_t id[16]) {esp_fill_random(id,16);id[6]=(id[6]&15)|64;id[8]=(id[8]&63)|128;}
bool outside(int s) {
  return isfinite(latest[s]) && (latest[s]*100<journal.config.sensors[s].min100 || latest[s]*100>journal.config.sensors[s].max100);
}
void updateOutputs() {
  digitalWrite(ICIV_ALARM_LED_PIN,outside(0)||outside(1)?HIGH:LOW);
  digitalWrite(ICIV_OFFLINE_LED_PIN,linkState==GATEWAY_DOWN||linkState==CENTRAL_DOWN||!storageOk?HIGH:LOW);
  // Bateria y caida de comunicacion son estados independientes.
  oled.setPowerSave(battery||linkState==GATEWAY_DOWN||linkState==CENTRAL_DOWN);
}
void readPower(uint32_t now) {
  if(ICIV_POWER_PIN<0)return;
  const bool observed=digitalRead(ICIV_POWER_PIN)!=ICIV_POWER_PRESENT_LEVEL;
  if(!powerSeen || observed!=powerCandidate) {
    powerSeen=true;powerCandidate=observed;powerChangedAt=now;
  }
  if(uint32_t(now-powerChangedAt)<200)return; // antirrebote de entrada
  if(!journal.powerKnown || observed!=bool(journal.onBattery)) {
    const bool transitioned=!journal.powerKnown || !journal.onBattery;
    journal.powerKnown=1;journal.onBattery=observed;battery=observed;
    if(observed && transitioned) {
      cutEpoch=readEpoch();journal.cutAt=cutEpoch;journal.capturePending=1;needsFirst=true;
      Serial.println("[ENERGIA] Bateria: capturar ambas sondas; hora = deteccion del corte");
    }
    if(storageOk)commit();updateOutputs();
  }
}
void beginMeasurements(uint32_t now) {
  if(conversion)return;
  uint8_t mask=needsFirst?3:0;
  for(int s=0;s<2;++s)if(uint32_t(now-lastMeasure[s])>=journal.config.sensors[s].intervalS*1000UL)mask|=1<<s;
  if(!mask)return;
  dueMask=mask;captureCut=needsFirst;needsFirst=false;
  conversionEpoch=captureCut?cutEpoch:readEpoch();sampleClock=conversionEpoch!=0;
  for(int s=0;s<2;++s)if(mask&(1<<s)) {probes[s]->requestTemperatures();lastMeasure[s]=now;}
  conversion=true;conversionStart=now;
}
void finishMeasurements(uint32_t now) {
  if(!conversion || uint32_t(now-conversionStart)<100)return; // 9 bits: 94 ms
  for(int s=0;s<2;++s)if(dueMask&(1<<s)) {
    const float t=probes[s]->getTempCByIndex(0);
    latest[s]=(isfinite(t)&&t>=-55&&t<=125)?t:NAN;
    sensorKnown[s]=true;snapshotPending=true;
    Muestra m={};newId(m.id);m.sensor=s;m.measuredAt=conversionEpoch;
    m.cutAt=journal.cutAt;m.temp100=isfinite(latest[s])?int16_t(lround(latest[s]*100)):TEMP_INVALIDA;
    m.flags=(isfinite(latest[s])?VALID:0)|(battery?BATTERY:0)|(captureCut?POWER_FIRST:0)|(sampleClock?CLOCK_VALID:0)|((ICIV_POWER_PIN>=0 && journal.powerKnown)?POWER_KNOWN:0);
    // No pisar primera muestra no confirmada aunque haya otro corte.
    if(!captureCut || !protectFirst(journal,m))pushSample(journal,m);
    Serial.printf("[MUESTRA] %s %.2f C t=%lu pendientes=%u descartadas=%lu\n",s?"freezer":"heladera",latest[s],(unsigned long)m.measuredAt,journal.count[s],(unsigned long)journal.dropped[s]);
  }
  if(captureCut)journal.capturePending=0;
  conversion=false;if(storageOk)commit();updateOutputs();
}
void sleepRadio() {
  if(radioRunning) {esp_wifi_stop();radioRunning=false;}
}
void wakeRadio() {
  if(!radioRunning && radioOk) {radioRunning=esp_wifi_start()==ESP_OK;}
}
void failDelivery(uint32_t now) {
  linkState=everGateway?CENTRAL_DOWN:GATEWAY_DOWN;waiting=false;scanning=false;
  nextRecovery=now+journal.config.recoveryS*1000UL;sleepRadio();updateOutputs();
  Serial.println(everGateway?"[OFFLINE] Gateway confirmo; falta central":"[OFFLINE] Sin confirmacion del gateway final");
}
void transmit(uint32_t now) {
  gatewayReceived=false;sentAt=now;++attempts;
  TramaMesh t=frame(TELEMETRIA,ownMac,gatewayMac,seq++,&flight,sizeof(flight));
  radioSend(parentMac,t); // Sin ACK final no se elimina de NVS.
  lastRadioSend=now;
}
void receiveMessages(uint32_t now) {
  RadioPacket p;
  while(radioRead(p)) {
    const auto& t=p.trama;
    if(memcmp(p.sender,parentMac,6) || memcmp(t.origen,gatewayMac,6) || memcmp(t.destino,ownMac,6))continue;
    if(t.tipo==PROBE_ACK && scanning && t.largo==0) {
      scanning=false;waiting=true;attempts=0;everGateway=false;transmit(now);continue;
    }
    if(waiting && t.largo==16 && sameId(t.payload,flight.id)) {
      if(t.tipo==ACK_GATEWAY) {gatewayReceived=true;everGateway=true;}
      if(t.tipo==ACK_CENTRAL) {
        acknowledge(journal,flight.id);
        if(!commit())return;
        waiting=false;linkState=ONLINE;attempts=0;nextRecovery=now;snapshotPending=true;updateOutputs();
        Serial.println("[ACK] Central confirmo: muestra persistida; retirar pendiente");
      }
    }
    if(t.tipo==COMANDO && t.largo==sizeof(ConfigCommand)) {
      ConfigCommand cmd;memcpy(&cmd,t.payload,sizeof(cmd));
      ConfigResult result={cmd,0};
      if(cmd.sensor<2 && validSensorConfig(cmd.config) && cmd.recoveryS>=60 && cmd.recoveryS<=86400 && storageOk) {
        Journal old=journal;
        journal.config.sensors[cmd.sensor]=cmd.config;journal.config.recoveryS=cmd.recoveryS;
        memcpy(journal.lastConfigId,cmd.id,16);
        if(commit())result.applied=1;else journal=old;
      }
      if(result.applied)updateOutputs();
      radioSend(parentMac,frame(CONFIG_RESULT,ownMac,gatewayMac,t.seq,&result,sizeof(result)));
      lastRadioSend=now;snapshotPending=true;
    }
  }
}
void sendSnapshot(uint32_t now) {
  if(!snapshotPending || !storageOk || !radioRunning || scanning || uint32_t(now-lastRadioSend)<30)return;
  NodeSnapshot snapshot={};snapshot.config=journal.config;
  snapshot.powerKnown=ICIV_POWER_PIN>=0 && journal.powerKnown;snapshot.onBattery=battery;snapshot.cutAt=journal.cutAt;
  for(int s=0;s<2;++s) {
    snapshot.pending[s]=journal.count[s]+(journal.hasFirst[s]&&!journal.firstAcked[s]?1:0);
    snapshot.dropped[s]=journal.dropped[s];snapshot.sensorKnown[s]=sensorKnown[s];snapshot.sensorValid[s]=isfinite(latest[s]);
  }
  if(radioSend(parentMac,frame(CONFIG_SNAPSHOT,ownMac,gatewayMac,seq++,&snapshot,sizeof(snapshot))))snapshotPending=false;
  lastRadioSend=now;
}
void startScan(uint32_t now) {
  wakeRadio();if(!radioRunning){failDelivery(now);return;}
  scanning=true;scanChannel=1;scanAt=now-500;
}
void serviceNetwork(uint32_t now) {
  if(!storageOk || !radioOk || !macSet(parentMac) || !macSet(gatewayMac))return;
  if(radioRunning)receiveMessages(now);
  if(scanning) {
    if(uint32_t(now-scanAt)>=500) {
      if(scanChannel>11){failDelivery(now);return;}
      esp_wifi_set_channel(scanChannel++,WIFI_SECOND_CHAN_NONE);scanAt=now;
      radioSend(parentMac,frame(PROBE,ownMac,gatewayMac,seq++,nullptr,0));
    }
    return;
  }
  if(waiting) {
    const uint32_t timeout=gatewayReceived?12000:4000;
    if(uint32_t(now-sentAt)>=timeout) {
      const uint8_t budget=(linkState==GATEWAY_DOWN||linkState==CENTRAL_DOWN)?1:4;
      if(attempts>=budget)failDelivery(now);else transmit(now);
    }
    return;
  }
  if(!nextSample(journal,flight))return;
  if(linkState==GATEWAY_DOWN||linkState==CENTRAL_DOWN) {
    if(int32_t(now-nextRecovery)<0)return;
    startScan(now);return;
  }
  waiting=true;everGateway=false;attempts=0;transmit(now);
}
void drawDisplay(uint32_t now) {
  if(uint32_t(now-displayAt)<1000 || battery || linkState==GATEWAY_DOWN || linkState==CENTRAL_DOWN)return;
  displayAt=now;oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);
  char text[28];
  for(int s=0;s<2;++s) {
    if(isfinite(latest[s]))snprintf(text,sizeof(text),"%s %.2f C",s?"Freezer:":"Heladera:",latest[s]);
    else snprintf(text,sizeof(text),"%s ERROR SONDA",s?"Freezer":"Heladera");
    oled.drawStr(0,12+s*12,text);
  }
  snprintf(text,sizeof(text),"Cola H:%u F:%u",journal.count[0],journal.count[1]);oled.drawStr(0,39,text);
  oled.drawStr(0,51,clockOk?"RTC valido":"RTC SIN HORA VALIDA");
  oled.drawStr(0,63,!storageOk?"ERROR NVS":!macSet(parentMac)?"CONFIGURAR MAC":linkState==ONLINE?"Gateway + AURA OK":"Esperando ACK finales");oled.sendBuffer();
}
void setup() {
  Serial.begin(115200);
  pinMode(ICIV_ALARM_LED_PIN,OUTPUT);pinMode(ICIV_OFFLINE_LED_PIN,OUTPUT);
  if(ICIV_POWER_PIN>=0)pinMode(ICIV_POWER_PIN,ICIV_POWER_PIN_MODE);
  Wire.begin(D4,D5);oled.begin();rtcOk=rtc.begin();
  if(rtcOk && rtc.lostPower() && ICIV_SET_RTC_FROM_BUILD)rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));
  clockOk=rtcOk && !rtc.lostPower();
  for(int s=0;s<2;++s) {probes[s]->begin();probes[s]->setResolution(9);probes[s]->setWaitForConversion(false);}
  storageOk=store.begin(journal);if(!storageOk)journalInit(journal);
  needsFirst=journal.capturePending;cutEpoch=journal.cutAt;
  battery=ICIV_POWER_PIN>=0 && journal.powerKnown && journal.onBattery;
  radioOk=radioInit(ICIV_CHANNEL);radioRunning=radioOk;
  if(radioOk)addPeer(parentMac);
  for(int s=0;s<2;++s)lastMeasure[s]=millis()-journal.config.sensors[s].intervalS*1000UL;
  if(!radioOk || !macSet(parentMac)||!macSet(gatewayMac))linkState=GATEWAY_DOWN;
  Serial.printf("[MESH] NVS=%s bytes=%u; MAC %02X:%02X:%02X:%02X:%02X:%02X\n",storageOk?"OK":"ERROR",unsigned(sizeof(Journal)),ownMac[0],ownMac[1],ownMac[2],ownMac[3],ownMac[4],ownMac[5]);
  updateOutputs();
}
void loop() {
  uint32_t now=millis();readPower(now);beginMeasurements(now);finishMeasurements(now);
  serviceNetwork(now);sendSnapshot(now);drawDisplay(now);delay(5);
}
