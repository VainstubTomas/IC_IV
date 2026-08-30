import { Router } from "express";
import sensorRoutes from "./sensor-data-routes.js";
import thresholdRoutes from "./threshold-routes.js";
import alertEmailRoutes from "./alert-email-routes.js";

const router = Router();

// Rutas versionadas: agregar nuevas versiones (/v2, etc.) sin tocar app.js
router.use("/v1", sensorRoutes);
router.use("/v1", thresholdRoutes);
router.use("/v1", alertEmailRoutes);

export default router;
