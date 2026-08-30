import thresholdService from "./threshold-service.js";
import alertEmailService from "./alert-email-service.js";
import mailerConfig from "../config/mailer/mailer-config.js";

class AlertService {
  constructor() {
    // Estado en memoria por dispositivo: 'normal' | 'alta' | 'baja'
    // Permite alertar solo al cruzar el umbral, no en cada lectura.
    this.lastAlertState = new Map();
  }

  /**
   * Evalúa una lectura contra el umbral vigente y, si cruza de estado,
   * envía un mail a todos los emails de alerta registrados.
   * @param {{deviceId: string, temperature: number}} reading
   */
  async checkThresholdAndNotify({ deviceId, temperature }) {
    const thresholds = await thresholdService.getThresholds(deviceId);
    if (!thresholds.configured) return;

    let currentState = "normal";
    if (temperature > thresholds.max) currentState = "alta";
    else if (temperature < thresholds.min) currentState = "baja";

    const previousState = this.lastAlertState.get(deviceId) || "normal";
    this.lastAlertState.set(deviceId, currentState);

    // Solo notificar si hay una alerta nueva (cambio de estado, y no es "normal")
    if (currentState === "normal" || currentState === previousState) return;

    const recipients = await alertEmailService.listEmails();
    if (recipients.length === 0) {
      console.log("[alert-service] Umbral cruzado pero no hay emails de alerta configurados.");
      return;
    }

    const subject = currentState === "alta"
      ? `⚠️ Alerta: temperatura alta en ${deviceId}`
      : `⚠️ Alerta: temperatura baja en ${deviceId}`;

    const text = `El dispositivo "${deviceId}" registró ${temperature}°C, fuera del rango configurado ` +
      `(mínimo: ${thresholds.min}°C, máximo: ${thresholds.max}°C).`;

    console.log(`[alert-service] Umbral cruzado (${previousState} -> ${currentState}), notificando a ${recipients.length} email(s)`);

    for (const { email } of recipients) {
      await mailerConfig.sendMail({ to: email, subject, text });
    }
  }
}

export default new AlertService();
