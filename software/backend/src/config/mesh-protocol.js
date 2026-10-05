import { isDeviceId, parseObject } from './mqtt/aura-protocol.js';
export const SENSORS=Object.freeze(['heladera','freezer']);
export const CONFIG_KEYS=Object.freeze(['intervalo_heladera_s','intervalo_freezer_s','min_heladera_c','max_heladera_c','min_freezer_c','max_freezer_c','recuperacion_s']);
export function validateSensor(s='heladera'){if(!SENSORS.includes(s))throw new Error('Sonda: heladera o freezer');return s;}
export function defaultMeshConfig(){return {intervalo_heladera_s:60,intervalo_freezer_s:300,min_heladera_c:2,max_heladera_c:6,min_freezer_c:-25,max_freezer_c:-15,recuperacion_s:300};}
export function validateMeshPatch(p,current=null){
 if(!p||typeof p!=='object'||Array.isArray(p)||!Object.keys(p).length||Object.keys(p).some(k=>!CONFIG_KEYS.includes(k)))throw new Error('Parche con parametros conocidos y no vacio');
 for(const [k,v] of Object.entries(p)){
  if(k.endsWith('_s')){if(!Number.isInteger(v)||v<(k==='recuperacion_s'?60:5)||v>86400)throw new Error('Intervalo fuera de rango');}
  else if(typeof v!=='number'||!Number.isFinite(v)||v<-55||v>125||Math.abs(v*100-Math.round(v*100))>1e-8)throw new Error('Umbral fuera de rango o precision 0.01');
 }
 const merged=current?{...current,...p}:p;
 for(const s of SENSORS)if(merged['min_'+s+'_c']!==undefined&&merged['max_'+s+'_c']!==undefined&&merged['min_'+s+'_c']>=merged['max_'+s+'_c'])throw new Error('Minimo debe ser menor al maximo');
 return {...p};
}
export function validateMeshConfig(p){validateMeshPatch(p);if(Object.keys(p).length!==7)throw new Error('Reporte completo del nodo requerido');return {...p};}
export function sensorDeviceId(config,sensor='heladera'){validateSensor(sensor);return config.AURA_DEVICE_ID;}
export function utcDate(value){
 if(value===undefined)return undefined;
 if(typeof value!=='string'||!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d{1,3})?Z$/.test(value)||!Number.isFinite(Date.parse(value))||new Date(value).toISOString().slice(0,19)!==value.slice(0,19))throw new Error('ts debe ser una fecha UTC valida');
 return new Date(value);
}
export function parseMeshMetadata(topic,payload){
 const m=/^devices\/([^/]+)\/data$/.exec(topic);if(!m||!isDeviceId(m[1]))return null;
 const p=parseObject(payload);if(p.ingest_id!==undefined&&!isDeviceId(p.ingest_id))throw new Error('ingest_id invalido');
 return {deviceId:m[1].toLowerCase(),...(p.ingest_id?{ingest_id:p.ingest_id.toLowerCase()}:{}),...(p.ts!==undefined?{measuredAt:utcDate(p.ts)}:{})};
}
export function parseMeshData(topic,payload){
 const meta=parseMeshMetadata(topic,payload);if(!meta)return [];
 const p=parseObject(payload);if(!p.values||typeof p.values!=='object'||Array.isArray(p.values))throw new Error('values requerido');
 if(Object.keys(p.values).some(k=>!['temp_heladera_c','temp_freezer_c'].includes(k)))throw new Error('Solo campos de temperatura de las sondas');
 const readings=[];
 for(const sensor of SENSORS){const key='temp_'+sensor+'_c';if(!(key in p.values))continue;const t=p.values[key];if(typeof t!=='number'||!Number.isFinite(t)||t<-55||t>125||t===85)throw new Error('No aceptar centinelas ni temperaturas invalidas');readings.push({...meta,sensor,temperature:t,source:'mqtt_local'});}
 return readings;
}
export function appliedMeshConfig(event,params){
 if(event.status!=='aplicado')return null;
 const c=validateMeshConfig(event.details?.config);
 if(Object.keys(params).some(k=>c[k]!==params[k]))throw new Error('Reporte no coincide con el parche solicitado');
 return c;
}
export function parseMeshAlert(topic,payload){
 const m=/^alerts\/([^/]+)\/(sensor|energia)$/.exec(topic);if(!m||!isDeviceId(m[1]))return null;
 const p=parseObject(payload),severity=p.severity||'unknown';if(!['info','warning','high','critical','unknown'].includes(severity))throw new Error('Severidad invalida');
 const d=p.details;if(!d||typeof d!=='object'||Array.isArray(d))throw new Error('details requerido para alerta');
 if(m[2]==='sensor'&&(!['temp_heladera_c','temp_freezer_c'].includes(d.campo)||!['sin_respuesta','fuera_de_rango','recuperada'].includes(d.motivo)))throw new Error('Alerta de sonda invalida');
 if(m[2]==='energia'&&!['bateria','red'].includes(d.alimentacion))throw new Error('Alimentacion invalida');
 return {deviceId:m[1].toLowerCase(),type:m[2],severity,message:typeof p.message==='string'?p.message:'',details:d,measuredAt:utcDate(p.ts)};
}
