const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const simulator = require('./simulator');

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: {
    origin: '*',
    methods: ['GET', 'POST']
  }
});

const PORT = process.env.PORT || 3001;
const SERIAL_PORT_PATH = process.env.SERIAL_PORT || '/dev/ttyUSB0';
const BAUD_RATE = 9600;

// Root route for health check
app.get('/', (req, res) => {
  res.json({ status: 'ok', message: 'Dashboard Server is running' });
});

// Determine if we should run in simulation mode
const isSimulated = process.argv.includes('--simulate');

if (isSimulated) {
  console.log('Running in SIMULATION mode.');
  simulator.startSimulator(io);
} else {
  console.log(`Attempting to connect to serial port: ${SERIAL_PORT_PATH}`);
  try {
    const port = new SerialPort({
      path: SERIAL_PORT_PATH,
      baudRate: BAUD_RATE,
      autoOpen: true
    });

    const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

    port.on('open', () => {
      console.log(`Successfully connected to ${SERIAL_PORT_PATH}`);
    });

    parser.on('data', (data) => {
      try {
        const payload = data.trim();
        if (payload) {
          const parsedData = JSON.parse(payload);
          io.emit('serialData', parsedData);
        }
      } catch (err) {
        console.error('Error parsing serial data:', err.message, 'Raw data:', data);
      }
    });

    port.on('error', (err) => {
      console.error('Serial port error: ', err.message);
      console.log('Falling back to simulation mode...');
      simulator.startSimulator(io);
    });

  } catch (err) {
    console.error('Failed to initialize serial port:', err);
    console.log('Falling back to simulation mode...');
    simulator.startSimulator(io);
  }
}

io.on('connection', (socket) => {
  console.log('Client connected:', socket.id);
  socket.on('disconnect', () => {
    console.log('Client disconnected:', socket.id);
  });
});

server.listen(PORT, () => {
  console.log(`Dashboard server running on http://localhost:${PORT}`);
});
