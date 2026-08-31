import { Router } from "express";
import thresholdController from "../controller/threshold-controller.js";

const router = Router();

/**
 * @openapi
 * /umbrales:
 *   get:
 *     summary: Obtiene el umbral de temperatura vigente de un dispositivo
 *     tags: [Umbrales]
 *     parameters:
 *       - name: deviceId
 *         in: query
 *         required: false
 *         description: Identificador del dispositivo (por defecto "Heladera1")
 *         schema: { type: string, example: Heladera1 }
 *     responses:
 *       200:
 *         description: Umbral vigente
 *         content:
 *           application/json:
 *             schema:
 *               $ref: '#/components/schemas/Threshold'
 *       500:
 *         description: Error al consultar los umbrales
 */
router.get("/umbrales", thresholdController.getThresholds);

/**
 * @openapi
 * /umbrales:
 *   post:
 *     summary: Crea o actualiza el umbral de temperatura de un dispositivo
 *     tags: [Umbrales]
 *     requestBody:
 *       required: true
 *       content:
 *         application/json:
 *           schema:
 *             type: object
 *             required: [min, max]
 *             properties:
 *               deviceId: { type: string, example: Heladera1 }
 *               min: { type: number, example: -25 }
 *               max: { type: number, example: -15 }
 *     responses:
 *       200:
 *         description: Umbrales guardados correctamente
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 status: { type: string, example: success }
 *                 message: { type: string }
 *                 data:
 *                   $ref: '#/components/schemas/Threshold'
 *       400:
 *         description: Faltan los campos min y/o max
 */
router.post("/umbrales", thresholdController.postThresholds);

export default router;
