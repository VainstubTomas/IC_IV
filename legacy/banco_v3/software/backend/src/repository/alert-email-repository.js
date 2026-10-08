import AlertEmail from "../models/alert-email-model.js";

/**
 * Repositorio de acceso a datos para los emails de alerta
 */
class AlertEmailRepository {
  /**
   * Crea un nuevo email de alerta
   * @param {string} email
   */
  async create(email) {
    const record = new AlertEmail({ email });
    return await record.save();
  }

  /**
   * Obtiene todos los emails de alerta registrados
   */
  async findAll() {
    return await AlertEmail.find().sort({ createdAt: 1 }).lean();
  }

  /**
   * Elimina un email de alerta por su id
   * @param {string} id
   */
  async deleteById(id) {
    return await AlertEmail.findByIdAndDelete(id).lean();
  }
}

export default new AlertEmailRepository();
