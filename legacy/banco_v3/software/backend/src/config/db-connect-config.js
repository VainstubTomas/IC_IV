import { connect } from 'mongoose';
import config from './config.js';

export const bdInit = async () => {
    try {
        await connect(config.BDURL);
    } catch (error) {
        console.log("[db-connect-config] error: ", error);
        throw error;
    }
}