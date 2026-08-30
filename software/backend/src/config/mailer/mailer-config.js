import nodemailer from "nodemailer";
import config from "../config.js";

let transporter = null;

/**
 * Crea (una sola vez) el transporter SMTP a partir de las variables de entorno.
 * Es genérico: funciona con cualquier proveedor SMTP (Gmail, Outlook, institucional,
 * Mailgun/Brevo/SES, etc.), solo cambian los valores del .env.
 */
function getTransporter() {
    if (transporter) return transporter;

    transporter = nodemailer.createTransport({
        host: config.SMTPHOST,
        port: Number(config.SMTPPORT) || 587,
        secure: config.SMTPSECURE === "true",
        auth: config.SMTPUSER
            ? { user: config.SMTPUSER, pass: config.SMTPPASS }
            : undefined
    });

    return transporter;
}

/**
 * Envía un mail. No lanza excepción si falla o si no hay SMTP configurado:
 * loguea y devuelve false, para no romper el flujo principal (guardado de telemetría).
 * @param {{to: string, subject: string, text: string}} params
 */
async function sendMail({ to, subject, text }) {
    if (!config.SMTPHOST || !config.SMTPUSER) {
        console.log("[mailer-config] SMTP no configurado, se omite el envío de mail.");
        return false;
    }

    try {
        await getTransporter().sendMail({
            from: config.SMTPFROM || config.SMTPUSER,
            to,
            subject,
            text
        });
        console.log(`[mailer-config] Mail enviado a ${to} ✉️`);
        return true;
    } catch (error) {
        console.error("[mailer-config] Error al enviar mail:", error.message);
        return false;
    }
}

export default {
    sendMail
};
