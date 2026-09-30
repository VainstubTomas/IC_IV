import { Router } from 'express';
import { randomUUID } from 'node:crypto';
import config from '../config/config.js';
import mqttConfig from '../config/mqtt/mqtt-config.js';
import { isDeviceId, validateConfig } from '../config/mqtt/aura-protocol.js';
import DeviceCommand from '../models/device-command-model.js';
const router = Router();
router.get('/dispositivo/config', async (req, res) => {
  try {
    const deviceId = config.AURA_DEVICE_ID;
    const latest = isDeviceId(deviceId) ? await DeviceCommand.findOne({ deviceId }).sort({ createdAt: -1 }).lean() : null;
    res.json({ deviceId: deviceId || null, experimental: config.AURA_CONFIG_EXPERIMENTAL, latest,
      message: 'Aplicacion pendiente de confirmar: falta acordar reporte de ejecucion con AURA.' });
  } catch (err) { res.status(503).json({ message: err.message }); }
});
router.post('/dispositivo/config', async (req, res) => {
  if (!config.AURA_CONFIG_EXPERIMENTAL) return res.status(409).json({ message: 'Propuesta de configuracion pendiente de validar con la catedra. Habilitar solo en banco acordado.' });
  const deviceId = config.AURA_DEVICE_ID;
  if (!isDeviceId(deviceId)) return res.status(400).json({ message: 'Configurar AURA_DEVICE_ID con el UUID real' });
  let params;
  try { params = validateConfig(req.body); } catch (err) { return res.status(400).json({ message: err.message }); }
  const command_id = randomUUID();
  let record;
  try {
    record = await DeviceCommand.create({ deviceId, command_id, params, state: 'publishing' });
    await mqttConfig.publishCommand(deviceId, { command: 'set_config', params, command_id });
    record.state = 'pending'; await record.save();
    res.status(202).json({ command_id, state: 'pending', message: 'Publicado; espera el proximo uplink y confirmacion de aplicacion.' });
  } catch (err) {
    if (record) { record.state = 'publish_failed'; record.error = err.message; await record.save().catch(() => {}); }
    res.status(503).json({ command_id, message: err.message });
  }
});
export default router;
