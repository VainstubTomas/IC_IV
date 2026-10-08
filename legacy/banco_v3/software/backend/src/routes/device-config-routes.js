import {Router} from 'express';
import {randomUUID} from 'node:crypto';
import config from '../config/config.js';
import mqttConfig from '../config/mqtt/mqtt-config.js';
import {isDeviceId,validateConfig} from '../config/mqtt/aura-protocol.js';
import {validateMeshConfig,defaultMeshConfig} from '../config/mesh-protocol.js';
import DeviceCommand from '../models/device-command-model.js';
import DeviceStatus from '../models/device-status-model.js';
import deviceEventService from '../services/device-event-service.js';
import MeshAlert from '../models/mesh-alert-model.js';
import {latestSensorStates} from '../services/mesh-alert-service.js';
const router=Router();
router.get('/dispositivo/status',async(req,res)=>{try{res.json({...await deviceEventService.getStatus(),mqttConnected:mqttConfig.isConnected(),sensorAlerts:await latestSensorStates()});}catch(e){res.status(503).json({message:e.message});}});
router.get('/dispositivo/config',async(req,res)=>{
 try{
  const deviceId=config.AURA_DEVICE_ID;
  const latest=isDeviceId(deviceId)?await DeviceCommand.findOne({deviceId}).sort({createdAt:-1}).lean():null;
  const status=isDeviceId(deviceId)?await DeviceStatus.findOne({deviceId}).lean():null;
  let reported=null;try{if(status?.details.config)reported={params:validateMeshConfig(status.details.config),updatedAt:status.updatedAt};}catch(_){}
  // Aplicado trae el reporte completo del nodo. No mostrar un snapshot previo
  // como vigente mientras llega el siguiente status retained.
  if(latest?.confirma_ejecucion && latest.responseDetails?.config &&
    (!reported || Date.parse(latest.updatedAt)>Date.parse(reported.updatedAt))){
    try{reported={params:validateMeshConfig(latest.responseDetails.config),updatedAt:latest.updatedAt};}catch(_){}
  }
  res.json({deviceId:deviceId||null,transport:config.ICIV_TRANSPORT,experimental:config.AURA_CONFIG_EXPERIMENTAL,latest,reported,defaults:defaultMeshConfig(),readOnly:config.ICIV_TRANSPORT==='mesh',message:'Un UUID por placa. Solo aplicado confirma ejecucion.'});
 }catch(e){res.status(503).json({message:e.message});}
});
router.post('/dispositivo/config',async(req,res)=>{
 // En mesh el broker local es un espejo: el gateway no acepta comandos desde ahi.
 // La configuracion se cambia en AURA, que registra el cambio y emite el command_id.
 if(config.ICIV_TRANSPORT==='mesh')return res.status(409).json({message:'En mesh la configuracion se cambia desde AURA; el broker local es solo un espejo'});
 if(!config.AURA_CONFIG_EXPERIMENTAL)return res.status(409).json({message:'Habilitar comandos solo en banco configurado'});
 const deviceId=config.AURA_DEVICE_ID;if(!isDeviceId(deviceId))return res.status(400).json({message:'Configurar UUID de la placa'});
 let params;
 try{params=validateConfig(req.body);}catch(e){return res.status(400).json({message:e.message});}
 const command_id=randomUUID();let record;
 try{record=await DeviceCommand.create({deviceId,command_id,params,state:'publishing'});await mqttConfig.publishCommand(deviceId,{command:'set_config',params,command_id});await DeviceCommand.updateOne({command_id,state:'publishing'},{$set:{state:'pending'}});res.status(202).json({command_id,state:'pending',message:'Esperando aplicado del nodo'});}
 catch(e){if(record)await DeviceCommand.updateOne({command_id,state:'publishing'},{$set:{state:'publish_failed',error:e.message}}).catch(()=>{});res.status(503).json({command_id,message:e.message});}
});
router.get('/dispositivo/energia',async(req,res)=>{try{const events=await MeshAlert.find({deviceId:config.AURA_DEVICE_ID,type:'energia','details.alimentacion':'bateria'}).sort({orderAt:-1,createdAt:-1}).limit(20).lean();res.json({events:events.map(e=>({...e,powerCutAt:e.measuredAt}))});}catch(e){res.status(503).json({message:e.message});}});
export default router;
