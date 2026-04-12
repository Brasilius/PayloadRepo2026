import React from 'react';
import type { TelemetryData } from '../hooks/useSerialData';

interface SoilConductivityChartProps {
  history: TelemetryData[];
}

const MAX_VALUE = 1500;

const SoilConductivityChart: React.FC<SoilConductivityChartProps> = ({ history }) => {
  if (history.length < 2) {
    return (
      <div className="bg-slate-800 p-4 rounded-xl border border-slate-700 shadow-xl h-[160px] flex flex-col">
        <h3 className="text-slate-400 text-xs font-semibold uppercase tracking-wider mb-2">
          Soil Conductivity — Live
        </h3>
        <div className="flex-1 flex items-center justify-center text-slate-500 text-xs italic">
          Awaiting telemetry...
        </div>
      </div>
    );
  }

  const svgWidth = 400;
  const svgHeight = 120;
  const padL = 36;
  const padR = 8;
  const padT = 8;
  const padB = 16;
  const innerW = svgWidth - padL - padR;
  const innerH = svgHeight - padT - padB;

  const toX = (i: number) =>
    padL + (i / (history.length - 1)) * innerW;
  const toY = (v: number) =>
    padT + innerH - Math.min(v / MAX_VALUE, 1) * innerH;

  const points = history
    .map((d, i) => `${toX(i)},${toY(d.soilConductivity)}`)
    .join(' ');

  // Y-axis tick labels
  const ticks = [0, 300, 600, 900, 1200, 1500];

  return (
    <div className="bg-slate-800 p-4 rounded-xl border border-slate-700 shadow-xl">
      <h3 className="text-slate-400 text-xs font-semibold uppercase tracking-wider mb-2">
        Soil Conductivity — Live
      </h3>
      <div className="w-full">
        <svg
          viewBox={`0 0 ${svgWidth} ${svgHeight}`}
          className="w-full"
          style={{ height: '130px' }}
          preserveAspectRatio="none"
        >
          {/* Grid lines + Y-axis ticks */}
          {ticks.map((tick) => {
            const y = toY(tick);
            return (
              <g key={tick}>
                <line
                  x1={padL}
                  y1={y}
                  x2={svgWidth - padR}
                  y2={y}
                  stroke="#1e3a4a"
                  strokeWidth="1"
                  strokeDasharray={tick === 0 ? 'none' : '3 3'}
                />
                <text
                  x={padL - 4}
                  y={y + 3}
                  textAnchor="end"
                  fontSize="9"
                  fill="#475569"
                >
                  {tick}
                </text>
              </g>
            );
          })}

          {/* Max reference line at 1500 */}
          <line
            x1={padL}
            y1={toY(1500)}
            x2={svgWidth - padR}
            y2={toY(1500)}
            stroke="#ef4444"
            strokeWidth="1"
            strokeDasharray="4 2"
          />

          {/* Area fill */}
          <polyline
            fill="url(#scGradient)"
            stroke="none"
            points={`${toX(0)},${padT + innerH} ${points} ${toX(history.length - 1)},${padT + innerH}`}
            style={{ opacity: 0.15 }}
          />

          {/* Data line */}
          <polyline
            fill="none"
            stroke="#ec4899"
            strokeWidth="2"
            strokeLinejoin="round"
            points={points}
          />

          <defs>
            <linearGradient id="scGradient" x1="0" x2="0" y1="0" y2="1">
              <stop offset="0%" stopColor="#ec4899" />
              <stop offset="100%" stopColor="transparent" />
            </linearGradient>
          </defs>
        </svg>
      </div>
      <div className="flex justify-between text-[10px] text-slate-500 px-1 mt-0.5">
        <span>0 µS/cm</span>
        <span>max 1500</span>
      </div>
    </div>
  );
};

export default SoilConductivityChart;
