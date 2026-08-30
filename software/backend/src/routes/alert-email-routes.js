import { Router } from "express";
import alertEmailController from "../controller/alert-email-controller.js";

const router = Router();

// Endpoints de emails de alerta
router.get("/alertas/emails", alertEmailController.getEmails);
router.post("/alertas/emails", alertEmailController.postEmail);
router.delete("/alertas/emails/:id", alertEmailController.deleteEmail);

export default router;
