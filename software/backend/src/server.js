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
import PowerEvent from './models/power-event-model.js';
import { savePowerEvent } from './services/power-event-service.js';

async function mainServer() {

    const server = http.createServer(app);
    const io = new Server(server);

    //ws config
    io.on("connection", (socket) => {
        console.log(`[server] ws: nuevo cliente conectado ${socket.id} 🔌`);


    });

    // DB e indices de deduplicacion listos antes de recibir mensajes MQTT.
    await bdInit();
    await Promise.all([SensorData.init(), DeviceStatus.init(), DeviceCommand.init(), PowerEvent.init()]);
    // Las lecturas anteriores no tenian hora de medicion separada de recepcion.
    // Completar la clave de orden antes de recibir historiales acumulados del nodo.
    await SensorData.updateMany(
      { orderAt: { $exists: false } },
      [{ $set: { orderAt: { $ifNull: ['$measuredAt', '$createdAt'] } } }]
    );
    mqttConfig.init(io, async (topic, payload) => {
      if (!topic.endsWith('/data')) return deviceEventService.processMessage(topic,payload);
      await savePowerEvent(topic,payload);
      return sensorDataService.parseAndSaveMqttMessage(topic,payload);
    });

    server.listen(config.SERVERPORT, () => {
        console.log("[server] Servidor levantado 🚀");
    })
}

mainServer().catch(err => { console.error('[server] No se pudo iniciar:', err.message); process.exit(1); });
