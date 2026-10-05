// GENERADO: editar el rol original y regenerar.
/* IC IV: dos DS18B20 -> ESP-NOW -> sala -> gateway -> AURA.
 * Cola persistente, doble ACK final, primera lectura de corte protegida.
 * Basado en el encadenamiento fijo de aura-firmware; protocolo IC_IV v3.
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
const uint8_t MESH_VERSION = 3;
const size_t MESH_FIFO_CAP = 30;
const int16_t TEMP_INVALIDA = 32767;
enum MeshTipo : uint8_t { TELEMETRIA=1, COMANDO=2, ACK_SALTO=3,
  ACK_GATEWAY=4, ACK_CENTRAL=5, PROBE=6, PROBE_ACK=7, CONFIG_RESULT=8, CONFIG_SNAPSHOT=9, ACK_ALERT=10 };
enum RecordKind : uint8_t { MEDICION=0, ALERTA_SENSOR=1, ALERTA_ENERGIA=2 };
enum SensorState : uint8_t { SONDA_OK=0, SIN_RESPUESTA=1, FUERA_RANGO=2 };
enum SampleFlags : uint8_t { VALID=1, BATTERY=2, POWER_FIRST=4, CLOCK_VALID=8, POWER_KNOWN=16 };
#pragma pack(push, 1)
struct Muestra {
  uint8_t id[16];
  uint32_t measuredAt;
  uint32_t cutAt;
  int16_t temp100;
  uint8_t sensor;
  uint8_t flags;
  uint8_t kind, state;
};
struct SensorConfig { uint32_t intervalS; int16_t min100; int16_t max100; };
struct MeshConfig { SensorConfig sensors[2]; uint32_t recoveryS; };
struct ConfigCommand { char id[65]; uint16_t mask; MeshConfig config; };
struct ConfigResult { ConfigCommand command; uint8_t applied; };
struct NodeSnapshot {
  MeshConfig config;
  uint8_t pending[2];
  uint32_t dropped[2];
  uint8_t powerKnown,onBattery,sensorKnown[2],sensorValid[2];
  uint32_t cutAt;
  uint8_t sensorState[2];
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
  uint8_t sensorKnown[2],sensorState[2];
  Muestra firstPower;
  uint8_t hasPower,powerAcked;
};
#pragma pack(pop)
static_assert(sizeof(Muestra)==30, "Formato de muestra v3");
static_assert(sizeof(ConfigResult)<=180, "Configuracion dentro del payload ESP-NOW");
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
  return s.sensor<2 && s.kind<=ALERTA_ENERGIA && s.state<=2 && !(s.flags & ~31) && (s.kind==MEDICION
    ? (s.flags&VALID) && s.temp100>=-5500 && s.temp100<=12500 && s.temp100!=8500
    : !(s.flags&VALID) && s.temp100==TEMP_INVALIDA) &&
    ((s.flags&CLOCK_VALID) ? s.measuredAt>=1700000000UL : s.measuredAt==0);
}
inline constexpr void journalInit(Journal& j) {j=Journal{};j.config=meshDefaults();}
inline constexpr bool validJournal(const Journal& j) {
  if (!validConfig(j.config) || j.powerKnown>1 || j.onBattery>1 || j.capturePending>1) return false;
  if(j.hasPower>1||j.powerAcked>1||(j.hasPower && (!validSample(j.firstPower)||j.firstPower.kind!=ALERTA_ENERGIA)))return false;
  for (int s=0;s<2;++s) {
    if(j.sensorKnown[s]>1 || j.sensorState[s]>2)return false;
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
  bool found=j.hasPower&&!j.powerAcked;if(found)out=j.firstPower;
  for(int s=0;s<2;++s) {
    const Muestra* m=nullptr;
    if(j.hasFirst[s] && !j.firstAcked[s]) m=&j.first[s];
    else if(j.count[s]) m=&j.fifo[s][j.head[s]];
    if(m && (!found || (m->measuredAt && (!out.measuredAt || m->measuredAt<out.measuredAt)))) {out=*m;found=true;}
  }
  return found;
}
inline constexpr bool acknowledge(Journal& j,const uint8_t id[16]) {
  if(j.hasPower && sameId(j.firstPower.id,id)){j.powerAcked=1;return true;}
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
inline constexpr bool centralAckAllowed(int http,int inserted,int duplicates,int errors,int sent=1) {
  return http==201 && sent==1 && inserted>=0 && duplicates>=0 && errors==0 && inserted<=1 && duplicates<=1 && inserted+duplicates==sent;
}
inline constexpr bool applyPatch(const MeshConfig& current,const ConfigCommand& cmd,MeshConfig& next) {
  if(!cmd.mask || (cmd.mask&~127))return false;
  next=current;
  if(cmd.mask&1)next.sensors[0].intervalS=cmd.config.sensors[0].intervalS;
  if(cmd.mask&2)next.sensors[1].intervalS=cmd.config.sensors[1].intervalS;
  if(cmd.mask&4)next.sensors[0].min100=cmd.config.sensors[0].min100;
  if(cmd.mask&8)next.sensors[0].max100=cmd.config.sensors[0].max100;
  if(cmd.mask&16)next.sensors[1].min100=cmd.config.sensors[1].min100;
  if(cmd.mask&32)next.sensors[1].max100=cmd.config.sensors[1].max100;
  if(cmd.mask&64)next.recoveryS=cmd.config.recoveryS;
  return validConfig(next);
}
inline constexpr uint8_t classifyTemperature(float t,bool conversionReady) {
  return !conversionReady || t!=t || t==-127?SIN_RESPUESTA:(t<-55||t>125||t==85)?FUERA_RANGO:SONDA_OK;
}
inline size_t frameBytes(const TramaMesh& t) {return 17+t.largo;}
inline bool validFrame(const uint8_t* p,int n) {
  if(!p || n<17) return false;
  const TramaMesh* t=reinterpret_cast<const TramaMesh*>(p);
  return t->version==MESH_VERSION && t->tipo>=1 && t->tipo<=10 && t->largo<=180 && n==17+t->largo;
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
const uint8_t MESH_VERSION = 3;
const size_t MESH_FIFO_CAP = 30;
const int16_t TEMP_INVALIDA = 32767;
enum MeshTipo : uint8_t { TELEMETRIA=1, COMANDO=2, ACK_SALTO=3,
  ACK_GATEWAY=4, ACK_CENTRAL=5, PROBE=6, PROBE_ACK=7, CONFIG_RESULT=8, CONFIG_SNAPSHOT=9, ACK_ALERT=10 };
enum RecordKind : uint8_t { MEDICION=0, ALERTA_SENSOR=1, ALERTA_ENERGIA=2 };
enum SensorState : uint8_t { SONDA_OK=0, SIN_RESPUESTA=1, FUERA_RANGO=2 };
enum SampleFlags : uint8_t { VALID=1, BATTERY=2, POWER_FIRST=4, CLOCK_VALID=8, POWER_KNOWN=16 };
#pragma pack(push, 1)
struct Muestra {
  uint8_t id[16];
  uint32_t measuredAt;
  uint32_t cutAt;
  int16_t temp100;
  uint8_t sensor;
  uint8_t flags;
  uint8_t kind, state;
};
struct SensorConfig { uint32_t intervalS; int16_t min100; int16_t max100; };
struct MeshConfig { SensorConfig sensors[2]; uint32_t recoveryS; };
struct ConfigCommand { char id[65]; uint16_t mask; MeshConfig config; };
struct ConfigResult { ConfigCommand command; uint8_t applied; };
struct NodeSnapshot {
  MeshConfig config;
  uint8_t pending[2];
  uint32_t dropped[2];
  uint8_t powerKnown,onBattery,sensorKnown[2],sensorValid[2];
  uint32_t cutAt;
  uint8_t sensorState[2];
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
  uint8_t sensorKnown[2],sensorState[2];
  Muestra firstPower;
  uint8_t hasPower,powerAcked;
};
#pragma pack(pop)
static_assert(sizeof(Muestra)==30, "Formato de muestra v3");
static_assert(sizeof(ConfigResult)<=180, "Configuracion dentro del payload ESP-NOW");
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
  return s.sensor<2 && s.kind<=ALERTA_ENERGIA && s.state<=2 && !(s.flags & ~31) && (s.kind==MEDICION
    ? (s.flags&VALID) && s.temp100>=-5500 && s.temp100<=12500 && s.temp100!=8500
    : !(s.flags&VALID) && s.temp100==TEMP_INVALIDA) &&
    ((s.flags&CLOCK_VALID) ? s.measuredAt>=1700000000UL : s.measuredAt==0);
}
inline constexpr void journalInit(Journal& j) {j=Journal{};j.config=meshDefaults();}
inline constexpr bool validJournal(const Journal& j) {
  if (!validConfig(j.config) || j.powerKnown>1 || j.onBattery>1 || j.capturePending>1) return false;
  if(j.hasPower>1||j.powerAcked>1||(j.hasPower && (!validSample(j.firstPower)||j.firstPower.kind!=ALERTA_ENERGIA)))return false;
  for (int s=0;s<2;++s) {
    if(j.sensorKnown[s]>1 || j.sensorState[s]>2)return false;
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
  bool found=j.hasPower&&!j.powerAcked;if(found)out=j.firstPower;
  for(int s=0;s<2;++s) {
    const Muestra* m=nullptr;
    if(j.hasFirst[s] && !j.firstAcked[s]) m=&j.first[s];
    else if(j.count[s]) m=&j.fifo[s][j.head[s]];
    if(m && (!found || (m->measuredAt && (!out.measuredAt || m->measuredAt<out.measuredAt)))) {out=*m;found=true;}
  }
  return found;
}
inline constexpr bool acknowledge(Journal& j,const uint8_t id[16]) {
  if(j.hasPower && sameId(j.firstPower.id,id)){j.powerAcked=1;return true;}
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
inline constexpr bool centralAckAllowed(int http,int inserted,int duplicates,int errors,int sent=1) {
  return http==201 && sent==1 && inserted>=0 && duplicates>=0 && errors==0 && inserted<=1 && duplicates<=1 && inserted+duplicates==sent;
}
inline constexpr bool applyPatch(const MeshConfig& current,const ConfigCommand& cmd,MeshConfig& next) {
  if(!cmd.mask || (cmd.mask&~127))return false;
  next=current;
  if(cmd.mask&1)next.sensors[0].intervalS=cmd.config.sensors[0].intervalS;
  if(cmd.mask&2)next.sensors[1].intervalS=cmd.config.sensors[1].intervalS;
  if(cmd.mask&4)next.sensors[0].min100=cmd.config.sensors[0].min100;
  if(cmd.mask&8)next.sensors[0].max100=cmd.config.sensors[0].max100;
  if(cmd.mask&16)next.sensors[1].min100=cmd.config.sensors[1].min100;
  if(cmd.mask&32)next.sensors[1].max100=cmd.config.sensors[1].max100;
  if(cmd.mask&64)next.recoveryS=cmd.config.recoveryS;
  return validConfig(next);
}
inline constexpr uint8_t classifyTemperature(float t,bool conversionReady) {
  return !conversionReady || t!=t || t==-127?SIN_RESPUESTA:(t<-55||t>125||t==85)?FUERA_RANGO:SONDA_OK;
}
inline size_t frameBytes(const TramaMesh& t) {return 17+t.largo;}
inline bool validFrame(const uint8_t* p,int n) {
  if(!p || n<17) return false;
  const TramaMesh* t=reinterpret_cast<const TramaMesh*>(p);
  return t->version==MESH_VERSION && t->tipo>=1 && t->tipo<=10 && t->largo<=180 && n==17+t->largo;
}
inline TramaMesh frame(uint8_t type,const uint8_t origin[6],const uint8_t dest[6],uint16_t seq,const void* payload,size_t size) {
  TramaMesh t={};t.version=MESH_VERSION;t.tipo=type;memcpy(t.origen,origin,6);memcpy(t.destino,dest,6);t.seq=seq;
  if(size<=180){t.largo=size;if(payload && size)memcpy(t.payload,payload,size);}return t;
}
#endif

// Dos bancos por registro: escribir banco inactivo y despues confirmar indice.
// Un corte antes del indice no modifica la cola que sigue referenciada.
#pragma pack(push,1)
struct QueueIndex {
  uint32_t magic,generation;
  MeshConfig config;
  uint8_t head[2],count[2],hasFirst[2],firstAcked[2];
  uint32_t dropped[2],missedCuts,cutAt;
  uint8_t powerKnown,onBattery,capturePending,lastConfigId[16];
  uint8_t sensorKnown[2],sensorState[2];
  uint8_t hasPower,powerAcked;
  uint64_t banks;
  uint32_t recordCrc[63];
  uint32_t crc;
};
struct StoredRecord {Muestra sample;uint32_t crc;};
#pragma pack(pop)
static_assert(sizeof(QueueIndex)<360,"Indice menor que el journal");
class MeshStorage {
  Preferences prefs;
  QueueIndex active={},candidate={};
  Journal previous={};
  uint8_t slot=0;
  bool activeSlot(const Journal& j,int s,int pos) {
    for(int i=0;i<j.count[s];++i)if((j.head[s]+i)%30==pos)return true;
    return false;
  }
  void key(char out[12],int record,bool bank){snprintf(out,12,"r%02d%c",record,bank?'b':'a');}
  bool validIndex(QueueIndex& index) {
    return index.magic==0x49433333 && index.crc==crc32(reinterpret_cast<uint8_t*>(&index),offsetof(QueueIndex,crc));
  }
  bool load(int n,QueueIndex& index,Journal& j) {
    const char* name=n?"index1":"index0";
    if(prefs.getBytesLength(name)!=sizeof(index)||prefs.getBytes(name,&index,sizeof(index))!=sizeof(index)||!validIndex(index))return false;
    journalInit(j);j.generation=index.generation;j.config=index.config;
    memcpy(j.head,index.head,2);memcpy(j.count,index.count,2);memcpy(j.hasFirst,index.hasFirst,2);memcpy(j.firstAcked,index.firstAcked,2);
    memcpy(j.dropped,index.dropped,8);j.missedCuts=index.missedCuts;j.cutAt=index.cutAt;
    j.powerKnown=index.powerKnown;j.onBattery=index.onBattery;j.capturePending=index.capturePending;memcpy(j.lastConfigId,index.lastConfigId,16);memcpy(j.sensorKnown,index.sensorKnown,2);memcpy(j.sensorState,index.sensorState,2);
    if(!validConfig(j.config)||j.count[0]>30||j.count[1]>30||j.head[0]>=30||j.head[1]>=30)return false;
    for(int s=0;s<2;++s)for(int i=0;i<31;++i) {
      const bool first=i==30;if(first?!j.hasFirst[s]:!activeSlot(j,s,i))continue;
      const int r=s*31+i;char name[12];key(name,r,(index.banks>>r)&1);StoredRecord b={};
      if(prefs.getBytesLength(name)!=sizeof(b)||prefs.getBytes(name,&b,sizeof(b))!=sizeof(b))return false;
      if(b.crc!=index.recordCrc[r]||b.crc!=crc32(reinterpret_cast<uint8_t*>(&b.sample),sizeof(Muestra))||!validSample(b.sample)||b.sample.sensor!=s)return false;
      if(first)j.first[s]=b.sample;else j.fifo[s][i]=b.sample;
    }
    j.hasPower=index.hasPower;j.powerAcked=index.powerAcked;
    if(j.hasPower) {
      char name[12];key(name,62,(index.banks>>62)&1);StoredRecord b={};
      if(prefs.getBytesLength(name)!=sizeof(b)||prefs.getBytes(name,&b,sizeof(b))!=sizeof(b)||b.crc!=index.recordCrc[62]||b.crc!=crc32(reinterpret_cast<uint8_t*>(&b.sample),sizeof(Muestra)))return false;
      j.firstPower=b.sample;
    }
    return validJournal(j);
  }
public:
  bool begin(Journal& j) {
    if(!prefs.begin("iciv_mesh3",false))return false;
    bool a=load(0,active,j),b=load(1,candidate,previous);
    if(a||b) {
      slot=b&&(!a||int32_t(candidate.generation-active.generation)>0);
      if(slot){active=candidate;j=previous;}previous=j;return true;
    }
    if(prefs.isKey("index0")||prefs.isKey("index1"))return false;
    // No borrar la cola anterior al cambiar de formato.
    Preferences legacy;
    if(legacy.begin("iciv_mesh",true)) {
      bool exists=legacy.isKey("q0")||legacy.isKey("q1");legacy.end();
      if(exists){Serial.println("[MIGRACION] NVS v2 presente: exportar/vaciar y revisar antes de v3; no se borro");return false;}
    }
    journalInit(j);previous=j;return save(j);
  }
  bool save(Journal& j) {
    if(!validJournal(j))return false;
    candidate=active;candidate.magic=0x49433333;candidate.generation=j.generation+1;candidate.config=j.config;
    memcpy(candidate.head,j.head,2);memcpy(candidate.count,j.count,2);memcpy(candidate.hasFirst,j.hasFirst,2);memcpy(candidate.firstAcked,j.firstAcked,2);
    memcpy(candidate.dropped,j.dropped,8);candidate.missedCuts=j.missedCuts;candidate.cutAt=j.cutAt;
    candidate.powerKnown=j.powerKnown;candidate.onBattery=j.onBattery;candidate.capturePending=j.capturePending;memcpy(candidate.lastConfigId,j.lastConfigId,16);memcpy(candidate.sensorKnown,j.sensorKnown,2);memcpy(candidate.sensorState,j.sensorState,2);
    for(int s=0;s<2;++s)for(int i=0;i<31;++i) {
      bool first=i==30;if(first?!j.hasFirst[s]:!activeSlot(j,s,i))continue;
      const Muestra& m=first?j.first[s]:j.fifo[s][i];const Muestra& old=first?previous.first[s]:previous.fifo[s][i];
      bool oldActive=first?previous.hasFirst[s]:activeSlot(previous,s,i);
      int r=s*31+i;
      if(oldActive && !memcmp(&m,&old,sizeof(m)))continue;
      bool bank=!((active.banks>>r)&1);char name[12];key(name,r,bank);
      StoredRecord record={m,crc32(reinterpret_cast<const uint8_t*>(&m),sizeof(m))};
      if(prefs.putBytes(name,&record,sizeof(record))!=sizeof(record))return false;
      StoredRecord check={};if(prefs.getBytes(name,&check,sizeof(check))!=sizeof(check)||memcmp(&record,&check,sizeof(record)))return false;
      candidate.banks=(candidate.banks&~(uint64_t(1)<<r))|(uint64_t(bank)<<r);candidate.recordCrc[r]=record.crc;
    }
    candidate.hasPower=j.hasPower;candidate.powerAcked=j.powerAcked;
    if(j.hasPower && (!previous.hasPower || memcmp(&j.firstPower,&previous.firstPower,sizeof(Muestra)))) {
      bool bank=!((active.banks>>62)&1);char name[12];key(name,62,bank);
      StoredRecord b={j.firstPower,crc32(reinterpret_cast<const uint8_t*>(&j.firstPower),sizeof(Muestra))};
      if(prefs.putBytes(name,&b,sizeof(b))!=sizeof(b))return false;
      StoredRecord check={};if(prefs.getBytes(name,&check,sizeof(check))!=sizeof(check)||memcmp(&b,&check,sizeof(b)))return false;
      candidate.banks=(candidate.banks&~(uint64_t(1)<<62))|(uint64_t(bank)<<62);candidate.recordCrc[62]=b.crc;
    }
    candidate.crc=crc32(reinterpret_cast<const uint8_t*>(&candidate),offsetof(QueueIndex,crc));
    const char* name=slot?"index0":"index1";
    if(prefs.putBytes(name,&candidate,sizeof(candidate))!=sizeof(candidate))return false;
    QueueIndex check={};if(prefs.getBytes(name,&check,sizeof(check))!=sizeof(check)||memcmp(&candidate,&check,sizeof(check)))return false;
    slot=1-slot;active=candidate;j.generation=candidate.generation;previous=j;return true;
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
bool sensorKnown[2]={false,false},snapshotPending=true,forceSnapshot=true;
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
uint8_t listeningChannel=ICIV_CHANNEL;
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
    journal.powerKnown=1;journal.onBattery=observed;battery=observed;snapshotPending=true;
    Muestra event={};newId(event.id);event.kind=ALERTA_ENERGIA;event.state=observed;
    event.sensor=0;event.temp100=TEMP_INVALIDA;event.measuredAt=readEpoch();
    event.flags=POWER_KNOWN|(observed?BATTERY:0)|(event.measuredAt?CLOCK_VALID:0);
    if(observed && (!journal.hasPower || journal.powerAcked)) {journal.firstPower=event;journal.hasPower=1;journal.powerAcked=0;}
    else pushSample(journal,event);
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
  if(!conversion || uint32_t(now-conversionStart)<100)return;
  for(int s=0;s<2;++s)if(dueMask&(1<<s)) {
    const bool ready=probes[s]->isConversionComplete();
    const float t=ready?probes[s]->getTempCByIndex(0):NAN;
    uint8_t state=classifyTemperature(t,ready);
    bool changed=!journal.sensorKnown[s] || journal.sensorState[s]!=state;
    bool recovered=journal.sensorKnown[s] && journal.sensorState[s]!=SONDA_OK && state==SONDA_OK;
    latest[s]=state==SONDA_OK?t:NAN;sensorKnown[s]=true;snapshotPending=true;
    journal.sensorKnown[s]=1;journal.sensorState[s]=state;
    Muestra m={};newId(m.id);m.sensor=s;m.measuredAt=conversionEpoch;m.cutAt=journal.cutAt;
    m.temp100=state==SONDA_OK?int16_t(lround(t*100)):TEMP_INVALIDA;
    m.flags=(state==SONDA_OK?VALID:0)|(battery?BATTERY:0)|(captureCut?POWER_FIRST:0)|(sampleClock?CLOCK_VALID:0)|((ICIV_POWER_PIN>=0&&journal.powerKnown)?POWER_KNOWN:0);
    if(state!=SONDA_OK){m.kind=ALERTA_SENSOR;m.state=state;}
    if(state==SONDA_OK || changed) {
      if(!captureCut || !protectFirst(journal,m))pushSample(journal,m);
    }
    if(recovered) {
      Muestra alert=m;newId(alert.id);alert.kind=ALERTA_SENSOR;alert.state=SONDA_OK;
      alert.flags&=~(VALID|POWER_FIRST);alert.temp100=TEMP_INVALIDA;pushSample(journal,alert);
    }
    Serial.printf("[MUESTRA] %s %.2f C estado=%u t=%lu pendientes=%u descartadas=%lu\n",s?"freezer":"heladera",latest[s],state,(unsigned long)m.measuredAt,journal.count[s],(unsigned long)journal.dropped[s]);
  }
  if(captureCut)journal.capturePending=0;
  conversion=false;if(storageOk)commit();updateOutputs();
}
void wakeRadio() {
  if(!radioRunning && radioOk) {radioRunning=esp_wifi_start()==ESP_OK;}
}
void failDelivery(uint32_t now) {
  linkState=everGateway?CENTRAL_DOWN:GATEWAY_DOWN;waiting=false;scanning=false;
  forceSnapshot=false;
  nextRecovery=now+journal.config.recoveryS*1000UL;
  // Offline limita transmisiones, no apaga la escucha de comandos (contrato
  // AURA 3.3). Si el barrido fallo, volver al ultimo canal conocido del padre.
  if(radioRunning)esp_wifi_set_channel(listeningChannel,WIFI_SECOND_CHAN_NONE);
  updateOutputs();
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
      wifi_second_chan_t second;esp_wifi_get_channel(&listeningChannel,&second);
      forceSnapshot=true;
      scanning=false;waiting=true;attempts=0;everGateway=false;transmit(now);continue;
    }
    if(waiting && t.largo==16 && sameId(t.payload,flight.id)) {
      if(t.tipo==ACK_GATEWAY) {gatewayReceived=true;everGateway=true;}
      if((t.tipo==ACK_CENTRAL && flight.kind==MEDICION)||(t.tipo==ACK_ALERT && flight.kind!=MEDICION)) {
        acknowledge(journal,flight.id);
        if(!commit())return;
        waiting=false;if(t.tipo==ACK_CENTRAL)linkState=ONLINE;attempts=0;nextRecovery=now;snapshotPending=true;updateOutputs();
        Serial.println(t.tipo==ACK_CENTRAL?"[ACK] AURA REST persistio muestra":"[ACK ALERTA] Gateway publico en MQTT; no acredita persistencia central");
      }
    }
    if(t.tipo==COMANDO && t.largo==sizeof(ConfigCommand)) {
      ConfigCommand cmd;memcpy(&cmd,t.payload,sizeof(cmd));
      ConfigResult result={cmd,0};MeshConfig next;
      if(memchr(cmd.id,0,sizeof(cmd.id)) && applyPatch(journal.config,cmd,next) && storageOk) {
        MeshConfig old=journal.config;journal.config=next;
        if(commit())result.applied=1;else journal.config=old;
      }
      result.command.config=journal.config;
      if(result.applied)updateOutputs();
      radioSend(parentMac,frame(CONFIG_RESULT,ownMac,gatewayMac,t.seq,&result,sizeof(result)));
      lastRadioSend=now;snapshotPending=true;forceSnapshot=true;
    }
  }
}
void sendSnapshot(uint32_t now) {
  if(!snapshotPending || !storageOk || !radioRunning || scanning || uint32_t(now-lastRadioSend)<30)return;
  if(!macSet(parentMac)||!macSet(gatewayMac))return;
  if((linkState==GATEWAY_DOWN||linkState==CENTRAL_DOWN)&&!forceSnapshot)return;
  NodeSnapshot snapshot={};snapshot.config=journal.config;
  snapshot.powerKnown=ICIV_POWER_PIN>=0 && journal.powerKnown;snapshot.onBattery=battery;snapshot.cutAt=journal.cutAt;
  for(int s=0;s<2;++s) {
    snapshot.pending[s]=journal.count[s]+(journal.hasFirst[s]&&!journal.firstAcked[s]?1:0);
    snapshot.dropped[s]=journal.dropped[s];snapshot.sensorKnown[s]=sensorKnown[s];snapshot.sensorValid[s]=journal.sensorState[s]==SONDA_OK;snapshot.sensorState[s]=journal.sensorState[s];
  }
  snapshot.pending[0]+=journal.hasPower&&!journal.powerAcked?1:0;
  if(radioSend(parentMac,frame(CONFIG_SNAPSHOT,ownMac,gatewayMac,seq++,&snapshot,sizeof(snapshot)))){snapshotPending=false;forceSnapshot=false;}
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
  for(int s=0;s<2;++s)sensorKnown[s]=journal.sensorKnown[s];
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
