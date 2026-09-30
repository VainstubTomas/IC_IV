import config from '../config/config.js';
import DeviceStatus from '../models/device-status-model.js';
import DeviceCommand from '../models/device-command-model.js';
import { parseAuraEvent, previousResponseStates } from '../config/mqtt/aura-protocol.js';
class DeviceEventService {
  async processMessage(topic, payload) {
    const kind = topic.endsWith('/status') ? 'status' : topic.endsWith('/response') ? 'response' : null;
    let event;
    try { event = parseAuraEvent(topic, payload, kind); }
    catch (err) { console.warn('[aura-events] Payload rechazado:', err.message); return null; }
    if (!event) return null;
    if (kind === 'status') {
      if (![config.AURA_DEVICE_ID, config.AURA_BRIDGE_DEVICE_ID].includes(event.deviceId)) return null;
      return await DeviceStatus.findOneAndUpdate({ deviceId: event.deviceId },
        { $set: { status: event.status, details: event.details } },
        { upsert: true, new: true, runValidators: true }).lean();
    }
    if (event.deviceId !== config.AURA_DEVICE_ID || typeof event.details.command_id !== 'string') return null;
    const filter = { deviceId: event.deviceId, command_id: event.details.command_id };
    // Conservar el historial incluso si la respuesta llega duplicada o fuera de orden.
    await DeviceCommand.updateOne(filter, { $addToSet: { responses: {
      status: event.status, details: event.details, confirma_ejecucion: false
    } } });
    return await DeviceCommand.findOneAndUpdate({ ...filter, state: { $in: previousResponseStates(event.status) } },
      { $set: { state: event.status, responseDetails: event.details, confirma_ejecucion: false } },
      { new: true, runValidators: true }).lean();
  }
  async getStatus() {
    const device = config.AURA_DEVICE_ID ? await DeviceStatus.findOne({ deviceId: config.AURA_DEVICE_ID }).lean() : null;
    const bridge = config.AURA_BRIDGE_DEVICE_ID ? await DeviceStatus.findOne({ deviceId: config.AURA_BRIDGE_DEVICE_ID }).lean() : null;
    return { device, bridge, bridgeConfigured: !!config.AURA_BRIDGE_DEVICE_ID };
  }
}
export default new DeviceEventService();
