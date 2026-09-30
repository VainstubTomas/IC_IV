import config from "../config/config.js";
import { parseAuraData, isDeviceId } from "../config/mqtt/aura-protocol.js";
import sensorDataRepository from "../repository/sensor-data-repository.js";
import alertService from "./alert-service.js";

/**
 * Formatea una fecha a string legible YYYY-MM-DD HH:mm:ss
 */
function formatTimestamp(date) {
  if (!date) return new Date().toISOString().replace('T', ' ').substring(0, 19);
  const d = new Date(date);
  const pad = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

class SensorDataService {
  /**
   * Guarda una nueva lectura de telemetría validada
   */
  async saveTelemetry({ temperature, temperatura, rssi, deviceId = config.AURA_DEVICE_ID, source = "http", ingest_id }) {
    if (!isDeviceId(deviceId)) throw new Error("Se requiere UUID AURA del dispositivo");
    // Aceptar tanto 'temperature' como 'temperatura'
    const tempVal = temperature !== undefined ? Number(temperature) : Number(temperatura);
    
    if (temperature === null || temperatura === null || !Number.isFinite(tempVal)) {
      throw new Error("El valor de temperatura debe ser un número válido.");
    }

    const rssiVal = rssi !== undefined && rssi !== null && !isNaN(Number(rssi)) ? Number(rssi) : null;

    const { record, inserted } = await sensorDataRepository.create({
      temperature: tempVal,
      rssi: rssiVal,
      deviceId,
      source,
      ...(ingest_id !== undefined ? { ingest_id } : {})
    });

    // Chequeo de umbral no bloqueante: un fallo de mail/DB acá nunca debe romper el guardado.
    if (inserted) alertService
      .checkThresholdAndNotify({ deviceId: record.deviceId, temperature: record.temperature })
      .catch((err) => console.error("[sensor-service] Error al chequear umbrales:", err.message));

    return {
      id: record._id,
      ingest_id: record.ingest_id,
      duplicate: !inserted,
      temperatura: record.temperature,
      temperature: record.temperature,
      rssi: record.rssi,
      deviceId: record.deviceId,
      timestamp: formatTimestamp(record.createdAt),
      createdAt: record.createdAt
    };
  }

  /**
   * Obtiene la última lectura registrada
   */
  async getLatestTelemetry() {
    if (!isDeviceId(config.AURA_DEVICE_ID)) return null;
    const latest = await sensorDataRepository.getLatest(config.AURA_DEVICE_ID);

    if (!latest) {
      return null;
    }

    return {
      id: latest._id,
      ingest_id: latest.ingest_id,
      temperatura: latest.temperature,
      temperature: latest.temperature,
      rssi: latest.rssi ?? null,
      deviceId: latest.deviceId,
      timestamp: formatTimestamp(latest.createdAt),
      createdAt: latest.createdAt
    };
  }

  /**
   * Obtiene el listado histórico de lecturas
   */
  async getTelemetryHistory(limit = 50) {
    if (!isDeviceId(config.AURA_DEVICE_ID)) return [];
    const records = await sensorDataRepository.getHistory(limit, config.AURA_DEVICE_ID);
    return records.map((r) => ({
      id: r._id,
      ingest_id: r.ingest_id,
      temperatura: r.temperature,
      temperature: r.temperature,
      rssi: r.rssi,
      deviceId: r.deviceId,
      timestamp: formatTimestamp(r.createdAt),
      createdAt: r.createdAt
    }));
  }

  /**
   * Ingesta mediciones values.temp_c del broker AURA; UUID siempre desde el topico.
   */
  async parseAndSaveMqttMessage(topic, payloadStr) {
    let reading;
    try { reading = parseAuraData(topic, payloadStr); }
    catch (err) { console.warn('[sensor-service] Payload AURA rechazado:', err.message); return null; }
    if (reading && config.AURA_DEVICE_ID && reading.deviceId !== config.AURA_DEVICE_ID) return null;
    // Errores de DB se propagan: no confirmar MQTT antes de persistir.
    return reading ? await this.saveTelemetry(reading) : null;
  }

}

export default new SensorDataService();
