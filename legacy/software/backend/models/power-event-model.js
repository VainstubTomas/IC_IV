import mongoose from 'mongoose';
const schema = new mongoose.Schema({
  deviceId: { type: String, required: true },
  ingest_id: { type: String, required: true, unique: true },
  measuredAt: Date, powerCutAt: Date,
  temperature: Number,
  onBattery: Boolean
}, { timestamps: true });
schema.index({ deviceId: 1, createdAt: -1 });
export default mongoose.model('PowerEvent', schema);
