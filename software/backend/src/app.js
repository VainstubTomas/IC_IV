import express from 'express';
import cors from 'cors';
import path from 'path';
import { fileURLToPath } from 'url';
import apiRoutes from './routes/index.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export const app = express();

// Middlewares globales
app.use(cors({
    origin: '*',
    methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization', 'Accept']
}));

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Servir frontend estático directamente si se accede por navegador
const frontendPath = path.resolve(__dirname, '../../../frontend');
app.use(express.static(frontendPath));

// Rutas de API REST
app.use('/api', apiRoutes);
app.use('/', apiRoutes); // Alias directo para compatibilidad de rutas