import React from 'react';
import { TelemetryData } from '../hooks/useSerialData';

interface SimplePingChartProps {
  history: TelemetryData[];
}

const SimplePingChart: React.FC<SimplePingChartProps> = ({ history }) => {
  const width = 400;
  const height = 150;
  const padding = 20;

  if (history.length < 2) {
    return (
      <div className="bg-slate-800 p-4 rounded-xl shadow-lg border border-slate-700 h-[220px] flex items-center justify-center">
        <p className="text-slate-500 text-sm animate-pulse">Gathering telemetry...</p>
      </div>
    );
  }

  // Calculate points for the SVG line
  const maxPing = Math.max(...history.map(d => d.pingLatency), 100);
  const minPing = 0;
  
  const points = history.map((d, i) => {
    const x = padding + (i / (history.length - 1)) * (width - 2 * padding);
    const y = height - padding - ((d.pingLatency - minPing) / (maxPing - minPing)) * (height - 2 * padding);
    return `${x},${y}`;
  }).join(' ');

  const latestPing = history[history.length - 1].pingLatency;

  return (
    <div className="bg-slate-800 p-6 rounded-xl shadow-lg border border-slate-700 h-[220px]">
      <div className="flex justify-between items-start mb-4">
        <h3 className="text-slate-400 text-xs font-semibold uppercase tracking-wider">
          Signal Latency
        </h3>
        <span className="text-pink-400 font-mono text-lg font-bold">
          {latestPing}ms
        </span>
      </div>
      
      <div className="relative w-full h-[120px]">
        <svg 
          viewBox={`0 0 ${width} ${height}`} 
          className="w-full h-full overflow-visible"
          preserveAspectRatio="none"
        >
          {/* Grid lines */}
          <line x1={padding} y1={height-padding} x2={width-padding} y2={height-padding} stroke="#334155" strokeWidth="1" />
          <line x1={padding} y1={padding} x2={width-padding} y2={padding} stroke="#334155" strokeWidth="1" strokeDasharray="4" />
          
          {/* The Line */}
          <polyline
            fill="none"
            stroke="#ec4899"
            strokeWidth="3"
            strokeLinejoin="round"
            points={points}
            className="transition-all duration-300 ease-in-out"
          />
          
          {/* Area fill */}
          <polyline
            fill="url(#gradient)"
            stroke="none"
            points={`${padding},${height-padding} ${points} ${width-padding},${height-padding}`}
            style={{ opacity: 0.2 }}
          />

          <defs>
            <linearGradient id="gradient" x1="0" x2="0" y1="0" y2="1">
              <stop offset="0%" stopColor="#ec4899" />
              <stop offset="100%" stopColor="transparent" />
            </linearGradient>
          </defs>
        </svg>
      </div>
    </div>
  );
};

export default SimplePingChart;
