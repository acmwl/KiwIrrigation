// ======================== Imports =============================
const express = require('express');
const mqtt = require('mqtt');
const http = require('http');
const { Server } = require('socket.io');
const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: { origin: "*" } 
});
const port = 3000;

// ======================== MQTT =============================

const protocol = 'mqtts'
const host = process.env.MQTT_BROKER_URL
const clientId = `mqtt_${Math.random().toString(16).slice(3)}`
const connectUrl = `${protocol}://${host}`
const client = mqtt.connect(connectUrl, {
  clientId,
  clean: true,
  connectTimeout: 4000,
  username: process.env.MQTT_USERNAME,
  password: process.env.MQTT_PASSWORD,
  reconnectPeriod: 1000,
})

client.on('connect', () => {
  console.log('Connected')
  const myTopic = 'esp32/test/status'; // The channel we want to listen to

  client.subscribe(myTopic, (err) => {
    if (!err) {
      console.log(`Now listening for messages on topic: "${myTopic}"`);
    } else {
      console.error('Failed to subscribe:', err);
    }
  });
})

client.on('message', (topic, message) => {
  const payload = message.toString();
  
  console.log(`[${topic}] says: ${payload}`);

  io.emit('sensorData', { topic: topic, message: payload });
});

client.on('error', (error) => {
  console.error('MQTT Connection Error:', error);
});

client.on('reconnect', () => {
  console.log('Retrying connection...');
});


// ======================== HTTP =============================

server.listen(3000, () => {
  console.log(`Backend & WebSockets running on port 3000`);
});

app.get('/', (req, res) => {
  res.send('The IoT Backend is online and listening!');
});
