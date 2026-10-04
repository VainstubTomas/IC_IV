import Threshold from "../models/threshold-model.js";

/**
 * Repositorio de acceso a datos para los umbrales de temperatura
 */
class ThresholdRepository {
  /**
   * Obtiene el umbral configurado para un dispositivo
   * @param {string} deviceId
   */
  async getByDevice(deviceId) {
    return await Threshold.findOne({ deviceId }).lean();
  }

  /**
   * Crea o actualiza el umbral de un dispositivo
   * @param {Object} data - { deviceId, min, max }
   */
  async upsert({ deviceId, min, max }) {
    return await Threshold.findOneAndUpdate(
      { deviceId },
      { deviceId, min, max },
      { upsert: true, new: true, setDefaultsOnInsert: true }
    ).lean();
  }
}

export default new ThresholdRepository();
