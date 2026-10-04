// GENERADO: editar el rol original y regenerar.
// Rele fijo del ejemplo AURA: NO fabrica ACK del gateway ni del central.
// El sensor conserva su copia hasta el ACK central, por lo que la sala no
// necesita asumir propiedad de las muestras ni borrarlas al confirmar un salto.
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

#if __has_include("config_local.h")
#include "config_local.h"
#endif
#ifndef ICIV_SENSOR_MAC
#define ICIV_SENSOR_MAC {0,0,0,0,0,0}
#endif
#ifndef ICIV_GATEWAY_MAC
#define ICIV_GATEWAY_MAC {0,0,0,0,0,0}
#endif
#ifndef ICIV_CHANNEL
#define ICIV_CHANNEL 1
#endif
const uint8_t sensorMac[6]=ICIV_SENSOR_MAC,gatewayMac[6]=ICIV_GATEWAY_MAC;
bool ready=false;
void setup() {
  Serial.begin(115200);ready=radioInit(ICIV_CHANNEL);
  if(ready)ready=addPeer(sensorMac)&&addPeer(gatewayMac);
  Serial.println(ready?"Sala ICIV lista":"Sala: configurar MAC/canal o revisar radio");
}
void loop() {
  if(!ready){delay(1000);return;}
  RadioPacket p;
  while(radioRead(p)) {
    if(!memcmp(p.sender,sensorMac,6) && !memcmp(p.trama.origen,sensorMac,6) && !memcmp(p.trama.destino,gatewayMac,6))radioSend(gatewayMac,p.trama);
    else if(!memcmp(p.sender,gatewayMac,6) && !memcmp(p.trama.origen,gatewayMac,6) && !memcmp(p.trama.destino,sensorMac,6))radioSend(sensorMac,p.trama);
  }
  delay(5);
}
