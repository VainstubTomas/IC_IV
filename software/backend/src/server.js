import { app } from './app.js';
import http from 'http';
import config from './config/config.js';
import { bdInit } from './config/db-connect-config.js';
import { Server } from 'socket.io';
import mqttConfig from './config/mqtt/mqtt-config.js';
import sensorDataService from './services/sensor-data-service.js';
import deviceEventService from './services/device-event-service.js';
import SensorData from './models/sensor-data-model.js';
import DeviceStatus from './models/device-status-model.js';
import DeviceCommand from './models/device-command-model.js';

async function mainServer() {

    const server = http.createServer(app);
    const io = new Server(server);

    //ws config
    io.on("connection", (socket) => {
        console.log(`[server] ws: nuevo cliente conectado ${socket.id} 🔌`);


    });

    // DB e indices de deduplicacion listos antes de recibir mensajes MQTT.
    await bdInit();
    await Promise.all([SensorData.init(), DeviceStatus.init(), DeviceCommand.init()]);
    mqttConfig.init(io, (topic, payload) => topic.endsWith('/data')
      ? sensorDataService.parseAndSaveMqttMessage(topic, payload)
      : deviceEventService.processMessage(topic, payload));

    server.listen(config.SERVERPORT, () => {
        console.log("[server] Servidor levantado 🚀");
    })
}

mainServer().catch(err => { console.error('[server] No se pudo iniciar:', err.message); process.exit(1); });