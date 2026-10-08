import mongoose from "mongoose";

const thresholdSchema = new mongoose.Schema(
  {
    deviceId: {
      type: String,
      required: true,
      trim: true,
      unique: true
    },
    min: {
      type: Number,
      required: [true, "El umbral mínimo es requerido"]
    },
    max: {
      type: Number,
      required: [true, "El umbral máximo es requerido"]
    }
  },
  { timestamps: true }
);

export default mongoose.model("Threshold", thresholdSchema);
