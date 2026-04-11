import { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';

export interface TelemetryData {
  pingLatency: number;
  motorRPM: number;
  stepCount: number; // NEMA motor steps
  soilConductivity: number; // 3-5 digit value with fractions
  timestamp: string;
}

export const useSerialData = (serverUrl: string = 'http://localhost:3001') => {
  const [data, setData] = useState<TelemetryData | null>(null);
  const [history, setHistory] = useState<TelemetryData[]>([]);
  const [isConnected, setIsConnected] = useState(false);

  useEffect(() => {
    const socket: Socket = io(serverUrl);

    socket.on('connect', () => {
      setIsConnected(true);
      console.log('Connected to dashboard server');
    });

    socket.on('serialData', (newData: TelemetryData) => {
      setData(newData);
      setHistory((prev) => [...prev.slice(-49), newData]); // Keep last 50 points
    });

    socket.on('disconnect', () => {
      setIsConnected(false);
    });

    return () => {
      socket.disconnect();
    };
  }, [serverUrl]);

  return { data, history, isConnected };
};
