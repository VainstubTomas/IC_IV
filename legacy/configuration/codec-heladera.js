/** Codec de mediciones IC_IV. Perfil propio de la aplicacion aura.
 * FPort 1: contador uint16 + temp_c int16 /100; 0x7FFF = sensor sin lectura.
 * FPort 10: propuesta set_config de 7 bytes. Reporte FPort 11 pendiente de contrato.
 * No publicar el reporte como values: no contiene mediciones.
 */
function decodeUplink(input) {
  if (input.fPort !== 1) return { errors: ["FPort no admitido para mediciones; reporte de configuracion pendiente de acuerdo con AURA"] };
  if (!input.bytes || input.bytes.length !== 4) return { errors: ["Se esperaban 4 bytes"] };
  var raw = (input.bytes[2] << 8) | input.bytes[3];
  if (raw === 0x7FFF) return { data: {}, warnings: ["DS18B20 sin lectura; canal de falla pendiente de contrato"] };
  if (raw & 0x8000) raw -= 65536;
  if (raw < -6000 || raw > 13000) return { errors: ["Temperatura fuera de rango"] };
  return { data: { temp_c: raw / 100 } };
}

function validarConfig(p) {
  if (!p || typeof p !== "object" || Array.isArray(p)) throw new Error("params obligatorio");
  var keys = ["interval_s", "confirmed", "adr", "dr", "offset_c"];
  Object.keys(p).forEach(function(k) { if (keys.indexOf(k) < 0) throw new Error("Parametro desconocido: " + k); });
  if (!Number.isInteger(p.interval_s) || p.interval_s < 20 || p.interval_s > 3600) throw new Error("interval_s: entero 20..3600");
  if (typeof p.confirmed !== "boolean" || typeof p.adr !== "boolean") throw new Error("confirmed y adr: booleanos");
  if (!Number.isInteger(p.dr) || p.dr < 2 || p.dr > 5) throw new Error("dr: entero 2..5");
  if (typeof p.offset_c !== "number" || !Number.isFinite(p.offset_c) || Math.abs(p.offset_c) > 5 || Math.abs(p.offset_c * 100 - Math.round(p.offset_c * 100)) > 1e-8) throw new Error("offset_c: -5..5, precision 0.01");
}
function encodeDownlink(input) {
  try {
    if (!input.data || input.data.command !== "set_config") throw new Error("Solo set_config admitido");
    var p = input.data.params;
    validarConfig(p);
    var offset = Math.round(p.offset_c * 100) & 0xFFFF;
    return { fPort: 10, bytes: [1, p.interval_s >> 8, p.interval_s & 255,
      (p.confirmed ? 1 : 0) | (p.adr ? 2 : 0), p.dr, offset >> 8, offset & 255] };
  } catch (e) { return { errors: [e.message] }; }
}
// Helper de laboratorio; NO se devuelve desde decodeUplink como values.
function decodeConfigReport(bytes) {
  if (!bytes || bytes.length !== 7 || bytes[0] !== 1 || (bytes[3] & 252)) throw new Error("Reporte invalido");
  var raw = (bytes[5] << 8) | bytes[6];
  if (raw & 32768) raw -= 65536;
  var p = { interval_s: (bytes[1] << 8) | bytes[2], confirmed: !!(bytes[3] & 1),
    adr: !!(bytes[3] & 2), dr: bytes[4], offset_c: raw / 100 };
  validarConfig(p);
  return p;
}
