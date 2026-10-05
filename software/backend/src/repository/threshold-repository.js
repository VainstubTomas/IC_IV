import Threshold from '../models/threshold-model.js';
import MeshThreshold from '../models/mesh-threshold-model.js';
import config from '../config/config.js';
class ThresholdRepository{
 async getByDevice(deviceId,sensor='heladera'){return (config.ICIV_TRANSPORT==='mesh'?MeshThreshold:Threshold).findOne({deviceId,...(config.ICIV_TRANSPORT==='mesh'?{sensor}:{})}).lean();}
 async upsert({deviceId,min,max,sensor='heladera'}){const filter={deviceId,...(config.ICIV_TRANSPORT==='mesh'?{sensor}:{})};return (config.ICIV_TRANSPORT==='mesh'?MeshThreshold:Threshold).findOneAndUpdate(filter,{...filter,min,max},{upsert:true,new:true,setDefaultsOnInsert:true}).lean();}
}export default new ThresholdRepository();
