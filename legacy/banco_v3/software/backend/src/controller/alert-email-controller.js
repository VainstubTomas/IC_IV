import alertEmailService from "../services/alert-email-service.js";

class AlertEmailController {
  /**
   * GET /api/v1/alertas/emails
   * Retorna los emails configurados para recibir alertas
   */
  async getEmails(req, res) {
    try {
      const emails = await alertEmailService.listEmails();
      return res.status(200).json({
        status: "success",
        count: emails.length,
        data: emails
      });
    } catch (error) {
      console.error("[controller] Error en getEmails:", error);
      return res.status(500).json({
        status: "error",
        message: "Error al consultar los emails de alerta"
      });
    }
  }

  /**
   * POST /api/v1/alertas/emails
   * Agrega un nuevo email de alerta
   */
  async postEmail(req, res) {
    try {
      const { email } = req.body || {};
      const result = await alertEmailService.addEmail(email);

      return res.status(201).json({
        status: "success",
        message: "Email agregado correctamente",
        data: result
      });
    } catch (error) {
      console.error("[controller] Error en postEmail:", error);
      return res.status(400).json({
        status: "error",
        message: error.message || "Error al agregar el email de alerta"
      });
    }
  }

  /**
   * DELETE /api/v1/alertas/emails/:id
   * Elimina un email de alerta
   */
  async deleteEmail(req, res) {
    try {
      const { id } = req.params;
      const result = await alertEmailService.removeEmail(id);

      return res.status(200).json({
        status: "success",
        message: "Email eliminado correctamente",
        data: result
      });
    } catch (error) {
      console.error("[controller] Error en deleteEmail:", error);
      const notFound = error.message?.includes("No se encontró");
      return res.status(notFound ? 404 : 400).json({
        status: "error",
        message: error.message || "Error al eliminar el email de alerta"
      });
    }
  }
}

export default new AlertEmailController();
