// Contrato MQTT AURA v2.0: secciones 2, 3 y 5.
export const isDeviceId = id => typeof id === 'string' && /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(id);
export function parseAuraData(topic, payload) {
  const match = /^devices\/([^/]+)\/data$/.exec(topic);
  if (!match || !isDeviceId(match[1])) return null;
  const json = parseObject(payload);
  const temperature = json?.values?.temp_c;
  if (typeof temperature !== 'number' || !Number.isFinite(temperature) || temperature < -60 || temperature > 130) return null;
  if (json.ingest_id !== undefined && !isDeviceId(json.ingest_id)) throw new Error('ingest_id debe ser UUID');
  return { deviceId: match[1].toLowerCase(), temperature, source: 'mqtt',
    ...(json.ingest_id !== undefined ? { ingest_id: json.ingest_id.toLowerCase() } : {}) };
}
export function validateConfig(p) {
  const keys = ['interval_s', 'confirmed', 'adr', 'dr', 'offset_c'];
  if (!p || typeof p !== 'object' || Array.isArray(p) || Object.keys(p).some(k => !keys.includes(k))) throw new Error('Configuracion completa requerida: interval_s, confirmed, adr, dr, offset_c');
  if (!Number.isInteger(p.interval_s) || p.interval_s < 20 || p.interval_s > 3600) throw new Error('Intervalo: entero 20..3600 segundos');
  if (typeof p.confirmed !== 'boolean' || typeof p.adr !== 'boolean') throw new Error('confirmed y adr deben ser booleanos');
  if (!Number.isInteger(p.dr) || p.dr < 2 || p.dr > 5) throw new Error('DR permitido: 2..5');
  if (typeof p.offset_c !== 'number' || !Number.isFinite(p.offset_c) || Math.abs(p.offset_c) > 5 || Math.abs(p.offset_c * 100 - Math.round(p.offset_c * 100)) > 1e-8) throw new Error('Offset: -5..5 C, precision 0.01');
  return p;
}

export const RESPONSE_STATES = Object.freeze(['encolado', 'transmitido', 'recibido', 'rechazado']);
export function parseObject(payload) {
  const json = JSON.parse(payload);
  if (!json || typeof json !== 'object' || Array.isArray(json)) throw new Error('El payload debe ser objeto JSON');
  return json;
}
export function parseAuraEvent(topic, payload, kind) {
  if (!['status', 'response'].includes(kind)) return null;
  const match = new RegExp(`^devices/([^/]+)/${kind}$`).exec(topic);
  if (!match || !isDeviceId(match[1])) return null;
  const json = parseObject(payload);
  const allowed = kind === 'status' ? ['online', 'offline'] : RESPONSE_STATES;
  if (!allowed.includes(json.status)) throw new Error(`Estado ${kind} no admitido`);
  const details = json.details === undefined ? {} : json.details;
  if (!details || typeof details !== 'object' || Array.isArray(details)) throw new Error('details debe ser objeto');
  return { deviceId: match[1].toLowerCase(), status: json.status, details };
}
// Nunca interpretar recibido como ejecucion. Evitar regresiones por duplicados tardios.
export function previousResponseStates(status) {
  return {
    encolado: ['publishing', 'pending', 'encolado'],
    transmitido: ['publishing', 'pending', 'encolado', 'transmitido'],
    recibido: ['publishing', 'pending', 'encolado', 'transmitido', 'recibido'],
    rechazado: ['publishing', 'pending', 'encolado', 'transmitido', 'rechazado']
  }[status] || [];
}
