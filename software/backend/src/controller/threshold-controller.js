import config from "../config/config.js";
import thresholdService from "../services/threshold-service.js";

class ThresholdController {
  /**
   * GET /api/v1/umbrales
   * Retorna el umbral de temperatura vigente
   */
  async getThresholds(req, res) {
    try {
      const deviceId = req.query.deviceId || config.AURA_DEVICE_ID;
      const thresholds = await thresholdService.getThresholds(deviceId,req.query.sensor || 'heladera');
      return res.status(200).json(thresholds);
    } catch (error) {
      console.error("[controller] Error en getThresholds:", error);
      return res.status(500).json({
        status: "error",
        message: "Error al consultar los umbrales de temperatura"
      });
    }
  }

  /**
   * POST /api/v1/umbrales
   * Crea o actualiza el umbral de temperatura
   */
  async postThresholds(req, res) {
    try {
      const { min, max, deviceId, sensor } = req.body || {};

      if (min === undefined || max === undefined) {
        return res.status(400).json({
          status: "error",
          message: "Los campos 'min' y 'max' son obligatorios."
        });
      }

      const result = await thresholdService.saveThresholds({ deviceId, min, max, sensor });

      return res.status(200).json({
        status: "success",
        message: "Umbrales guardados correctamente",
        data: result
      });
    } catch (error) {
      console.error("[controller] Error en postThresholds:", error);
      return res.status(400).json({
        status: "error",
        message: error.message || "Error al guardar los umbrales de temperatura"
      });
    }
  }
}

export default new ThresholdController();
