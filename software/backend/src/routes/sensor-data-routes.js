import { Router } from "express";
import sensorDataController from "../controller/sensor-data-controller.js";

const router = Router();

/**
 * @openapi
 * /health:
 *   get:
 *     summary: Verifica el estado del backend
 *     tags: [Sistema]
 *     responses:
 *       200:
 *         description: El servicio está online
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 status: { type: string, example: online }
 *                 service: { type: string }
 *                 uptimeSeconds: { type: number }
 *                 timestamp: { type: string, format: date-time }
 */
router.get("/health", sensorDataController.healthCheck);

/**
 * @openapi
 * /telemetria:
 *   post:
 *     summary: Registra una nueva lectura de temperatura
 *     tags: [Telemetría]
 *     requestBody:
 *       required: true
 *       content:
 *         application/json:
 *           schema:
 *             type: object
 *             required: [temperature]
 *             properties:
 *               temperature: { type: number, example: -18.2 }
 *               deviceId: { type: string, example: 650e8400-e29b-41d4-a716-446655440001 }
 *               rssi: { type: number }
 *               source: { type: string, enum: [lora, mqtt, http, manual] }
 *     responses:
 *       201:
 *         description: Telemetría registrada
 *       400:
 *         description: Falta el campo temperature
 */
router.post("/telemetria", sensorDataController.postTelemetry);

/**
 * @openapi
 * /telemetria/latest:
 *   get:
 *     summary: Obtiene la última lectura de temperatura registrada
 *     tags: [Telemetría]
 *     responses:
 *       200:
 *         description: Última lectura registrada
 *         content:
 *           application/json:
 *             schema:
 *               $ref: '#/components/schemas/SensorData'
 *       500:
 *         description: Error al consultar la última lectura
 */
router.get("/telemetria/latest", sensorDataController.getLatestTelemetry);

/**
 * @openapi
 * /telemetria/history:
 *   get:
 *     summary: Obtiene el histórico de lecturas de temperatura
 *     tags: [Telemetría]
 *     parameters:
 *       - name: limit
 *         in: query
 *         required: false
 *         description: Cantidad máxima de registros a devolver (por defecto 50)
 *         schema: { type: integer, example: 50 }
 *     responses:
 *       200:
 *         description: Histórico de lecturas
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 status: { type: string, example: success }
 *                 count: { type: integer }
 *                 data:
 *                   type: array
 *                   items:
 *                     $ref: '#/components/schemas/SensorData'
 *       500:
 *         description: Error al consultar el historial
 */
router.get("/telemetria/history", sensorDataController.getHistory);

/**
 * @openapi
 * /leer:
 *   post:
 *     summary: Fuerza una lectura inmediata del sensor vía comando MQTT
 *     tags: [Telemetría]
 *     responses:
 *       200:
 *         description: Comando de lectura forzada emitido
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 status: { type: string, example: ok }
 *                 mensaje: { type: string }
 *                 mqttSent: { type: boolean, description: "false si el broker MQTT no está conectado" }
 *                 telemetria:
 *                   $ref: '#/components/schemas/SensorData'
 *       500:
 *         description: Error al forzar la lectura del dispositivo
 */
router.post("/leer", sensorDataController.forceRead);

export default router;
