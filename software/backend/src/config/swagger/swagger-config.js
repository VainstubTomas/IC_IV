// API general metadata

import swaggerJsdoc from "swagger-jsdoc";
import config from "../config.js";

const options = {
    definition: {
    openapi: "3.0.0",
    info: {
        title: "IC_IV Telemetry API",
        version: "1.0.0",
        description: "API REST para telemetría de temperatura de refrigeraciones industriales (IC IV)."
    },
    servers: [
        { url: `http://localhost:${config.SERVERPORT}/api/v1` }
    ],
    components: {
        schemas: {
            SensorData: {
                type: "object",
                properties: {
                id: { type: "string" },
                deviceId: { type: "string", example: "Heladera1" },
                temperature: { type: "number", example: -18.5 },
                rssi: { type: "number", nullable: true },
                source: { type: "string", enum: ["lora", "mqtt", "http", "manual"] },
                createdAt: { type: "string", format: "date-time" }
                }
            },
            Threshold: {
                type: "object",
                properties: {
                deviceId: { type: "string", example: "Heladera1" },
                min: { type: "number" },
                max: { type: "number" }
                }
            },
            AlertEmail: {
                type: "object",
                properties: {
                email: { type: "string", format: "email" }
                }
            }
        }
    }
    },
  apis: ["./src/routes/*.js"] // le decís dónde buscar los comentarios @openapi
};

export const swaggerSpec = swaggerJsdoc(options);