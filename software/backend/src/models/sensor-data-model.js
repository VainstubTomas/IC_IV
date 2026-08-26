import mongoose from "mongoose";

const sensorDataModel = new mongoose.Schema(
  {
    temperature: { type: Number, required: true },
  },
  { timestamps: true }
);

export default mongoose.model("SensorData", sensorDataModel);