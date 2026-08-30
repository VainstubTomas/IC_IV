import { Router } from "express";
import sensorRoutes from "./sensor-data-routes.js";

const router = Router();

// Rutas versionadas: agregar nuevas versiones (/v2, etc.) sin tocar app.js
router.use("/v1", sensorRoutes);

export default router;
