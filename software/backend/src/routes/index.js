import { Router } from "express";
import sensorRoutes from "./sensor-data-routes.js";

const router = Router();

// Montar rutas de sensores y telemetría bajo el prefijo correspondiente
router.use("/", sensorRoutes);

export default router;
