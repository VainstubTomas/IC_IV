import mqtt from 'mqtt';
import fs from 'fs';
import config from '../config.js';
import { TOPICS, commandTopic } from './mqtt-topics.js';
let client = null;
function init(io, persistHandler) {
  if (client) return;
  const options = { clean: false, clientId: config.MQTT_CLIENT_ID, protocolVersion: 4, reconnectPeriod: 3000 };
  if (!options.clientId) throw new Error('Se requiere MQTT_CLIENT_ID estable');
  if (config.BROKERUSERNAME) options.username = config.BROKERUSERNAME;
  if (config.BROKERPASSW) options.password = config.BROKERPASSW;
  if (config.MQTTBROKERCAPATH && fs.existsSync(config.MQTTBROKERCAPATH)) options.ca = fs.readFileSync(config.MQTTBROKERCAPATH);
  client = mqtt.connect(config.MQTTBROKERURL, options);
  client.on('connect', () => {
    client.subscribe([TOPICS.DATA, TOPICS.STATUS, TOPICS.RESPONSE, 'alerts/+/+'], { qos: 1 }, (err, granted) => {
      if (err || granted?.some(item => item.qos !== 1)) console.error('[mqtt] No se obtuvo suscripcion QoS 1:', err?.message || granted);
      else console.log('[mqtt] Suscrito al broker configurado con QoS 1');
    });
  });
  client.on('error', err => {
    console.error('[mqtt]', err.message);
    io?.emit('system_fault', { source: 'MQTTBROKER', message: 'Fallo de conexion al broker configurado' });
  });
  // MQTT.js espera este callback antes de PUBACK. No confirmar lecturas sin guardar.
  client.handleMessage = (packet, callback) => {
    const process = async () => {
      try {
        const topic = packet.topic.toString(), payload = packet.payload.toString();
        const data = await persistHandler(topic, payload);
        io?.emit('mqtt_update', { topic, data });
        callback();
      } catch (err) {
        console.error('[mqtt] Persistencia fallida; se reintentara sin confirmar:', err.message);
        setTimeout(process, 3000);
      }
    };
    process();
  };
}
async function publishCommand(deviceId, message) {
  const topic = commandTopic(deviceId);
  if (!client?.connected) throw new Error('Broker AURA desconectado');
  if (message.command !== 'set_config') throw new Error('Comando no soportado');
  await new Promise((resolve, reject) => client.publish(topic, JSON.stringify(message),
    { qos: 1, retain: false }, err => err ? reject(err) : resolve()));
  return true;
}
function isConnected() { return !!client?.connected; }
export default { init, publishCommand, isConnected };
