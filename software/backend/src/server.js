import { app } from './app.js';
import http from 'http';
import config from './config/config.js';
import { bdInit } from './config/db-connect-config.js';
import { Server } from 'socket.io';
import mqttConfig from './config/mqtt/mqtt-config.js';
import sensorDataService from './services/sensor-data-service.js';

async function mainServer() {

    const server = http.createServer(app);
    const io = new Server(server);

    //ws config
    io.on("connection", (socket) => {
        console.log(`[server] ws: nuevo cliente conectado ${socket.id} 🔌`);


    });

    mqttConfig.init(io, (topic, payload) => sensorDataService.parseAndSaveMqttMessage(topic, payload));

    bdInit()
        .then(() => console.log('[server] Conexión exitosa con la base de datos 🤝'))
        .catch((error) => console.log('[server] Error durante la conexión a la base de datos: ', error));

    server.listen(config.SERVERPORT, () => {
        console.log("[server] Servidor levantado 🚀");
    })
}

mainServer();