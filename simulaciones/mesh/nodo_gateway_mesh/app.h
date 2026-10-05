/* Adaptador de banco IC IV / contrato AURA v3.
 * Telemetria AURA solo REST. MQTT AURA: estado, comandos, respuestas, alertas.
 * El espejo opcional del dashboard usa OTRO broker. HTTP corre en una tarea.
 */
#include <HTTPClient.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include "../../../firmware/mesh_comun/mesh_radio.h"
#include "../../../firmware/mesh_comun/mesh_ingest.h"
#if __has_include("config_local.h")
#include "config_local.h"
#else
#include "config_local.h.example"
#endif
const uint8_t sensorMac[6]=ICIV_SENSOR_MAC,roomMac[6]=ICIV_ROOM_MAC;
const char* fields[2]={"temp_heladera_c","temp_freezer_c"};
WiFiClient auraNet,localNet;
MqttClient aura(auraNet),local(localNet);
bool ready=false,localAllowed=false,centralKnown=false,centralUp=false,nodeSeen=false,nodeOffline=false;
uint32_t wifiAt=0,auraAt=0,localAt=0,diagAt=0,lastSeen=0;
uint8_t wifiTries=0,auraTries=0;
int lastHttp=0;
uint32_t orphanCount=0;
NodeSnapshot nodeInfo={};
struct HttpJob {Muestra sample;uint16_t seq;};
struct HttpResult {HttpJob job;bool accepted;int http;};
QueueHandle_t httpJobs=nullptr,httpResults=nullptr;
uint8_t accepted[16][16]={},cacheHead=0,cacheCount=0;
bool queued=false;uint8_t queuedId[16]={};
void uuidText(const uint8_t id[16],char out[37]) {
  snprintf(out,37,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",id[0],id[1],id[2],id[3],id[4],id[5],id[6],id[7],id[8],id[9],id[10],id[11],id[12],id[13],id[14],id[15]);
}
bool validUuid(const char* text) {
  if(!text||strlen(text)!=36)return false;bool nonzero=false;
  for(int i=0;i<36;++i){if(i==8||i==13||i==18||i==23){if(text[i]!='-')return false;}else{if(!isxdigit(text[i]))return false;if(text[i]!='0')nonzero=true;}}
  return nonzero;
}
void isoTime(uint32_t epoch,char out[21]) {time_t t=epoch;struct tm tm;gmtime_r(&t,&tm);strftime(out,21,"%Y-%m-%dT%H:%M:%SZ",&tm);}
const uint8_t* route(){return macSet(roomMac)?roomMac:sensorMac;}
void ack(uint8_t type,uint16_t seq,const uint8_t id[16]){radioSend(route(),frame(type,ownMac,sensorMac,seq,id,16));}
bool cached(const uint8_t id[16]) {for(int i=0;i<cacheCount;++i)if(sameId(accepted[i],id))return true;return false;}
void remember(const uint8_t id[16]) {memcpy(accepted[cacheHead],id,16);cacheHead=(cacheHead+1)%16;if(cacheCount<16)++cacheCount;}
bool publish(MqttClient& client,const char* device,const char* kind,JsonDocument& doc,bool retain=false) {
  if(!client.connected()||!validUuid(device))return false;
  char topic[100];snprintf(topic,sizeof(topic),"devices/%s/%s",device,kind);String body;serializeJson(doc,body);
  client.beginMessage(topic,retain,1);client.print(body);return client.endMessage()==1;
}
void configJson(JsonObject o,const MeshConfig& c) {
  o["intervalo_heladera_s"]=c.sensors[0].intervalS;o["intervalo_freezer_s"]=c.sensors[1].intervalS;
  o["min_heladera_c"]=c.sensors[0].min100/100.0;o["max_heladera_c"]=c.sensors[0].max100/100.0;
  o["min_freezer_c"]=c.sensors[1].min100/100.0;o["max_freezer_c"]=c.sensors[1].max100/100.0;o["recuperacion_s"]=c.recoveryS;
}
void response(const char* id,const char* status,const char* reason=nullptr,const MeshConfig* config=nullptr) {
  JsonDocument d;d["status"]=status;if(id&&*id)d["details"]["command_id"]=id;
  if(reason)d["details"]["motivo"]=reason;if(config)configJson(d["details"]["config"].to<JsonObject>(),*config);
  publish(aura,ICIV_DEVICE_ID,"response",d);if(localAllowed)publish(local,ICIV_DEVICE_ID,"response",d);
}
bool decodePatch(JsonObject p,ConfigCommand& cmd) {
  const char* keys[]={"intervalo_heladera_s","intervalo_freezer_s","min_heladera_c","max_heladera_c","min_freezer_c","max_freezer_c","recuperacion_s"};
  if(!p.size()||p.size()>7)return false;
  for(auto item:p){bool found=false;for(auto key:keys)if(!strcmp(item.key().c_str(),key))found=true;if(!found)return false;}
  for(int i=0;i<7;++i)if(p[keys[i]].isUnbound()==false){
    cmd.mask|=1<<i;
    if(i<2||i==6){if(!p[keys[i]].is<uint32_t>())return false;uint32_t n=p[keys[i]];if(n<(i==6?60:5)||n>86400)return false;if(i==6)cmd.config.recoveryS=n;else cmd.config.sensors[i].intervalS=n;}
    else {if(!p[keys[i]].is<double>())return false;double n=p[keys[i]];if(!isfinite(n)||n<-55||n>125||fabs(n*100-round(n*100))>0.00001)return false;
      int16_t v=lround(n*100);if(i==2)cmd.config.sensors[0].min100=v;if(i==3)cmd.config.sensors[0].max100=v;if(i==4)cmd.config.sensors[1].min100=v;if(i==5)cmd.config.sensors[1].max100=v;}
  }
  MeshConfig merged;if(nodeSeen&&!applyPatch(nodeInfo.config,cmd,merged))return false;
  return cmd.mask!=0;
}
void command(MqttClient& client,int size) {
  String topic=client.messageTopic(),body;
  while(client.available()){char c=client.read();if(body.length()<2048)body+=c;}
  if(topic!=String("devices/")+ICIV_DEVICE_ID+"/command")return;
  if(size>2048){response(nullptr,"rechazado","JSON supera limite del adaptador");return;}
  JsonDocument d;if(deserializeJson(d,body)){response(nullptr,"rechazado","JSON invalido");return;}
  if(!d["command_id"].isUnbound()&&!d["command_id"].is<const char*>()){response(nullptr,"rechazado","command_id debe ser texto");return;}
  const char* id=d["command_id"]|"";
  if(strlen(id)>64){response(id,"rechazado","command_id supera 64 caracteres");return;}
  ConfigCommand cmd={};strlcpy(cmd.id,id,sizeof(cmd.id));
  if(strcmp(d["command"]|"","set_config")){response(id,"rechazado","Comando no soportado");return;}
  if(!macSet(sensorMac)||!validUuid(ICIV_DEVICE_ID)){response(id,"rechazado","MAC/UUID sin destino");return;}
  if(!d["params"].is<JsonObject>()||!decodePatch(d["params"].as<JsonObject>(),cmd)){response(id,"rechazado","Parche invalido o fuera de rango");return;}
  if(radioSend(route(),frame(COMANDO,ownMac,sensorMac,0,&cmd,sizeof(cmd))))response(id,"transmitido");else response(id,"rechazado","No se pudo transmitir");
}
void onAura(int size){command(aura,size);}void onLocal(int size){command(local,size);}
HttpResult post(const HttpJob& job) {
  HttpResult out={job,false,0};const Muestra& m=job.sample;
  if(m.kind!=MEDICION||!ICIV_AURA_INGEST_ENABLED||!validUuid(ICIV_DEVICE_ID)||!validUuid(ICIV_TENANT_ID)||WiFi.status()!=WL_CONNECTED)return out;
  char id[37];uuidText(m.id,id);JsonDocument d;auto ev=d["events"].to<JsonArray>().add<JsonObject>();
  ev["tenant_id"]=ICIV_TENANT_ID;ev["device_id"]=ICIV_DEVICE_ID;ev["type"]="temperatura";ev["ingest_id"]=id;
  if(m.flags&CLOCK_VALID){char ts[21];isoTime(m.measuredAt,ts);ev["ts"]=ts;}
  ev["payload"][fields[m.sensor]]=m.temp100/100.0;
  String body;serializeJson(d,body);HTTPClient http;http.setConnectTimeout(1500);http.setTimeout(4000);
  if(!http.begin(String(ICIV_API_BASE)+"/api/v1/telemetry/ingest"))return out;
  http.addHeader("Content-Type","application/json");if(strlen(ICIV_API_TOKEN))http.addHeader("Authorization",String("Bearer ")+ICIV_API_TOKEN);
  out.http=http.POST(body);String reply=http.getString();http.end();
  out.accepted=acceptedIngestReply(out.http,reply.c_str());
  return out;
}
void httpWorker(void*) {HttpJob job;for(;;)if(xQueueReceive(httpJobs,&job,portMAX_DELAY)==pdTRUE){HttpResult result=post(job);xQueueSend(httpResults,&result,portMAX_DELAY);}}
void mirrorSample(const Muestra& m) {
  if(!localAllowed||m.kind!=MEDICION)return;
  JsonDocument d;char id[37];uuidText(m.id,id);d["ingest_id"]=id;d["values"][fields[m.sensor]]=m.temp100/100.0;
  if(m.flags&CLOCK_VALID){char ts[21];isoTime(m.measuredAt,ts);d["ts"]=ts;}
  publish(local,ICIV_DEVICE_ID,"data",d);
}
bool publishAlert(const Muestra& m) {
  JsonDocument d;const char* type=m.kind==ALERTA_SENSOR?"sensor":"energia";
  if(m.kind==ALERTA_SENSOR){d["severity"]=m.state==SONDA_OK?"info":"high";d["message"]=m.state==SONDA_OK?"Sonda recuperada":"Sonda sin medicion valida";d["details"]["campo"]=fields[m.sensor];d["details"]["motivo"]=m.state==SONDA_OK?"recuperada":m.state==SIN_RESPUESTA?"sin_respuesta":"fuera_de_rango";}
  else {d["severity"]=m.state?"warning":"info";d["message"]=m.state?"Nodo en bateria":"Nodo volvio a la red";d["details"]["alimentacion"]=m.state?"bateria":"red";}
  if(m.flags&CLOCK_VALID){char ts[21];isoTime(m.measuredAt,ts);d["ts"]=ts;}
  String body;serializeJson(d,body);char topic[96];snprintf(topic,sizeof(topic),"alerts/%s/%s",ICIV_DEVICE_ID,type);
  bool ok=false;if(aura.connected()){aura.beginMessage(topic,false,1);aura.print(body);ok=aura.endMessage()==1;}
  if(localAllowed&&local.connected()){local.beginMessage(topic,false,1);local.print(body);local.endMessage();}
  return ok; // recibo de publicacion, NO persistencia central de alertas
}
void nodeStatus(bool offline=false) {
  if(!nodeSeen)return;JsonDocument d;d["status"]=offline?"offline":"online";
  d["details"]["evento"]="reporte";d["details"]["transporte"]="mesh";if(offline)d["details"]["motivo"]="sin_uplinks";
  configJson(d["details"]["config"].to<JsonObject>(),nodeInfo.config);
  d["details"]["pendientes"]=nodeInfo.pending[0]+nodeInfo.pending[1];d["details"]["descartadas"]=nodeInfo.dropped[0]+nodeInfo.dropped[1];
  d["details"]["alimentacion"]=nodeInfo.powerKnown?(nodeInfo.onBattery?"bateria":"red"):"desconocida";
  publish(aura,ICIV_DEVICE_ID,"status",d,true);if(localAllowed)publish(local,ICIV_DEVICE_ID,"status",d,true);
}
void processRadio() {
  RadioPacket p;int limit=4;
  while(limit--&&radioRead(p)) {
    const auto& t=p.trama;if(memcmp(t.destino,ownMac,6))continue;
    if(memcmp(p.sender,route(),6)||memcmp(t.origen,sensorMac,6)||!validUuid(ICIV_DEVICE_ID)){
      ++orphanCount;Serial.println("[HUERFANO] MAC/UUID sin mapeo: trama descartada");continue;
    }
    lastSeen=millis();nodeOffline=false;
    if(t.tipo==PROBE&&t.largo==0){radioSend(route(),frame(PROBE_ACK,ownMac,sensorMac,t.seq,nullptr,0));continue;}
    if(t.tipo==CONFIG_SNAPSHOT&&t.largo==sizeof(NodeSnapshot)){NodeSnapshot snap;memcpy(&snap,t.payload,sizeof(snap));if(validConfig(snap.config)){nodeInfo=snap;nodeSeen=true;nodeStatus();}continue;}
    if(t.tipo==CONFIG_RESULT&&t.largo==sizeof(ConfigResult)){ConfigResult r;memcpy(&r,t.payload,sizeof(r));if(memchr(r.command.id,0,sizeof(r.command.id))&&validConfig(r.command.config))response(r.command.id,r.applied?"aplicado":"rechazado",r.applied?nullptr:"Nodo rechazo o fallo NVS",&r.command.config);continue;}
    if(t.tipo!=TELEMETRIA||t.largo!=sizeof(Muestra))continue;
    Muestra m;memcpy(&m,t.payload,sizeof(m));if(!validSample(m))continue;ack(ACK_GATEWAY,t.seq,m.id);
    if(cached(m.id)){ack(m.kind==MEDICION?ACK_CENTRAL:ACK_ALERT,t.seq,m.id);continue;}
    if(m.kind!=MEDICION){if(publishAlert(m)){remember(m.id);ack(ACK_ALERT,t.seq,m.id);}continue;}
    if(queued)continue;
    HttpJob job={m,t.seq};if(httpJobs&&xQueueSend(httpJobs,&job,0)==pdTRUE){queued=true;memcpy(queuedId,m.id,16);}
  }
  HttpResult r;
  while(httpResults&&xQueueReceive(httpResults,&r,0)==pdTRUE){queued=false;lastHttp=r.http;centralKnown=true;centralUp=r.accepted;
    if(r.accepted){remember(r.job.sample.id);ack(ACK_CENTRAL,r.job.seq,r.job.sample.id);mirrorSample(r.job.sample);}
    char id[37];uuidText(r.job.sample.id,id);Serial.printf("[INGESTA] %s HTTP=%d central=%s\n",id,r.http,r.accepted?"CONFIRMADO":"PENDIENTE");}
}
bool brokersDifferent() {
  if(!ICIV_LOCAL_MQTT_ENABLED)return false;
  IPAddress a,b;if(!WiFi.hostByName(ICIV_MQTT_HOST,a)||!WiFi.hostByName(ICIV_LOCAL_MQTT_HOST,b))return false;
  return a!=b || ICIV_MQTT_PORT!=ICIV_LOCAL_MQTT_PORT;
}
void serviceWifi(uint32_t now) {
  if(WiFi.status()==WL_CONNECTED){wifiTries=0;return;}localAllowed=false;
  if(int32_t(now-wifiAt)<0)return;WiFi.begin(ICIV_WIFI_SSID,ICIV_WIFI_PASS);
  wifiAt=now+(++wifiTries>=4?300000UL:5000UL);if(wifiTries>=4)wifiTries=0;
}
void serviceMqtt(uint32_t now) {
  if(WiFi.status()!=WL_CONNECTED)return;aura.poll();local.poll();
  if(!aura.connected()&&int32_t(now-auraAt)>=0){if(aura.connect(ICIV_MQTT_HOST,ICIV_MQTT_PORT)){aura.subscribe(String("devices/")+ICIV_DEVICE_ID+"/command",1);auraTries=0;nodeStatus(nodeOffline);}auraAt=now+(++auraTries>=4?300000UL:5000UL);if(auraTries>=4)auraTries=0;}
  if(ICIV_LOCAL_MQTT_ENABLED&&!local.connected()&&int32_t(now-localAt)>=0){localAllowed=brokersDifferent();if(localAllowed&&local.connect(ICIV_LOCAL_MQTT_HOST,ICIV_LOCAL_MQTT_PORT)){local.subscribe(String("devices/")+ICIV_DEVICE_ID+"/command",1);nodeStatus(nodeOffline);}else if(!localAllowed)Serial.println("[ESPEJO] Bloqueado: broker local sin resolver o igual a AURA");localAt=now+5000;}
}
void diagnostics(uint32_t now) {
  if(nodeSeen&&!nodeOffline&&uint32_t(now-lastSeen)>3UL*min(nodeInfo.config.sensors[0].intervalS,nodeInfo.config.sensors[1].intervalS)*1000UL){nodeOffline=true;nodeStatus(true);}
  if(uint32_t(now-diagAt)<15000)return;diagAt=now;JsonDocument d;d["status"]="online";d["details"]["transporte"]="mesh";d["details"]["central"]=centralKnown?(centralUp?"online":"offline"):"unknown";d["details"]["http_status"]=lastHttp;
  d["details"]["huerfanos"]=orphanCount;
  publish(aura,ICIV_GATEWAY_ID,"status",d,true);if(localAllowed)publish(local,ICIV_GATEWAY_ID,"status",d,true);
}
void setup() {
  Serial.begin(115200);WiFi.mode(WIFI_STA);WiFi.setSleep(false);WiFi.begin(ICIV_WIFI_SSID,ICIV_WIFI_PASS);
  ready=radioInit(1,true);if(ready)ready=addPeer(route());
  aura.setId(String("iciv-aura-")+ICIV_GATEWAY_ID);local.setId(String("iciv-local-")+ICIV_GATEWAY_ID);
  aura.setConnectionTimeout(1500);local.setConnectionTimeout(1500);
  if(strlen(ICIV_MQTT_USER))aura.setUsernamePassword(ICIV_MQTT_USER,ICIV_MQTT_PASS);
  if(strlen(ICIV_LOCAL_MQTT_USER))local.setUsernamePassword(ICIV_LOCAL_MQTT_USER,ICIV_LOCAL_MQTT_PASS);
  char topic[96];snprintf(topic,sizeof(topic),"devices/%s/status",ICIV_GATEWAY_ID);
  aura.beginWill(topic,20,true,1);aura.print("{\"status\":\"offline\"}");aura.endWill();
  local.beginWill(topic,20,true,1);local.print("{\"status\":\"offline\"}");local.endWill();
  aura.onMessage(onAura);local.onMessage(onLocal);
  httpJobs=xQueueCreate(4,sizeof(HttpJob));httpResults=xQueueCreate(4,sizeof(HttpResult));
  if(!httpJobs||!httpResults||xTaskCreate(httpWorker,"aura_http",10240,nullptr,1,nullptr)!=pdPASS){ready=false;Serial.println("ERROR tarea HTTP: no enviar ACK central");}
  Serial.println("Gateway v3: un UUID por placa; mediciones solo REST; espejo solo broker separado");
}
void loop(){uint32_t now=millis();if(ready)processRadio();serviceWifi(now);serviceMqtt(now);diagnostics(now);delay(5);}
