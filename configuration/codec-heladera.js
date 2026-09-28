/**
 * Codec JavaScript para ChirpStack v4 — Telemetría Heladera LoRaWAN UNRaf
 *
 * Configurar en la interfaz web de ChirpStack:
 *   Device profiles -> [Perfil de tu dispositivo] -> Codec -> Payload codec: JavaScript functions
 *
 * Desempaqueta los 4 bytes enviados por nodo_lorawan.ino:
 *   - Bytes 0-1: Contador de secuencia (uint16 big-endian)
 *   - Bytes 2-3: Temperatura de la sonda en centésimas de grado (int16 big-endian con signo)
 */

function decodeUplink(input) {
  if (!input.bytes || input.bytes.length < 4) {
    return {
      errors: ["Payload inválido: se esperaban al menos 4 bytes (contador + temperatura)"]
    };
  }

  // 1. Decodificar contador (uint16)
  var contador = (input.bytes[0] << 8) | input.bytes[1];

  // 2. Decodificar temperatura (int16 con signo para temperaturas bajo cero)
  var temp_raw = (input.bytes[2] << 8) | input.bytes[3];
  if (temp_raw & 0x8000) {
    temp_raw = temp_raw - 0x10000;
  }
  var temperatura = temp_raw / 100.0;

  return {
    data: {
      contador: contador,
      temperatura: temperatura
    }
  };
}

// Codificador de downlink opcional
function encodeDownlink(input) {
  var valor = input.data.contador || 0;
  return {
    bytes: [(valor >> 8) & 0xff, valor & 0xff]
  };
}
