import sensorDataService from "../services/sensor-data-service.js";
import mqttConfig from "../config/mqtt/mqtt-config.js";

class SensorDataController {
  /**
   * POST /api/telemetria
   * Inserta un nuevo registro de telemetría enviado por HTTP
   */
  async postTelemetry(req, res) {
    try {
      const { temperatura, temperature, rssi, deviceId, source } = req.body || {};

      const tempToUse = temperature !== undefined ? temperature : temperatura;

      if (tempToUse === undefined || tempToUse === null) {
        return res.status(400).json({
          status: "error",
          message: "El campo 'temperatura' (o 'temperature') es obligatorio y debe ser un número."
        });
      }

      const result = await sensorDataService.saveTelemetry({
        temperature: tempToUse,
        rssi,
        deviceId,
        source: source || "http"
      });

      return res.status(201).json({
        status: "success",
        message: "Telemetría registrada correctamente",
        data: result
      });
    } catch (error) {
      console.error("[controller] Error en postTelemetry:", error);
      return res.status(500).json({
        status: "error",
        message: error.message || "Error interno del servidor al guardar telemetría"
      });
    }
  }

  /**
   * GET /api/telemetria/latest y GET /api/telemetria
   * Retorna el último registro de telemetría registrado
   */
  async getLatestTelemetry(req, res) {
    try {
      const latest = await sensorDataService.getLatestTelemetry();
      return res.status(200).json(latest);
    } catch (error) {
      console.error("[controller] Error en getLatestTelemetry:", error);
      return res.status(500).json({
        status: "error",
        message: "Error al consultar la última lectura de telemetría"
      });
    }
  }

  /**
   * GET /api/telemetria/history
   * Retorna el histórico de telemetría
   */
  async getHistory(req, res) {
    try {
      const limit = parseInt(req.query.limit) || 50;
      const history = await sensorDataService.getTelemetryHistory(limit);
      return res.status(200).json({
        status: "success",
        count: history.length,
        data: history
      });
    } catch (error) {
      console.error("[controller] Error en getHistory:", error);
      return res.status(500).json({
        status: "error",
        message: "Error al consultar el historial de telemetría"
      });
    }
  }

  /**
   * POST /api/leer
   * Forzar lectura inmediata enviando comando MQTT hacia el nodo
   */
  async forceRead(req, res) {
    try {
      const published = mqttConfig.publishCommand("analog", "force_read");

      // Consultar la última lectura para responder
      const latest = await sensorDataService.getLatestTelemetry();

      return res.status(200).json({
        status: "ok",
        mensaje: "Comando de lectura forzada emitido",
        mqttSent: published,
        telemetria: latest
      });
    } catch (error) {
      console.error("[controller] Error en forceRead:", error);
      return res.status(500).json({
        status: "error",
        message: "Error al forzar la lectura del dispositivo"
      });
    }
  }

  /**
   * GET /api/health
   * Verificación de salud del backend
   */
  healthCheck(req, res) {
    return res.status(200).json({
      status: "online",
      service: "IC_IV Telemetry Backend",
      uptimeSeconds: Math.floor(process.uptime()),
      timestamp: new Date().toISOString()
    });
  }
}

export default new SensorDataController();
