import mongoose from 'mongoose';
export default mongoose.model('DeviceCommand', new mongoose.Schema({
  deviceId: { type: String, required: true }, command_id: { type: String, required: true, unique: true },
  params: { type: mongoose.Schema.Types.Mixed, required: true },
  state: { type: String, enum: ['publishing', 'pending', 'publish_failed', 'encolado', 'transmitido', 'recibido', 'aplicado', 'rechazado'], required: true },
  confirma_ejecucion: { type: Boolean, default: false },
  responseDetails: mongoose.Schema.Types.Mixed,
  responses: { type: [mongoose.Schema.Types.Mixed], default: [] },
  error: String
}, { timestamps: true }));
