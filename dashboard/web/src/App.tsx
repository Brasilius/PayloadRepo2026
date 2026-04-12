import React from 'react';
import { Activity, Radio, Cpu, Settings, AlertCircle, Droplets, Terminal } from 'lucide-react';
import { useSerialData } from './hooks/useSerialData';
import DrillGraphic from './components/DrillGraphic';
import SoilConductivityChart from './components/SoilConductivityChart';

function App() {
  const { data, history, isConnected } = useSerialData();

  const renderPingChart = () => {
    if (!history || history.length < 2) return (
      <div className="h-full flex items-center justify-center text-slate-500 text-sm italic">
        Awaiting telemetry...
      </div>
    );

    const width = 300;
    const height = 100;
    const padding = 10;
    const maxPing = Math.max(...history.map(d => d.pingLatency), 100);
    const points = history.map((d, i) => {
      const x = padding + (i / (history.length - 1)) * (width - 2 * padding);
      const y = height - padding - (d.pingLatency / maxPing) * (height - 2 * padding);
      return `${x},${y}`;
    }).join(' ');

    return (
      <svg viewBox={`0 0 ${width} ${height}`} className="w-full h-full">
        <polyline fill="none" stroke="#ec4899" strokeWidth="2" points={points} />
      </svg>
    );
  };

  return (
    <div className="min-h-screen bg-slate-900 text-slate-100 flex overflow-hidden">
      {/* Sidebar */}
      <aside className="w-64 bg-slate-800 border-r border-slate-700 flex flex-col">
        <div className="p-6">
          <h1 className="text-xl font-bold flex items-center gap-2">
            <Radio className="text-pink-500" /> PayloadRepo
          </h1>
          <p className="text-xs text-slate-500 mt-1 uppercase tracking-widest">Ground Station v2.6</p>
        </div>

        <nav className="flex-1 px-4 space-y-2">
          <div className="w-full flex items-center gap-3 px-4 py-2 bg-pink-500/10 text-pink-400 rounded-lg border border-pink-500/20">
            <Activity size={18} /> Dashboard
          </div>
          <button className="w-full flex items-center gap-3 px-4 py-2 text-slate-400 hover:bg-slate-700 rounded-lg transition-colors">
            <Cpu size={18} /> Motor Config
          </button>
        </nav>

        <div className="p-4 border-t border-slate-700">
          <div className="flex items-center gap-3 px-4 py-2">
            <div className={`w-2 h-2 rounded-full ${isConnected ? 'bg-emerald-500' : 'bg-rose-500'}`} />
            <span className="text-xs uppercase font-semibold text-slate-400 text-center">
              {isConnected ? 'System Online' : 'System Offline'}
            </span>
          </div>
        </div>
      </aside>

      {/* Main Content */}
      <main className="flex-1 overflow-y-auto p-8 space-y-8">
        <header className="flex justify-between items-end">
          <div>
            <h2 className="text-3xl font-bold">Ground Station Dashboard</h2>
            <p className="text-slate-400 italic">Real-time Soil Analysis & Payload Positioning</p>
          </div>
        </header>

        {!isConnected && (
          <div className="bg-rose-500/10 border border-rose-500/20 text-rose-400 px-6 py-4 rounded-xl flex items-center gap-4 animate-pulse">
            <AlertCircle />
            <span>Waiting for connection to backend... (Ensure server is running with --simulate)</span>
          </div>
        )}

        <div className="grid grid-cols-1 lg:grid-cols-4 gap-8">
          {/* Left Column: Metrics & Charts */}
          <div className="lg:col-span-1 space-y-8">
            
            {/* Soil Conductivity Card */}
            <div className="bg-slate-800 p-6 rounded-xl border border-slate-700 shadow-xl overflow-hidden relative">
              <div className="absolute top-0 right-0 p-2 text-pink-500/20">
                <Droplets size={48} />
              </div>
              <h3 className="text-slate-400 text-xs font-semibold uppercase tracking-wider mb-2">Soil Conductivity</h3>
              <div className="flex items-baseline gap-2">
                <span className="text-4xl font-mono text-white font-bold tracking-tight">
                  {data?.soilConductivity?.toFixed(2) ?? '0.00'}
                </span>
                <span className="text-xs text-slate-500 font-semibold uppercase">µS/cm</span>
              </div>
            </div>

            {/* Soil Conductivity Chart */}
            <SoilConductivityChart history={history} />

            {/* Signal Delay Chart */}
            <div className="bg-slate-800 p-6 rounded-xl border border-slate-700 h-[180px] flex flex-col shadow-lg">
               <h3 className="text-slate-400 text-xs font-semibold uppercase tracking-wider mb-4">Signal Delay</h3>
               <div className="flex-1 min-h-0">
                 {renderPingChart()}
               </div>
               <div className="mt-2 text-right">
                 <span className="text-pink-400 font-mono text-sm font-bold">{data?.pingLatency ?? 0}ms</span>
               </div>
            </div>

            {/* Ideal Parse Format Card */}
            <div className="bg-slate-900/50 p-4 rounded-xl border border-slate-800 border-dashed">
              <div className="flex items-center gap-2 text-slate-400 text-xs font-semibold uppercase mb-3">
                <Terminal size={14} /> Hardware JSON Format
              </div>
              <pre className="text-[10px] font-mono text-pink-400/80 leading-relaxed bg-black/30 p-2 rounded">
{`{
  "pingLatency": 25,
  "motorRPM": 120.5,
  "stepCount": 4500,
  "soilConductivity": 1850.25,
  "timestamp": "2026-04-11T..."
}`}
              </pre>
            </div>
          </div>

          {/* Right Column: 3D Visualization */}
          <div className="lg:col-span-3">
            <DrillGraphic 
              rpm={data?.motorRPM ?? 0} 
              steps={data?.stepCount ?? 0} 
            />
          </div>
        </div>

        {/* Status Indicators Footer */}
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
          <div className="bg-slate-800/50 p-3 rounded-lg border border-slate-700 flex justify-between items-center">
            <span className="text-xs text-slate-500 uppercase font-semibold">LoRa Status</span>
            <span className="text-xs text-pink-400 font-bold px-2 py-0.5 bg-pink-400/10 rounded">LOCKED</span>
          </div>
          <div className="bg-slate-800/50 p-3 rounded-lg border border-slate-700 flex justify-between items-center">
            <span className="text-xs text-slate-500 uppercase font-semibold">NEMA Stepper</span>
            <span className="text-xs text-pink-400 font-bold px-2 py-0.5 bg-pink-400/10 rounded">ONLINE</span>
          </div>
          <div className="bg-slate-800/50 p-3 rounded-lg border border-slate-700 flex justify-between items-center">
            <span className="text-xs text-slate-500 uppercase font-semibold">Soil Sensor</span>
            <span className="text-xs text-pink-400 font-bold px-2 py-0.5 bg-pink-400/10 rounded">SENSING</span>
          </div>
          <div className="bg-slate-800/50 p-3 rounded-lg border border-slate-700 flex justify-between items-center">
            <span className="text-xs text-slate-500 uppercase font-semibold">Buffer State</span>
            <span className="text-xs text-amber-400 font-bold px-2 py-0.5 bg-amber-400/10 rounded">SYNCED</span>
          </div>
        </div>
      </main>
    </div>
  );
}

export default App;
