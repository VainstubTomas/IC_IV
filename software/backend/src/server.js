import { app } from './app';
import http from 'http';

async function main() {

    const server = http.createServer(app);
}

main();