import sensorDataService from "../services/sensor-data-service.js";

class SensorDataController {
  /**
   * POST /api/v1/telemetria
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
   * GET /api/v1/telemetria/latest
   * Retorna el último registro de telemetría registrado
   */
  async getLatestTelemetry(req, res) {
    try {
      const latest = await sensorDataService.getLatestTelemetry(req.query.sensor || 'heladera');
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
   * GET /api/v1/telemetria/history
   * Retorna el histórico de telemetría
   */
  async getHistory(req, res) {
    try {
      const limit = parseInt(req.query.limit) || 50;
      const history = await sensorDataService.getTelemetryHistory(limit,req.query.sensor || 'heladera');
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
   * POST /api/v1/leer
   * Forzar lectura inmediata enviando comando MQTT hacia el nodo
   */
  async forceRead(req, res) {
    return res.status(409).json({ message: 'La lectura se realiza segun el intervalo propio de cada sonda. Comando de lectura forzada no definido.' });
  }

  /**
   * GET /api/v1/health
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
