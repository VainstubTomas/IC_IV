import express from 'express';

export const app = () => {
    app = express();
    app.use(express.json());
    app.use(express.urlencoded({ extended: true }));
}