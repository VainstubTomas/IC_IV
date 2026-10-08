import { randomUUID } from 'node:crypto';
import config from '../config/config.js';
import MeshReading from '../models/mesh-reading-model.js';
import { parseMeshData,validateSensor } from '../config/mesh-protocol.js';
import alertService from './alert-service.js';
const format=r=>r?{...r,temperatura:r.temperature}:null;
class MeshDataService {
 async persist(reading){
  let record,inserted=true;
  try{record=await MeshReading.create({...reading,orderAt:reading.measuredAt||new Date()});}
  catch(e){if(e.code!==11000||!reading.ingest_id)throw e;record=await MeshReading.findOne({ingest_id:reading.ingest_id,sensor:reading.sensor}).lean();if(!record)throw e;inserted=false;}
  if(inserted)alertService.checkThresholdAndNotify(reading).catch(e=>console.error('[alertas]',e.message));
  const raw=record.toObject?record.toObject():record;return {...format(raw),duplicate:!inserted};
 }
 async parseAndSaveMqttMessage(topic,payload){
  let readings;try{readings=parseMeshData(topic,payload);}catch(e){console.warn('[mesh]',e.message);return null;}
  const results=[];for(const reading of readings)if(reading.deviceId===config.AURA_DEVICE_ID)results.push(await this.persist(reading));return results;
 }
 async saveTelemetry({temperature,temperatura,sensor='heladera',deviceId=config.AURA_DEVICE_ID}){
  validateSensor(sensor);const t=temperature??temperatura;const reading=parseMeshData(`devices/${deviceId}/data`,JSON.stringify({values:{['temp_'+sensor+'_c']:t},ingest_id:randomUUID()}))[0];
  if(!reading||deviceId!==config.AURA_DEVICE_ID)throw new Error('UUID del nodo y temperatura validos requeridos');return this.persist({...reading,source:'http_local'});
 }
 async getLatestTelemetry(sensor='heladera'){validateSensor(sensor);return format(await MeshReading.findOne({deviceId:config.AURA_DEVICE_ID,sensor}).sort({orderAt:-1,createdAt:-1}).lean());}
 async getTelemetryHistory(limit=50,sensor='heladera'){validateSensor(sensor);return (await MeshReading.find({deviceId:config.AURA_DEVICE_ID,sensor}).sort({orderAt:-1,createdAt:-1}).limit(Math.min(500,Math.max(1,parseInt(limit)||50))).lean()).map(format);}
}
export default new MeshDataService();
