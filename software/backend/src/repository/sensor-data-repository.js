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
    const record = new SensorData({ ...data, orderAt:data.measuredAt || new Date() });
    try {
      return { record: await record.save(), inserted: true };
    } catch (err) {
      if (data.ingest_id && err.code === 11000) {
        const existing = await SensorData.findOne({ ingest_id: data.ingest_id }).lean();
        if (existing) return { record: existing, inserted: false };
      }
      throw err;
    }
  }

  /**
   * Obtiene la lectura más reciente
   */
  async getLatest(deviceId) {
    return await SensorData.findOne({ deviceId })
      .sort({ orderAt: -1, createdAt: -1 })
      .lean();
  }

  /**
   * Obtiene el historial de lecturas ordenadas cronológicamente descendente
   * @param {number} limit - Cantidad máxima de registros
   */
  async getHistory(limit = 50, deviceId) {
    const parsedLimit = Math.min(Math.max(1, parseInt(limit) || 50), 500);
    return await SensorData.find({ deviceId })
      .sort({ orderAt: -1, createdAt: -1 })
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
