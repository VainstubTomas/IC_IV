import alertEmailRepository from "../repository/alert-email-repository.js";

const EMAIL_REGEX = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

class AlertEmailService {
  /**
   * Lista todos los emails de alerta registrados
   */
  async listEmails() {
    const records = await alertEmailRepository.findAll();
    return records.map((r) => ({
      id: r._id,
      email: r.email,
      createdAt: r.createdAt
    }));
  }

  /**
   * Agrega un nuevo email de alerta
   * @param {string} email
   */
  async addEmail(email) {
    const trimmed = (email || "").trim().toLowerCase();

    if (!trimmed || !EMAIL_REGEX.test(trimmed)) {
      throw new Error("El email es obligatorio y debe tener un formato válido.");
    }

    try {
      const record = await alertEmailRepository.create(trimmed);
      return {
        id: record._id,
        email: record.email,
        createdAt: record.createdAt
      };
    } catch (error) {
      if (error.code === 11000) {
        throw new Error("Ese email ya está registrado para recibir alertas.");
      }
      throw error;
    }
  }

  /**
   * Elimina un email de alerta por id
   * @param {string} id
   */
  async removeEmail(id) {
    const deleted = await alertEmailRepository.deleteById(id);
    if (!deleted) {
      throw new Error("No se encontró el email a eliminar.");
    }
    return { id: deleted._id, email: deleted.email };
  }
}

export default new AlertEmailService();
