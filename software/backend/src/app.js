import express from 'express';
import cors from 'cors';
import path from 'path';
import apiRoutes from './routes/index.js';
import swaggerUi from 'swagger-ui-express';
import { swaggerSpec } from './config/swagger/swagger-config.js';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export const app = express();

// Middlewares
app.use(cors({
    origin: '*',
    methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization', 'Accept']
}));

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Servir frontend estático directamente si se accede por navegador
const frontendPath = path.resolve(__dirname, '../../frontend');
app.use(express.static(frontendPath));

app.use('/api', apiRoutes);

app.use('/api-docs', swaggerUi.serve, swaggerUi.setup(swaggerSpec));