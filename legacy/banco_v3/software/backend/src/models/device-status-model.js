import mongoose from 'mongoose';
export default mongoose.model('DeviceStatus', new mongoose.Schema({
  deviceId: { type: String, required: true, unique: true },
  status: { type: String, enum: ['online', 'offline'], required: true },
  details: { type: mongoose.Schema.Types.Mixed, default: {} }
}, { timestamps: true }));
