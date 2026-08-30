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
  async saveTelemetry({ temperature, temperatura, rssi, deviceId = "Heladera1", source = "http" }) {
    // Aceptar tanto 'temperature' como 'temperatura'
    const tempVal = temperature !== undefined ? Number(temperature) : Number(temperatura);
    
    if (isNaN(tempVal)) {
      throw new Error("El valor de temperatura debe ser un número válido.");
    }

    const rssiVal = rssi !== undefined && rssi !== null && !isNaN(Number(rssi)) ? Number(rssi) : null;

    const record = await sensorDataRepository.create({
      temperature: tempVal,
      rssi: rssiVal,
      deviceId: deviceId || "Heladera1",
      source
    });

    // Chequeo de umbral no bloqueante: un fallo de mail/DB acá nunca debe romper el guardado.
    alertService
      .checkThresholdAndNotify({ deviceId: record.deviceId, temperature: record.temperature })
      .catch((err) => console.error("[sensor-service] Error al chequear umbrales:", err.message));

    return {
      id: record._id,
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
    const latest = await sensorDataRepository.getLatest();

    if (!latest) {
      return {
        temperatura: 3.8,
        temperature: 3.8,
        rssi: -75,
        deviceId: "Heladera1",
        timestamp: formatTimestamp(new Date()),
        isInitialDefault: true
      };
    }

    return {
      id: latest._id,
      temperatura: latest.temperature,
      temperature: latest.temperature,
      rssi: latest.rssi ?? -80,
      deviceId: latest.deviceId,
      timestamp: formatTimestamp(latest.createdAt),
      createdAt: latest.createdAt
    };
  }

  /**
   * Obtiene el listado histórico de lecturas
   */
  async getTelemetryHistory(limit = 50) {
    const records = await sensorDataRepository.getHistory(limit);
    return records.map((r) => ({
      id: r._id,
      temperatura: r.temperature,
      temperature: r.temperature,
      rssi: r.rssi,
      deviceId: r.deviceId,
      timestamp: formatTimestamp(r.createdAt),
      createdAt: r.createdAt
    }));
  }

  /**
   * Parsea e ingesta automáticamente mensajes recibidos desde el Broker MQTT
   * Soporta formato de texto de firmware ("Heladera1:4.20") o JSON ("{"temperature": 4.2}")
   */
  async parseAndSaveMqttMessage(topic, payloadStr) {
    try {
      let parsedTemp = null;
      let parsedRssi = null;
      let parsedDevice = "Heladera1";

      const trimmed = payloadStr.trim();

      // Caso 1: Formato JSON
      if (trimmed.startsWith("{") && trimmed.endsWith("}")) {
        const json = JSON.parse(trimmed);
        parsedTemp = json.temperature ?? json.temperatura ?? json.temp;
        parsedRssi = json.rssi ?? json.signal;
        parsedDevice = json.deviceId ?? json.device ?? "Heladera1";
      } 
      // Caso 2: Formato texto de firmware ESP32 "Heladera1:3.85" o "3.85"
      else if (trimmed.includes(":")) {
        const parts = trimmed.split(":");
        parsedDevice = parts[0] || "Heladera1";
        parsedTemp = parseFloat(parts[1]);
      } else {
        parsedTemp = parseFloat(trimmed);
      }

      if (parsedTemp !== null && !isNaN(parsedTemp)) {
        const saved = await this.saveTelemetry({
          temperature: parsedTemp,
          rssi: parsedRssi,
          deviceId: parsedDevice,
          source: "mqtt"
        });
        console.log(`[sensor-service] 💾 Telemetría MQTT persistida en DB: ${parsedDevice} -> ${parsedTemp}°C`);
        return saved;
      }
    } catch (err) {
      console.error("[sensor-service] Error procesando payload MQTT:", err.message);
    }
    return null;
  }
}

export default new SensorDataService();
