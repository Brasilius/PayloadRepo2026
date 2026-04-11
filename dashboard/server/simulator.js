function startSimulator(io) {
  let steps = 0;
  let direction = 1;
  let rpm = 0;

  setInterval(() => {
    // Simulate NEMA steps (0 to 10000 steps)
    steps += (10 * direction);
    if (steps > 10000 || steps < 0) direction *= -1;
    
    // Simulate Jittery RPM
    rpm = 120 + (Math.random() - 0.5) * 5;

    // Simulate 3-5 digit Soil Conductivity with fractions
    // e.g., 123.45 to 12345.67
    const baseConductivity = 1500.00;
    const conductivity = baseConductivity + (Math.random() * 500);

    const data = {
      pingLatency: Math.floor(Math.random() * 30) + 15,
      motorRPM: parseFloat(rpm.toFixed(2)),
      stepCount: steps,
      soilConductivity: parseFloat(conductivity.toFixed(2)),
      timestamp: new Date().toISOString()
    };

    io.emit('serialData', data);
  }, 100);
}

module.exports = { startSimulator };
