import mqtt from "mqtt";
import fs from "fs";
import config from "../config.js";
import { TOPICS, commandTopic } from "./mqtt-topics.js";

// broker config
const MQTTBROKERURL = config.MQTTBROKERURL;

// mqtt client
let client = null;

/**
 * socket.io objeto from app.js to listen events in live
 * @param {object} io
 * @param {(topic: string, payload: string) => Promise<any>} [persistHandler] callback para persistir mensajes MQTT entrantes, inyectado por el composition root para evitar un import circular con la capa de servicio
 */

function init(io, persistHandler) {
    if(client) return;

    // Opciones de conexión MQTT
    const mqttOptions = {
        clean: true
    };

    if (config.BROKERUSERNAME) mqttOptions.username = config.BROKERUSERNAME;
    if (config.BROKERPASSW) mqttOptions.password = config.BROKERPASSW;

    // Solo cargar certificado CA si está definido y el archivo existe
    if (config.MQTTBROKERCAPATH && fs.existsSync(config.MQTTBROKERCAPATH)) {
        mqttOptions.ca = fs.readFileSync(config.MQTTBROKERCAPATH);
    }

    client = mqtt.connect(MQTTBROKERURL, mqttOptions);

    // drive connection
    client.on("connect", () => {
        console.log("[mqtt-config] cliente mqtt conectado al broker 🔌");
        
        // Consumir exclusivamente el broker de AURA; ChirpStack pertenece al bridge.
        const subTopics = [TOPICS.DATA, TOPICS.STATUS, TOPICS.RESPONSE];
        client.subscribe(subTopics, (err) => {
            if (err) {
                console.log('[mqtt-config] Error al suscribirse a tópicos:', err);
            } else {
                console.log(`[mqtt-config] Suscrito a los tópicos: ${subTopics.join(', ')}`);
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

    // messages reception (gateway mqtt -> socket.io + MongoDB persistence)
    client.on('message', async (topic, message) => {
        try {
            const payload = message.toString();
            
            // Log of receipted data
            console.log(`[mqtt-config] Tópico: ${topic}, Payload: ${payload}`);

            let savedData = null;
            if (topic.endsWith('/data') && typeof persistHandler === 'function') {
                try {
                    savedData = await persistHandler(topic, payload);
                } catch (serviceErr) {
                    console.error('[mqtt-config] Error al persistir mensaje MQTT en DB:', serviceErr.message);
                }
            }

            // io propagation - send to all web connected clients
            if (io) {
                io.emit('mqtt_update', { topic, payload, data: savedData });
            }

        } catch (e) {
            console.error('Error al procesar mensaje MQTT:', e);
        }
    });
}

/**
 * Publica un comando de control en el tópico MQTT.
 * @param {string} deviceId - UUID AURA
 * @param {object} message - command, params y command_id.
 */

async function publishCommand(deviceId, message) {
    const topic = commandTopic(deviceId);
    if (!client || !client.connected) throw new Error('Broker AURA desconectado');
    if (message.command !== 'set_config') throw new Error('Comando no soportado');
    await new Promise((resolve, reject) => {
        client.publish(topic, JSON.stringify(message), { qos: 1, retain: false }, err => err ? reject(err) : resolve());
    });
    // Confirma publicacion MQTT, nunca ejecucion en el nodo.
    return true;
}

export default {
    init,
    publishCommand
}