import mongoose from "mongoose";

const alertEmailSchema = new mongoose.Schema(
  {
    email: {
      type: String,
      required: [true, "El email es requerido"],
      unique: true,
      trim: true,
      lowercase: true,
      match: [/^[^\s@]+@[^\s@]+\.[^\s@]+$/, "El formato del email es inválido"]
    }
  },
  { timestamps: true }
);

export default mongoose.model("AlertEmail", alertEmailSchema);
