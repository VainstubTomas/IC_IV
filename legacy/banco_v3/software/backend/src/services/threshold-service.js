import { isDeviceId } from "../config/mqtt/aura-protocol.js";
import thresholdRepository from "../repository/threshold-repository.js";
import config from "../config/config.js";
import {validateSensor} from '../config/mesh-protocol.js';

class ThresholdService {
  /**
   * Obtiene el umbral vigente de un dispositivo
   */
  async getThresholds(deviceId = config.AURA_DEVICE_ID, sensor = "heladera") {
    validateSensor(sensor);
    if (!isDeviceId(deviceId)) return { deviceId: null, min: null, max: null, configured: false };
    const config = await thresholdRepository.getByDevice(deviceId,sensor);

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
  async saveThresholds({ deviceId = config.AURA_DEVICE_ID, min, max, sensor = "heladera" }) {
    validateSensor(sensor);
    if (!isDeviceId(deviceId)) throw new Error("Configurar el UUID AURA para los umbrales");
    const minVal = Number(min);
    const maxVal = Number(max);

    if (min === null || max === null || !Number.isFinite(minVal) || !Number.isFinite(maxVal)) {
      throw new Error("Los umbrales 'min' y 'max' deben ser números válidos.");
    }

    if (minVal >= maxVal) {
      throw new Error("El umbral mínimo debe ser menor al máximo.");
    }

    const saved = await thresholdRepository.upsert({ deviceId, sensor, min: minVal, max: maxVal });

    // Umbrales de email: viven solo en la plataforma, no son configuracion del nodo.

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
