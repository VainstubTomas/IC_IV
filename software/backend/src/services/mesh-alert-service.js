import {createHash} from 'node:crypto';
import config from '../config/config.js';
import MeshAlert from '../models/mesh-alert-model.js';
import {parseMeshAlert} from '../config/mesh-protocol.js';
export async function saveMeshAlert(topic,payload){
 let e;try{e=parseMeshAlert(topic,payload);}catch(err){console.warn('[alerts]',err.message);return null;}if(!e||e.deviceId!==config.AURA_DEVICE_ID)return null;
 // Ordenar por medicion conocida o llegada, sin fabricar un ts del nodo.
 e.orderAt=e.measuredAt||new Date();
 if(e.measuredAt)e.dedupKey=createHash('sha256').update(JSON.stringify([e.deviceId,e.type,e.measuredAt.toISOString(),e.details])).digest('hex');
 try{return await MeshAlert.create(e);}catch(err){if(err.code!==11000||!e.dedupKey)throw err;return MeshAlert.findOne({dedupKey:e.dedupKey}).lean();}
}
export async function latestSensorStates(){
 const result={};for(const sensor of ['heladera','freezer'])result[sensor]=await MeshAlert.findOne({deviceId:config.AURA_DEVICE_ID,type:'sensor','details.campo':'temp_'+sensor+'_c'}).sort({orderAt:-1,createdAt:-1}).lean();return result;
}
