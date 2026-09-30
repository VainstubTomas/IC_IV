// Solo campos documentados en los mails. No interpreta status/response sin contrato v2.
export const isDeviceId = id => typeof id === 'string' && /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(id);
export function parseAuraData(topic, payload) {
  const match = /^devices\/([^/]+)\/data$/.exec(topic);
  if (!match || !isDeviceId(match[1])) return null;
  const json = JSON.parse(payload);
  const temperature = json?.values?.temp_c;
  if (typeof temperature !== 'number' || !Number.isFinite(temperature) || temperature < -60 || temperature > 130) return null;
  return { deviceId: match[1], temperature, source: 'mqtt' };
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
