import config from '../config/config.js';
import DeviceStatus from '../models/device-status-model.js';
import DeviceCommand from '../models/device-command-model.js';
import { parseAuraEvent, previousResponseStates } from '../config/mqtt/aura-protocol.js';
import { appliedMeshConfig, validateMeshConfig } from '../config/mesh-protocol.js';
import thresholdService from './threshold-service.js';
class DeviceEventService {
  async processMessage(topic, payload) {
    const kind = topic.endsWith('/status') ? 'status' : topic.endsWith('/response') ? 'response' : null;
    let event;
    try { event = parseAuraEvent(topic, payload, kind); }
    catch (err) { console.warn('[aura-events] Payload rechazado:', err.message); return null; }
    if (!event) return null;
    if (kind === 'status') {
      if (![config.AURA_DEVICE_ID, config.AURA_FREEZER_DEVICE_ID, config.AURA_BRIDGE_DEVICE_ID].includes(event.deviceId)) return null;
      if(config.ICIV_TRANSPORT==='mesh' && config.AURA_MESH_EXTENSIONS_ENABLED && event.details.iciv?.config) {
        let p;
        try {p=validateMeshConfig(event.details.iciv.config);}catch(_){return null;}
        if(event.deviceId!==(p.sensor==='heladera'?config.AURA_DEVICE_ID:config.AURA_FREEZER_DEVICE_ID))return null;
        await thresholdService.saveThresholds({deviceId:event.deviceId,min:p.min_c,max:p.max_c});
      }
      return await DeviceStatus.findOneAndUpdate({ deviceId: event.deviceId },
        { $set: { status: event.status, details: event.details } },
        { upsert: true, new: true, runValidators: true }).lean();
    }
    if (![config.AURA_DEVICE_ID,config.AURA_FREEZER_DEVICE_ID].includes(event.deviceId) || typeof event.details.command_id !== 'string') return null;
    const filter = { deviceId: event.deviceId, command_id: event.details.command_id };
    let applied = null;
    if (config.ICIV_TRANSPORT === 'mesh' && config.AURA_MESH_EXTENSIONS_ENABLED) {
      const command = await DeviceCommand.findOne(filter).lean();
      if (!command) return null;
      try { applied = appliedMeshConfig(event,command.params); }
      catch (err) { console.warn('[mesh-config]',err.message); return null; }
      if (applied) await thresholdService.saveThresholds({deviceId:event.deviceId,min:applied.min_c,max:applied.max_c});
    }
    // Conservar el historial incluso si la respuesta llega duplicada o fuera de orden.
    await DeviceCommand.updateOne(filter, { $addToSet: { responses: {
      status: event.status, details: event.details, confirma_ejecucion: !!applied
    } } });
    return await DeviceCommand.findOneAndUpdate({ ...filter, state: { $in: previousResponseStates(event.status) } },
      { $set: { state: event.status, responseDetails: event.details, ...(applied ? {confirma_ejecucion:true} : config.ICIV_TRANSPORT==='mesh' && config.AURA_MESH_EXTENSIONS_ENABLED ? {} : {confirma_ejecucion:false}) } },
      { new: true, runValidators: true }).lean();
  }
  async getStatus() {
    const device = config.AURA_DEVICE_ID ? await DeviceStatus.findOne({ deviceId: config.AURA_DEVICE_ID }).lean() : null;
    const bridge = config.AURA_BRIDGE_DEVICE_ID ? await DeviceStatus.findOne({ deviceId: config.AURA_BRIDGE_DEVICE_ID }).lean() : null;
    const freezer = config.AURA_FREEZER_DEVICE_ID ? await DeviceStatus.findOne({deviceId:config.AURA_FREEZER_DEVICE_ID}).lean() : null;
    return { device, bridge, bridgeConfigured: !!config.AURA_BRIDGE_DEVICE_ID,
      transport:config.ICIV_TRANSPORT, sensors:{heladera:device,freezer} };
  }
}
export default new DeviceEventService();
