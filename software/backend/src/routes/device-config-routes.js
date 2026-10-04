import { Router } from 'express';
import { randomUUID } from 'node:crypto';
import config from '../config/config.js';
import mqttConfig from '../config/mqtt/mqtt-config.js';
import { isDeviceId, validateConfig } from '../config/mqtt/aura-protocol.js';
import DeviceCommand from '../models/device-command-model.js';
import deviceEventService from '../services/device-event-service.js';
import { sensorDeviceId, validateSensor, validateMeshConfig, defaultMeshConfig } from '../config/mesh-protocol.js';
import PowerEvent from '../models/power-event-model.js';
import DeviceStatus from '../models/device-status-model.js';
const router = Router();
router.get('/dispositivo/status', async (req, res) => {
  try { res.json({ ...(await deviceEventService.getStatus()), mqttConnected: mqttConfig.isConnected() }); }
  catch (err) { res.status(503).json({ message: err.message }); }
});
router.get('/dispositivo/config', async (req, res) => {
  try {
    const sensor=validateSensor(req.query.sensor || 'heladera');
    const deviceId = sensorDeviceId(config,sensor);
    const latest = isDeviceId(deviceId) ? await DeviceCommand.findOne({ deviceId }).sort({ createdAt: -1 }).lean() : null;
    const status = isDeviceId(deviceId) ? await DeviceStatus.findOne({deviceId}).lean() : null;
    let reported=null;
    if(config.AURA_MESH_EXTENSIONS_ENABLED && status?.details?.iciv?.config) {
      try {const candidate=validateMeshConfig(status.details.iciv.config);if(candidate.sensor===sensor)reported={params:candidate,updatedAt:status.updatedAt};}catch(_){}
    }
    res.json({ deviceId: deviceId || null, sensor, transport:config.ICIV_TRANSPORT,
      experimental:config.AURA_CONFIG_EXPERIMENTAL && (config.ICIV_TRANSPORT!=='mesh' || config.AURA_MESH_EXTENSIONS_ENABLED), latest,
      reported,defaults:config.ICIV_TRANSPORT==='mesh'?defaultMeshConfig(sensor):null,
      message: 'Confirmacion de aplicacion requiere reporte explicito del nodo y adaptacion del gateway.' });
  } catch (err) { res.status(503).json({ message: err.message }); }
});
router.post('/dispositivo/config', async (req, res) => {
  if (!config.AURA_CONFIG_EXPERIMENTAL) return res.status(409).json({ message: 'Propuesta de configuracion pendiente de validar con la catedra. Habilitar solo en banco acordado.' });
  let sensor,deviceId;
  try {sensor=validateSensor(req.body?.sensor || 'heladera');deviceId=sensorDeviceId(config,sensor);}
  catch(err){return res.status(400).json({message:err.message});}
  if (!isDeviceId(deviceId)) return res.status(400).json({ message: 'Configurar AURA_DEVICE_ID con el UUID real' });
  if(config.ICIV_TRANSPORT==='mesh' && !config.AURA_MESH_EXTENSIONS_ENABLED)return res.status(409).json({message:'Adaptacion mesh y reporte de aplicacion no habilitados'});
  if(sensor==='freezer' && deviceId===config.AURA_DEVICE_ID)return res.status(400).json({message:'Cada sonda necesita su UUID AURA independiente'});
  let params;
  try { params = config.ICIV_TRANSPORT==='mesh'?validateMeshConfig(req.body):validateConfig(req.body); } catch (err) { return res.status(400).json({ message: err.message }); }
  const command_id = randomUUID();
  let record;
  try {
    record = await DeviceCommand.create({ deviceId, command_id, params, state: 'publishing' });
    await mqttConfig.publishCommand(deviceId, { command: 'set_config', params, command_id });
    await DeviceCommand.updateOne({ command_id, state: 'publishing' }, { $set: { state: 'pending' } });
    res.status(202).json({ command_id, state: 'pending', message: 'Publicado; pendiente de reporte de aplicacion del nodo.' });
  } catch (err) {
    if (record) await DeviceCommand.updateOne({ command_id, state: 'publishing' }, { $set: { state: 'publish_failed', error: err.message } }).catch(() => {});
    res.status(503).json({ command_id, message: err.message });
  }
});
router.get('/dispositivo/energia',async(req,res)=>{
  try {
    const ids=[config.AURA_DEVICE_ID,config.AURA_FREEZER_DEVICE_ID].filter(isDeviceId);
    const events=await PowerEvent.find({deviceId:{$in:ids}}).sort({powerCutAt:-1,createdAt:-1}).limit(20).lean();
    res.json({extensionsEnabled:config.AURA_MESH_EXTENSIONS_ENABLED,events});
  }catch(err){res.status(503).json({message:err.message});}
});
export default router;
