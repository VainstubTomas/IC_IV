import mqtt from "mqtt";
import fs from "fs";
import config from "../config.js";
import { TOPICS } from "./mqtt-topics.js";

// broker config
const MQTTBROKERURL = config.MQTTBROKERURL;

// mqtt client
let client = null;

/**
 * socket.io objeto from app.js to listen events in live
 * @param {object} io
 */

function init(io) {
    // broker auth certificate
    const CA = fs.readFileSync(config.MQTTBROKERCAPATH);

    if(client) return;

    client = mqtt.connect(MQTTBROKERURL, {
        username: config.BROKERUSERNAME,
        password: config.BROKERPASSW,
        clean: true,
        ca: CA
    });

    // drive connection
    client.on("connect", () => {
        console.log("[mqtt-config] cliente mqtt conectado al broker 🔌");
        
        // topic base subscription
        client.subscribe(TOPICS.STATUSBASE, (err) => {
            if (err) {
                console.log('[mqtt-config] Error al suscribirse a tópicos:', err);
            } else {
                console.log(`[mqtt-config] Suscrito a la base de topicos ${TOPICS.STATUSBASE}`);
            }
        });
    });

    // estado mqtt
    client.on("error", (err) => {

        console.log('[mqtt-config] Error en el cliente MQTT:', err.message);
        console.log('[mqtt-config] Intentando emitir fallo al cliente');

        if (io) {
            io.emit('system_fault', { source: 'MQTTBROKER', message: 'Conexión perdida con el Broker' });
        }
    });

    // messages reception (gateway mqtt -> socket.io)
    client.on('message', (topic, message) => {
        try {
            const payload = message.toString();
            
            // Log of receipted data
            console.log(`[mqtt-config] Tópico: ${topic}, Payload: ${payload}`);

            // io propagation - send to all web connected clients
            if (io) {
                io.emit('mqtt_update', { topic, payload });
            }

        } catch (e) {
            console.error('Error al procesar mensaje MQTT:', e);
        }
    });
}

/**
 * Publica un comando de control en el tópico MQTT.
 * @param {string} type - 'analogico'
 * @param {string} payload - El valor del comando (ej: '150' o '1').
 */

function publishCommand(type, payload) {
    if (!client || !client.connected) {
        console.error('[mqtt-config] No se puede publicar porque el cliente MQTT no está conectado.');
        return false;
    }

    let topic;

    // select topic
    switch(type) {
        case "analog":
            topic = TOPICS.CMDANALOG;
            break;
        default:
            console.log(`[mqtt-config] comando desconocido ${type}`);
            return false;
    }

    // payload publish
    client.publish(topic, String(payload), { qos: 0, retain: false }, (err) => {
        if (err) {
            console.log(`[mqtt-config] Error al publicar en ${topic}:`, err);
        } else {
            console.log(`[mqtt-config] Comando publicado: ${topic} -> ${payload}`);
        }
    });
    return true;
}

export default {
    init,
    publishCommand
}