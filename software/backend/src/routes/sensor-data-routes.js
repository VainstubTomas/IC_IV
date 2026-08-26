import { Router } from "express";
import sensorDataController from "../controller/sensor-data-controller.js";

const router = Router();

// Endpoint de salud
router.get("/health", sensorDataController.healthCheck);

// Endpoints de telemetría
router.post("/telemetria", sensorDataController.postTelemetry);
router.get("/telemetria/latest", sensorDataController.getLatestTelemetry);
router.get("/telemetria", sensorDataController.getLatestTelemetry); // Alias directo para el frontend
router.get("/telemetria/history", sensorDataController.getHistory);

// Endpoint de interacción / control de hardware
router.post("/leer", sensorDataController.forceRead);

export default router;
