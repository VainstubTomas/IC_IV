import mongoose from 'mongoose';
const schema=new mongoose.Schema({deviceId:{type:String,required:true},type:{type:String,enum:['sensor','energia'],required:true},severity:String,message:String,details:mongoose.Schema.Types.Mixed,measuredAt:Date,orderAt:{type:Date,required:true},dedupKey:String},{timestamps:true});
schema.index({dedupKey:1},{unique:true,partialFilterExpression:{dedupKey:{$type:'string'}}});schema.index({deviceId:1,type:1,createdAt:-1});
export default mongoose.model('MeshAlert',schema);
