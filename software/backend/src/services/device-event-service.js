import config from '../config/config.js';
import DeviceStatus from '../models/device-status-model.js';
import DeviceCommand from '../models/device-command-model.js';
import {parseAuraEvent,previousResponseStates} from '../config/mqtt/aura-protocol.js';
import {appliedMeshConfig,validateMeshConfig} from '../config/mesh-protocol.js';
import thresholdService from './threshold-service.js';
class DeviceEventService {
 async updateThresholds(p){for(const sensor of ['heladera','freezer'])await thresholdService.saveThresholds({deviceId:config.AURA_DEVICE_ID,sensor,min:p['min_'+sensor+'_c'],max:p['max_'+sensor+'_c']});}
 async processMessage(topic,payload){
  const kind=topic.endsWith('/status')?'status':topic.endsWith('/response')?'response':null;let e;
  try{e=parseAuraEvent(topic,payload,kind);}catch(err){console.warn('[aura-events]',err.message);return null;}
  if(!e||![config.AURA_DEVICE_ID,config.AURA_BRIDGE_DEVICE_ID].includes(e.deviceId))return null;
  if(kind==='status'){
   if(config.ICIV_TRANSPORT==='mesh'&&e.deviceId===config.AURA_DEVICE_ID&&e.details.config){let p;try{p=validateMeshConfig(e.details.config);}catch(_){return null;}await this.updateThresholds(p);}
   return DeviceStatus.findOneAndUpdate({deviceId:e.deviceId},{$set:{status:e.status,details:e.details}},{upsert:true,new:true,runValidators:true}).lean();
  }
  if(e.deviceId!==config.AURA_DEVICE_ID||typeof e.details.command_id!=='string')return null;
  const filter={deviceId:e.deviceId,command_id:e.details.command_id};let applied=null;
  const command=await DeviceCommand.findOne(filter).lean();if(!command)return null;
  if(config.ICIV_TRANSPORT==='mesh'){try{applied=appliedMeshConfig(e,command.params);}catch(err){console.warn('[mesh-config]',err.message);return null;}if(applied)await this.updateThresholds(applied);}
  await DeviceCommand.updateOne(filter,{$addToSet:{responses:{status:e.status,details:e.details,confirma_ejecucion:!!applied}}});
  return DeviceCommand.findOneAndUpdate({...filter,state:{$in:previousResponseStates(e.status)}},{$set:{state:e.status,responseDetails:e.details,...(applied?{confirma_ejecucion:true}:{})}},{new:true,runValidators:true}).lean();
 }
 async getStatus(){
  const device=config.AURA_DEVICE_ID?await DeviceStatus.findOne({deviceId:config.AURA_DEVICE_ID}).lean():null;
  const bridge=config.AURA_BRIDGE_DEVICE_ID?await DeviceStatus.findOne({deviceId:config.AURA_BRIDGE_DEVICE_ID}).lean():null;
  return {device,bridge,transport:config.ICIV_TRANSPORT,bridgeConfigured:!!config.AURA_BRIDGE_DEVICE_ID,sensors:{heladera:device,freezer:device}};
 }
}
export default new DeviceEventService();
