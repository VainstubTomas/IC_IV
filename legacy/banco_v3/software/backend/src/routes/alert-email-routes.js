import { Router } from "express";
import alertEmailController from "../controller/alert-email-controller.js";

const router = Router();

/**
 * @openapi
 * /alertas/emails:
 *   get:
 *     summary: Lista los emails configurados para recibir alertas
 *     tags: [Alertas]
 *     responses:
 *       200:
 *         description: Listado de emails
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
 *                     $ref: '#/components/schemas/AlertEmail'
 *       500:
 *         description: Error al consultar los emails de alerta
 */
router.get("/alertas/emails", alertEmailController.getEmails);

/**
 * @openapi
 * /alertas/emails:
 *   post:
 *     summary: Agrega un email para recibir alertas
 *     tags: [Alertas]
 *     requestBody:
 *       required: true
 *       content:
 *         application/json:
 *           schema:
 *             type: object
 *             required: [email]
 *             properties:
 *               email: { type: string, format: email, example: alumno@fi.uba.ar }
 *     responses:
 *       201:
 *         description: Email agregado correctamente
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 status: { type: string, example: success }
 *                 message: { type: string }
 *                 data:
 *                   $ref: '#/components/schemas/AlertEmail'
 *       400:
 *         description: Email inválido o ya registrado
 */
router.post("/alertas/emails", alertEmailController.postEmail);

/**
 * @openapi
 * /alertas/emails/{id}:
 *   delete:
 *     summary: Elimina un email de alerta
 *     tags: [Alertas]
 *     parameters:
 *       - name: id
 *         in: path
 *         required: true
 *         description: ID (Mongo ObjectId) del email a eliminar
 *         schema: { type: string }
 *     responses:
 *       200:
 *         description: Email eliminado correctamente
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 status: { type: string, example: success }
 *                 message: { type: string }
 *                 data:
 *                   $ref: '#/components/schemas/AlertEmail'
 *       404:
 *         description: No se encontró el email indicado
 *       400:
 *         description: Error al eliminar el email de alerta
 */
router.delete("/alertas/emails/:id", alertEmailController.deleteEmail);

export default router;
