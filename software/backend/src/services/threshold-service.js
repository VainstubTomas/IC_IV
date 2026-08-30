import thresholdRepository from "../repository/threshold-repository.js";
import mqttConfig from "../config/mqtt/mqtt-config.js";

class ThresholdService {
  /**
   * Obtiene el umbral vigente de un dispositivo
   */
  async getThresholds(deviceId = "Heladera1") {
    const config = await thresholdRepository.getByDevice(deviceId);

    if (!config) {
      return {
        deviceId,
        min: null,
        max: null,
        configured: false
      };
    }

    return {
      deviceId: config.deviceId,
      min: config.min,
      max: config.max,
      configured: true,
      updatedAt: config.updatedAt
    };
  }

  /**
   * Guarda el umbral de un dispositivo
   */
  async saveThresholds({ deviceId = "Heladera1", min, max }) {
    const minVal = Number(min);
    const maxVal = Number(max);

    if (isNaN(minVal) || isNaN(maxVal)) {
      throw new Error("Los umbrales 'min' y 'max' deben ser números válidos.");
    }

    if (minVal >= maxVal) {
      throw new Error("El umbral mínimo debe ser menor al máximo.");
    }

    const saved = await thresholdRepository.upsert({ deviceId, min: minVal, max: maxVal });

    mqttConfig.publishCommand(
      "threshold",
      JSON.stringify({ deviceId: saved.deviceId, min: saved.min, max: saved.max }),
      { retain: true }
    );

    return {
      deviceId: saved.deviceId,
      min: saved.min,
      max: saved.max,
      configured: true,
      updatedAt: saved.updatedAt
    };
  }
}

export default new ThresholdService();
