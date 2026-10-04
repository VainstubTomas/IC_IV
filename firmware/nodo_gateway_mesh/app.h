/* Adaptador IC_IV del gateway ESP-NOW de AURA. Requiere coordinar su carga
 * con quien administra el gateway. ACK_GATEWAY = recibido en este proceso;
 * ACK_CENTRAL = ingesta durable confirmada por respuesta de AURA, NO PUBACK MQTT.
 * Dependencias: ArduinoMqttClient y ArduinoJson 7.
 */
#include <HTTPClient.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include "../mesh_comun/mesh_radio.h"
#if __has_include("config_local.h")
#include "config_local.h"
#else
#include "config_local.h.example"
#endif
#ifndef ICIV_TELEMETRY_TYPE
#define ICIV_TELEMETRY_TYPE "temperature"
#endif
const uint8_t sensorMac[6]=ICIV_SENSOR_MAC,roomMac[6]=ICIV_ROOM_MAC;
const char* sensorIds[2]={ICIV_FRIDGE_ID,ICIV_FREEZER_ID};
WiFiClient mqttNet;
MqttClient mqtt(mqttNet);
bool ready=false;
uint32_t reconnectAt=0,diagAt=0;
uint8_t wifiTries=0,mqttTries=0;
int lastHttp=0;
bool centralKnown=false,centralUp=false;
uint8_t acceptedCache[16][16]={};
uint8_t cached=0,cacheHead=0;
void uuidText(const uint8_t id[16],char out[37]) {
  snprintf(out,37,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",id[0],id[1],id[2],id[3],id[4],id[5],id[6],id[7],id[8],id[9],id[10],id[11],id[12],id[13],id[14],id[15]);
}
bool uuidBytes(const char* text,uint8_t out[16]) {
  if(!text || strlen(text)!=36)return false;
  int idx=0,high=-1;
  for(int i=0;i<36;++i) {
    if(i==8||i==13||i==18||i==23){if(text[i]!='-')return false;continue;}
    char c=text[i];int v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
    if(v<0)return false;if(high<0)high=v;else{out[idx++]=(high<<4)|v;high=-1;}
  }
  return idx==16;
}
bool validUuid(const char* text) {uint8_t id[16];if(!uuidBytes(text,id))return false;for(auto b:id)if(b)return true;return false;}
void isoTime(uint32_t epoch,char out[21]) {
  time_t t=epoch;struct tm tm;gmtime_r(&t,&tm);strftime(out,21,"%Y-%m-%dT%H:%M:%SZ",&tm);
}
const uint8_t* routeToSensor() {return macSet(roomMac)?roomMac:sensorMac;}
void sendAck(uint8_t type,const TramaMesh& original,const uint8_t id[16]) {
  radioSend(routeToSensor(),frame(type,ownMac,sensorMac,original.seq,id,16));
}
bool publishJson(const char* device,const char* kind,JsonDocument& doc,bool retain=false) {
  if(!mqtt.connected() || !validUuid(device))return false;
  String text;serializeJson(doc,text);char topic[96];snprintf(topic,sizeof(topic),"devices/%s/%s",device,kind);
  mqtt.beginMessage(topic,retain,1);mqtt.print(text);return mqtt.endMessage()==1;
}
void response(int sensor,const char* commandId,const char* status,const char* reason=nullptr,const ConfigResult* result=nullptr) {
  JsonDocument d;d["status"]=status;d["details"]["command_id"]=commandId;
  if(reason)d["details"]["motivo"]=reason;
  // Extension IC_IV propuesta; no emitirla hasta acordar su uso con AURA.
  if(result && ICIV_AURA_EXTENSIONS_ENABLED) {
    d["details"]["iciv"]["applied"]=bool(result->applied);
    d["details"]["iciv"]["sensor"]=sensor?"freezer":"heladera";
    d["details"]["iciv"]["interval_s"]=result->command.config.intervalS;
    d["details"]["iciv"]["min_c"]=result->command.config.min100/100.0;
    d["details"]["iciv"]["max_c"]=result->command.config.max100/100.0;
    d["details"]["iciv"]["recovery_s"]=result->command.recoveryS;
  }
  publishJson(sensorIds[sensor],"response",d);
}
void onMqtt(int size) {
  String topic=mqtt.messageTopic(),body;
  while(mqtt.available()){if(body.length()<2048)body+=char(mqtt.read());else mqtt.read();}
  if(size>2048)return;
  int sensor=-1;for(int s=0;s<2;++s)if(topic==String("devices/")+sensorIds[s]+"/command")sensor=s;
  if(sensor<0)return;
  JsonDocument d;if(deserializeJson(d,body))return;
  const char* commandId=d["command_id"]|"";
  ConfigCommand cmd={};
  if(!uuidBytes(commandId,cmd.id))return;
  if(!ICIV_AURA_EXTENSIONS_ENABLED){response(sensor,commandId,"rechazado","Adaptacion mesh IC_IV no habilitada en el gateway");return;}
  if(strcmp(d["command"]|"","set_config")){response(sensor,commandId,"rechazado","Comando no soportado");return;}
  auto p=d["params"].as<JsonObject>();
  const char* expected=sensor?"freezer":"heladera";
  if(p.size()!=5 || strcmp(p["sensor"]|"",expected) || !p["interval_s"].is<uint32_t>() || !p["recovery_s"].is<uint32_t>() || !p["min_c"].is<double>() || !p["max_c"].is<double>()) {
    response(sensor,commandId,"rechazado","Configuracion completa y sensor del topico requeridos");return;
  }
  const double lo=p["min_c"],hi=p["max_c"];
  if(!isfinite(lo)||!isfinite(hi)||lo<-55||hi>125||lo>=hi||fabs(lo*100-round(lo*100))>0.00001||fabs(hi*100-round(hi*100))>0.00001) {
    response(sensor,commandId,"rechazado","Umbrales invalidos");return;
  }
  cmd.sensor=sensor;cmd.config={p["interval_s"].as<uint32_t>(),int16_t(lround(lo*100)),int16_t(lround(hi*100))};cmd.recoveryS=p["recovery_s"].as<uint32_t>();
  if(!validSensorConfig(cmd.config)||cmd.recoveryS<60||cmd.recoveryS>86400){response(sensor,commandId,"rechazado","Intervalo fuera de rango");return;}
  if(radioSend(routeToSensor(),frame(COMANDO,ownMac,sensorMac,0,&cmd,sizeof(cmd))))response(sensor,commandId,"transmitido");
  else response(sensor,commandId,"rechazado","No se pudo transmitir al siguiente salto");
}
bool cachedAccepted(const uint8_t id[16]) {for(int i=0;i<cached;++i)if(sameId(acceptedCache[i],id))return true;return false;}
void remember(const uint8_t id[16]) {memcpy(acceptedCache[cacheHead],id,16);cacheHead=(cacheHead+1)%16;if(cached<16)++cached;}
bool submitCentral(const Muestra& m,const char* id) {
  if(!ICIV_AURA_INGEST_ENABLED || !validUuid(ICIV_TENANT_ID) || !validUuid(sensorIds[m.sensor]) || WiFi.status()!=WL_CONNECTED)return false;
  JsonDocument d;auto ev=d["events"].to<JsonArray>().add<JsonObject>();
  ev["tenant_id"]=ICIV_TENANT_ID;ev["device_id"]=sensorIds[m.sensor];ev["type"]=ICIV_TELEMETRY_TYPE;ev["ingest_id"]=id;
  auto payload=ev["payload"].to<JsonObject>();if(m.flags&VALID)payload["temp_c"]=m.temp100/100.0;
  if(ICIV_AURA_EXTENSIONS_ENABLED) {
    if(m.flags&CLOCK_VALID){char ts[21];isoTime(m.measuredAt,ts);payload["measured_at"]=ts;}
    payload["clock_valid"]=bool(m.flags&CLOCK_VALID);payload["power_known"]=bool(m.flags&POWER_KNOWN);
    if(m.flags&POWER_KNOWN)payload["on_battery"]=bool(m.flags&BATTERY);
    payload["power_first"]=bool(m.flags&POWER_FIRST);payload["sensor_valid"]=bool(m.flags&VALID);
    if(m.cutAt){char ts[21];isoTime(m.cutAt,ts);payload["power_cut_at"]=ts;}
  }
  String body;serializeJson(d,body);
  HTTPClient http;http.setConnectTimeout(1500);http.setTimeout(4000);
  if(!http.begin(String(ICIV_API_BASE)+"/api/v1/telemetry/ingest"))return false;
  http.addHeader("Content-Type","application/json");
  if(strlen(ICIV_API_TOKEN))http.addHeader("Authorization",String("Bearer ")+ICIV_API_TOKEN);
  lastHttp=http.POST(body);String reply=http.getString();http.end();
  if(lastHttp!=200 && lastHttp!=201)return false;
  // El 200 del broker NO es un ACK de AURA. IDs aceptados o acuerdo explicito
  // sobre la respuesta REST: nunca confirmar un rechazo parcial como guardado.
  JsonDocument result;const bool parsed=!deserializeJson(result,reply);
  const bool rejected=parsed && (result["errors"].as<JsonArray>().size()>0 || result["rejected"].as<int>()>0 || !strcmp(result["status"]|"","error"));
  const bool explicitIds=parsed && result["accepted_ingest_ids"].is<JsonArray>();
  bool accepted=false;
  if(explicitIds)for(auto value:result["accepted_ingest_ids"].as<JsonArray>())if(value.is<const char*>() && !strcmp(value.as<const char*>(),id))accepted=true;
  return centralAckAllowed(lastHttp,explicitIds,accepted,rejected,ICIV_HTTP_SUCCESS_IS_DURABLE);
}
void mirrorSample(const Muestra& m,const char* id) {
  JsonDocument d;d["ingest_id"]=id;auto values=d["values"].to<JsonObject>();
  if(m.flags&VALID)values["temp_c"]=m.temp100/100.0;
  if(ICIV_AURA_EXTENSIONS_ENABLED) {
    if(m.flags&CLOCK_VALID){char ts[21];isoTime(m.measuredAt,ts);d["measured_at"]=ts;}
    if(m.cutAt){char ts[21];isoTime(m.cutAt,ts);d["power_cut_at"]=ts;}
    d["power_first"]=bool(m.flags&POWER_FIRST);
    if(m.flags&POWER_KNOWN)d["on_battery"]=bool(m.flags&BATTERY);
  }
  publishJson(sensorIds[m.sensor],"data",d);
}
void publishSnapshot(const NodeSnapshot& snap) {
  if(!ICIV_AURA_EXTENSIONS_ENABLED || !validConfig(snap.config))return;
  for(int s=0;s<2;++s) {
    JsonDocument d;d["status"]="online";auto info=d["details"]["iciv"].to<JsonObject>();
    info["sensor"]=s?"freezer":"heladera";
    info["config"]["sensor"]=s?"freezer":"heladera";
    info["config"]["interval_s"]=snap.config.sensors[s].intervalS;
    info["config"]["min_c"]=snap.config.sensors[s].min100/100.0;
    info["config"]["max_c"]=snap.config.sensors[s].max100/100.0;
    info["config"]["recovery_s"]=snap.config.recoveryS;
    info["pending"]=snap.pending[s];info["dropped"]=snap.dropped[s];
    info["power_known"]=bool(snap.powerKnown);
    if(snap.powerKnown)info["on_battery"]=bool(snap.onBattery);
    if(snap.sensorKnown[s])info["sensor_valid"]=bool(snap.sensorValid[s]);
    if(snap.cutAt){char ts[21];isoTime(snap.cutAt,ts);info["power_cut_at"]=ts;}
    publishJson(sensorIds[s],"status",d,true);
  }
}
void processRadio() {
  RadioPacket p;
  while(radioRead(p)) {
    const auto& t=p.trama;
    if(memcmp(p.sender,routeToSensor(),6)||memcmp(t.origen,sensorMac,6)||memcmp(t.destino,ownMac,6))continue;
    if(t.tipo==PROBE && t.largo==0){radioSend(routeToSensor(),frame(PROBE_ACK,ownMac,sensorMac,t.seq,nullptr,0));continue;}
    if(t.tipo==CONFIG_SNAPSHOT && t.largo==sizeof(NodeSnapshot)) {
      NodeSnapshot snap;memcpy(&snap,t.payload,sizeof(snap));publishSnapshot(snap);continue;
    }
    if(t.tipo==CONFIG_RESULT && t.largo==sizeof(ConfigResult)) {
      ConfigResult result;memcpy(&result,t.payload,sizeof(result));if(result.command.sensor>1)continue;
      char id[37];uuidText(result.command.id,id);
      response(result.command.sensor,id,result.applied?"recibido":"rechazado",result.applied?nullptr:"Nodo rechazo configuracion o fallo NVS",&result);continue;
    }
    if(t.tipo!=TELEMETRIA || t.largo!=sizeof(Muestra))continue;
    Muestra m;memcpy(&m,t.payload,sizeof(m));if(!validSample(m))continue;
    sendAck(ACK_GATEWAY,t,m.id); // receptor final del dato, no la sala
    char id[37];uuidText(m.id,id);
    if(cachedAccepted(m.id)){sendAck(ACK_CENTRAL,t,m.id);continue;}
    centralUp=submitCentral(m,id);centralKnown=true;
    if(centralUp){remember(m.id);sendAck(ACK_CENTRAL,t,m.id);mirrorSample(m,id);}
    Serial.printf("[INGESTA] %s sensor=%u HTTP=%d central=%s\n",id,m.sensor,lastHttp,centralUp?"CONFIRMADO":"PENDIENTE");
  }
}
void serviceWifi(uint32_t now) {
  if(WiFi.status()==WL_CONNECTED){wifiTries=0;return;}
  if(int32_t(now-reconnectAt)<0)return;
  WiFi.begin(ICIV_WIFI_SSID,ICIV_WIFI_PASS);
  reconnectAt=now+(++wifiTries>=4?300000UL:5000UL);
  if(wifiTries>=4)wifiTries=0;
}
void serviceMqtt(uint32_t now) {
  if(WiFi.status()!=WL_CONNECTED)return;
  mqtt.poll();if(mqtt.connected()){mqttTries=0;return;}
  if(int32_t(now-reconnectAt)<0)return;
  if(mqtt.connect(ICIV_MQTT_HOST,ICIV_MQTT_PORT)) {
    for(auto id:sensorIds){char topic[96];snprintf(topic,sizeof(topic),"devices/%s/command",id);mqtt.subscribe(topic,1);}
    reconnectAt=now;
  }else {reconnectAt=now+(++mqttTries>=4?300000UL:5000UL);if(mqttTries>=4)mqttTries=0;}
}
void diagnostics(uint32_t now) {
  if(uint32_t(now-diagAt)<15000)return;diagAt=now;
  JsonDocument d;d["status"]="online";
  if(ICIV_AURA_EXTENSIONS_ENABLED) {
    d["details"]["iciv"]["central"]=centralKnown?(centralUp?"online":"offline"):"unknown";
    d["details"]["iciv"]["http_status"]=lastHttp;
  }
  publishJson(ICIV_GATEWAY_ID,"status",d,true);
}
void setup() {
  Serial.begin(115200);WiFi.mode(WIFI_STA);WiFi.setSleep(false);
  WiFi.begin(ICIV_WIFI_SSID,ICIV_WIFI_PASS); // no bloquear la radio esperando WiFi
  ready=radioInit(1,true);if(ready)addPeer(routeToSensor());
  mqtt.setId(String("iciv-gw-")+ICIV_GATEWAY_ID);mqtt.setConnectionTimeout(1500);
  if(strlen(ICIV_MQTT_USER))mqtt.setUsernamePassword(ICIV_MQTT_USER,ICIV_MQTT_PASS);
  char topic[96];snprintf(topic,sizeof(topic),"devices/%s/status",ICIV_GATEWAY_ID);
  mqtt.beginWill(topic,20,true,1);mqtt.print("{\"status\":\"offline\"}");mqtt.endWill();
  mqtt.onMessage(onMqtt);
  Serial.println("Gateway ICIV: configurar MAC/UUID/red y acordar respuesta durable de AURA");
}
void loop() {
  uint32_t now=millis();if(ready)processRadio();serviceWifi(now);serviceMqtt(now);diagnostics(now);delay(5);
}
