import mongoose from 'mongoose';
const schema=new mongoose.Schema({deviceId:{type:String,required:true},sensor:{type:String,enum:['heladera','freezer'],required:true},min:{type:Number,required:true},max:{type:Number,required:true}},{timestamps:true});
schema.index({deviceId:1,sensor:1},{unique:true});export default mongoose.model('MeshThreshold',schema);
