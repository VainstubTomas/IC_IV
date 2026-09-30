import mongoose from 'mongoose';
export default mongoose.model('DeviceCommand', new mongoose.Schema({
  deviceId: { type: String, required: true }, command_id: { type: String, required: true, unique: true },
  params: { type: mongoose.Schema.Types.Mixed, required: true },
  state: { type: String, enum: ['publishing', 'pending', 'publish_failed'], required: true },
  error: String
}, { timestamps: true }));
