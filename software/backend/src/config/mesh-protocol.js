import { isDeviceId, parseObject } from './mqtt/aura-protocol.js';
export const SENSORS = Object.freeze(['heladera', 'freezer']);
export function validateSensor(sensor = 'heladera') {
  if (!SENSORS.includes(sensor)) throw new Error('Sonda permitida: heladera o freezer');
  return sensor;
}
export function validateMeshConfig(p) {
  const keys = ['sensor', 'interval_s', 'min_c', 'max_c', 'recovery_s'];
  if (!p || typeof p !== 'object' || Array.isArray(p) || Object.keys(p).length !== 5 || Object.keys(p).some(k => !keys.includes(k))) throw new Error('Configuracion completa: sensor, interval_s, min_c, max_c, recovery_s');
  validateSensor(p.sensor);
  if (!Number.isInteger(p.interval_s) || p.interval_s < 5 || p.interval_s > 86400) throw new Error('Muestreo: entero de 5 a 86400 segundos');
  if (!Number.isInteger(p.recovery_s) || p.recovery_s < 60 || p.recovery_s > 86400) throw new Error('Recuperacion: entero de 60 a 86400 segundos');
  for (const key of ['min_c', 'max_c']) if (typeof p[key] !== 'number' || !Number.isFinite(p[key]) || p[key] < -55 || p[key] > 125 || Math.abs(p[key] * 100 - Math.round(p[key] * 100)) > 1e-8) throw new Error('Umbrales: -55 a 125 C, precision 0.01 C');
  if (p.min_c >= p.max_c) throw new Error('Minimo debe ser menor al maximo');
  return { ...p };
}
export function defaultMeshConfig(sensor) {
  validateSensor(sensor);
  return { sensor, interval_s: sensor === 'heladera' ? 60 : 300, min_c: sensor === 'heladera' ? 2 : -25, max_c: sensor === 'heladera' ? 6 : -15, recovery_s: 300 };
}
export function sensorDeviceId(config, sensor = 'heladera') {
  validateSensor(sensor);
  return sensor === 'heladera' ? config.AURA_DEVICE_ID : config.AURA_FREEZER_DEVICE_ID;
}
export function parseMeshMetadata(topic, payload) {
  const match = /^devices\/([^/]+)\/data$/.exec(topic);
  if (!match || !isDeviceId(match[1])) return null;
  const p = parseObject(payload);
  const metadata = { deviceId: match[1].toLowerCase() };
  for (const [key, result] of [['measured_at', 'measuredAt'], ['power_cut_at', 'powerCutAt']]) {
    if (p[key] !== undefined) {
      if (typeof p[key] !== 'string' || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/.test(p[key]) || !Number.isFinite(Date.parse(p[key])) || new Date(p[key]).toISOString().slice(0, 19) + 'Z' !== p[key]) throw new Error('Timestamp mesh invalido');
      metadata[result] = new Date(p[key]);
    }
  }
  for (const [key, result] of [['power_first','powerFirst'], ['on_battery','onBattery']]) {
    if (p[key] !== undefined && typeof p[key] !== 'boolean') throw new Error('Estado de energia invalido');
    if (p[key] !== undefined) metadata[result] = p[key];
  }
  if (p.ingest_id !== undefined) {
    if (!isDeviceId(p.ingest_id)) throw new Error('ingest_id invalido');
    metadata.ingest_id = p.ingest_id.toLowerCase();
  }
  return metadata;
}
// Se exige el reporte explicito y coherente del firmware, no solo status=recibido.
export function appliedMeshConfig(event, params) {
  const r = event.details?.iciv;
  if (event.status !== 'recibido' || r?.applied !== true) return null;
  const candidate = validateMeshConfig({ sensor:r.sensor, interval_s:r.interval_s, min_c:r.min_c, max_c:r.max_c, recovery_s:r.recovery_s });
  if (['sensor','interval_s','min_c','max_c','recovery_s'].some(k => candidate[k] !== params[k])) throw new Error('Reporte del nodo no coincide con el comando');
  return candidate;
}
