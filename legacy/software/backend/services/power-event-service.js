import config from '../config/config.js';
import { parseMeshMetadata } from '../config/mesh-protocol.js';
import PowerEvent from '../models/power-event-model.js';
export async function savePowerEvent(topic, payload) {
  if (!config.AURA_MESH_EXTENSIONS_ENABLED) return null;
  let m;
  try { m = parseMeshMetadata(topic,payload); } catch (_) { return null; }
  if (!m || !m.powerFirst || !m.ingest_id || ![config.AURA_DEVICE_ID,config.AURA_FREEZER_DEVICE_ID].includes(m.deviceId)) return null;
  const p=JSON.parse(payload), temperature=p.values?.temp_c;
  const data={deviceId:m.deviceId,ingest_id:m.ingest_id,measuredAt:m.measuredAt,powerCutAt:m.powerCutAt,onBattery:m.onBattery};
  if (typeof temperature==='number' && Number.isFinite(temperature) && temperature>=-55 && temperature<=125) data.temperature=temperature;
  try { return await PowerEvent.findOneAndUpdate({ingest_id:m.ingest_id},{$setOnInsert:data},{upsert:true,new:true,runValidators:true}).lean(); }
  catch (err) { if(err.code===11000)return await PowerEvent.findOne({ingest_id:m.ingest_id}).lean();throw err; }
}
