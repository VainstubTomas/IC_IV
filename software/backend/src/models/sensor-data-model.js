import mongoose from "mongoose";

const sensorDataSchema = new mongoose.Schema(
  {
    deviceId: { 
      type: String, 
      default: "Heladera1",
      trim: true 
    },
    temperature: { 
      type: Number, 
      required: [true, "La temperatura es requerida"] 
    },
    rssi: { 
      type: Number,
      default: null 
    },
    source: {
      type: String,
      enum: ["lora", "mqtt", "http", "manual"],
      default: "mqtt"
    }
  },
  { 
    timestamps: true,
    toJSON: {
      virtuals: true,
      transform: (doc, ret) => {
        // Formatear para compatibilidad tanto en inglés como en español
        ret.temperatura = ret.temperature;
        ret.id = ret._id;
        delete ret.__v;
        return ret;
      }
    }
  }
);

// Índice compuesto para acelerar consultas del último registro
sensorDataSchema.index({ createdAt: -1 });

export default mongoose.model("SensorData", sensorDataSchema);