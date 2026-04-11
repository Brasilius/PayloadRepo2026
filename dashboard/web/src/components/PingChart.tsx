import React from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
} from 'recharts';
import { TelemetryData } from '../hooks/useSerialData';

interface PingChartProps {
  history: TelemetryData[];
}

const PingChart: React.FC<PingChartProps> = ({ history }) => {
  // Debug log to ensure history is reaching the component
  console.log('PingChart history length:', history.length);

  return (
    <div className="bg-slate-800 p-4 rounded-xl shadow-lg border border-slate-700 h-[300px]">
      <h3 className="text-slate-400 text-sm font-semibold mb-4 uppercase tracking-wider">
        Communication Latency (ms)
      </h3>
      <div style={{ width: '100%', height: '200px' }}>
        <LineChart width={300} height={200} data={history}>
          <CartesianGrid strokeDasharray="3 3" stroke="#334155" />
          <XAxis 
            dataKey="timestamp" 
            hide 
          />
          <YAxis 
            stroke="#94a3b8"
            fontSize={12}
            tickFormatter={(value) => `${value}ms`}
            domain={[0, 'auto']}
          />
          <Tooltip 
            contentStyle={{ backgroundColor: '#1e293b', border: 'none', borderRadius: '8px' }}
            itemStyle={{ color: '#38bdf8' }}
            labelClassName="hidden"
          />
          <Line 
            type="monotone" 
            dataKey="pingLatency" 
            stroke="#38bdf8" 
            strokeWidth={2} 
            dot={false}
            isAnimationActive={false}
          />
        </LineChart>
      </div>
    </div>
  );
};

export default PingChart;
