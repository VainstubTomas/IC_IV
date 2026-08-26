import 'dotenv/config';

export default {
    BDURL: process.env.BDURL,
    SERVERPORT: process.env.SERVERPORT,
    BROKERURL: process.env.BROKERURL,
    BROKERPORT: process.env.BROKERPORT,
    MQTTBROKERURL: process.env.MQTTBROKERURL,
    MQTTBROKERCAPATH: process.env.MQTTBROKERCAPATH,
    BROKERUSERNAME: process.env.BROKERUSERNAME,
    BROKERPASSW: process.env.BROKERPASSW
}