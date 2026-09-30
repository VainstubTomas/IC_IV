import SensorData from "../models/sensor-data-model.js";

/**
 * Repositorio de acceso a datos para telemetría de sensores
 */
class SensorDataRepository {
  /**
   * Guarda un nuevo registro de telemetría en MongoDB
   * @param {Object} data - { temperature, rssi, deviceId, source }
   */
  async create(data) {
    const record = new SensorData(data);
    return await record.save();
  }

  /**
   * Obtiene la lectura más reciente
   */
  async getLatest(deviceId) {
    return await SensorData.findOne({ deviceId })
      .sort({ createdAt: -1 })
      .lean();
  }

  /**
   * Obtiene el historial de lecturas ordenadas cronológicamente descendente
   * @param {number} limit - Cantidad máxima de registros
   */
  async getHistory(limit = 50, deviceId) {
    const parsedLimit = Math.min(Math.max(1, parseInt(limit) || 50), 500);
    return await SensorData.find({ deviceId })
      .sort({ createdAt: -1 })
      .limit(parsedLimit)
      .lean();
  }

  /**
   * Obtiene estadísticas térmicas básicas (mínimo, máximo, promedio, total)
   */
  async getStats() {
    const result = await SensorData.aggregate([
      {
        $group: {
          _id: "$deviceId",
          minTemp: { $min: "$temperature" },
          maxTemp: { $max: "$temperature" },
          avgTemp: { $avg: "$temperature" },
          totalReadings: { $sum: 1 },
          lastReading: { $max: "$createdAt" }
        }
      }
    ]);
    return result[0] || null;
  }
}

export default new SensorDataRepository();
