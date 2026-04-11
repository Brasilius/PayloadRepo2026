import React, { useRef } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, PerspectiveCamera, Grid, Center, ContactShadows } from '@react-three/drei';
import * as THREE from 'three';

interface DrillModelProps {
  rpm: number;
  steps: number; // NEMA motor steps
}

const DrillModel: React.FC<DrillModelProps> = ({ rpm, steps }) => {
  const drillRef = useRef<THREE.Group>(null);

  useFrame((state, delta) => {
    if (drillRef.current) {
      // Rotate based on RPM
      const rotationSpeed = (rpm * (2 * Math.PI)) / 60;
      drillRef.current.rotation.y += rotationSpeed * delta;
      
      // Interpolate vertical position based on step count 
      // Assuming 10000 steps = 5 units depth
      const targetY = - (steps / 2000);
      drillRef.current.position.y = THREE.MathUtils.lerp(drillRef.current.position.y, targetY, 0.1);
    }
  });

  return (
    <group ref={drillRef}>
      <mesh position={[0, 0, 0]} castShadow>
        <cylinderGeometry args={[0.5, 0.05, 2, 8]} />
        <meshStandardMaterial color="#cbd5e1" metalness={0.9} roughness={0.1} />
      </mesh>
      <mesh position={[0, 1.5, 0]} castShadow>
        <cylinderGeometry args={[0.4, 0.4, 3, 32]} />
        <meshStandardMaterial color="#94a3b8" metalness={0.8} roughness={0.2} />
      </mesh>
      {[0, 1, 2, 3].map((i) => (
        <mesh key={i} position={[0, -0.5 + i * 0.4, 0]} rotation={[0.5, 0, 0]} castShadow>
          <torusGeometry args={[0.42, 0.04, 16, 32]} />
          <meshStandardMaterial color="#38bdf8" metalness={0.5} roughness={0.5} />
        </mesh>
      ))}
    </group>
  );
};

interface DrillGraphicProps {
  rpm: number;
  steps: number;
}

const DrillGraphic: React.FC<DrillGraphicProps> = ({ rpm, steps }) => {
  return (
    <div className="bg-slate-800 rounded-xl shadow-lg border border-slate-700 h-[500px] relative overflow-hidden">
      <div className="absolute top-4 left-4 z-10 pointer-events-none">
        <h3 className="text-slate-400 text-xs font-semibold uppercase tracking-wider mb-2">
          NEMA Stepper Array
        </h3>
        <div className="space-y-1">
          <p className="text-3xl font-mono text-sky-400 font-bold">
            {rpm.toFixed(1)} <span className="text-xs text-slate-500 uppercase">RPM</span>
          </p>
          <p className="text-3xl font-mono text-emerald-400 font-bold">
            {steps.toLocaleString()} <span className="text-xs text-slate-500 uppercase">Steps</span>
          </p>
        </div>
      </div>

      <Canvas shadows>
        <PerspectiveCamera makeDefault position={[5, 5, 10]} />
        <OrbitControls enableZoom={true} autoRotate={false} />
        <ambientLight intensity={1.5} />
        <pointLight position={[10, 10, 10]} intensity={2} castShadow />
        <directionalLight position={[-5, 5, 5]} intensity={1.5} color="#38bdf8" />
        <spotLight position={[0, 10, 0]} angle={0.3} penumbra={1} intensity={2} castShadow />

        <Grid
          infiniteGrid
          fadeDistance={50}
          fadeStrength={5}
          cellSize={1}
          cellThickness={1}
          cellColor="#475569"
          sectionSize={5}
          sectionThickness={2}
          sectionColor="#94a3b8"
        />

        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -2.01, 0]} receiveShadow>
          <planeGeometry args={[100, 100]} />
          <meshStandardMaterial color="#1e293b" transparent opacity={0.4} />
        </mesh>

        <Center top>
          <DrillModel rpm={rpm} steps={steps} />
        </Center>
        <ContactShadows position={[0, -2, 0]} opacity={0.4} scale={10} blur={2} far={4.5} />
      </Canvas>
    </div>
  );
};

export default DrillGraphic;
