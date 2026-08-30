import { Router } from "express";
import thresholdController from "../controller/threshold-controller.js";

const router = Router();

// Endpoints de umbrales de temperatura
router.get("/umbrales", thresholdController.getThresholds);
router.post("/umbrales", thresholdController.postThresholds);

export default router;
