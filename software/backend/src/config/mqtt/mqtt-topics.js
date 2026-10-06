import { isDeviceId } from './aura-protocol.js';
export const TOPICS = Object.freeze({ DATA: 'devices/+/data', STATUS: 'devices/+/status', RESPONSE: 'devices/+/response' });
export function commandTopic(deviceId) {
  if (!isDeviceId(deviceId)) throw new Error('Se requiere el UUID del dispositivo en AURA');
  return `devices/${deviceId}/command`;
}
