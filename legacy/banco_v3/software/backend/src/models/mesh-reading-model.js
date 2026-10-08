import mongoose from 'mongoose';
const schema=new mongoose.Schema({deviceId:{type:String,required:true},sensor:{type:String,enum:['heladera','freezer'],required:true},temperature:{type:Number,required:true},ingest_id:String,measuredAt:Date,orderAt:{type:Date,required:true},source:String},{timestamps:true});
schema.index({ingest_id:1,sensor:1},{unique:true,partialFilterExpression:{ingest_id:{$type:'string'}}});
schema.index({deviceId:1,sensor:1,orderAt:-1});
export default mongoose.model('MeshReading',schema);
