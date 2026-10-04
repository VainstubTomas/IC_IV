import 'dotenv/config';

export default {
    AURA_DEVICE_ID: (process.env.AURA_FRIDGE_DEVICE_ID || process.env.AURA_DEVICE_ID)?.trim().toLowerCase(),
    AURA_FREEZER_DEVICE_ID: process.env.AURA_FREEZER_DEVICE_ID?.trim().toLowerCase(),
    AURA_BRIDGE_DEVICE_ID: (process.env.AURA_GATEWAY_DEVICE_ID || process.env.AURA_BRIDGE_DEVICE_ID)?.trim().toLowerCase(),
    ICIV_TRANSPORT: process.env.ICIV_TRANSPORT || 'mesh',
    AURA_MESH_EXTENSIONS_ENABLED: process.env.AURA_MESH_EXTENSIONS_ENABLED === 'true',
    MQTT_CLIENT_ID: process.env.MQTT_CLIENT_ID || `iciv-backend-${process.env.AURA_DEVICE_ID?.trim().toLowerCase() || "local"}`,
    AURA_CONFIG_EXPERIMENTAL: process.env.AURA_CONFIG_EXPERIMENTAL === "true",
    BDURL: process.env.BDURL,
    SERVERPORT: process.env.SERVERPORT,
    BROKERURL: process.env.BROKERURL,
    BROKERPORT: process.env.BROKERPORT,
    MQTTBROKERURL: process.env.MQTTBROKERURL,
    MQTTBROKERCAPATH: process.env.MQTTBROKERCAPATH,
    BROKERUSERNAME: process.env.BROKERUSERNAME,
    BROKERPASSW: process.env.BROKERPASSW,
    SMTPHOST: process.env.SMTPHOST,
    SMTPPORT: process.env.SMTPPORT,
    SMTPSECURE: process.env.SMTPSECURE,
    SMTPUSER: process.env.SMTPUSER,
    SMTPPASS: process.env.SMTPPASS,
    SMTPFROM: process.env.SMTPFROM
}
