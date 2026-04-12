# Use Fedora 42 as requested
FROM fedora:42

# Set working directory
WORKDIR /app

# Install system dependencies
# - gcc-c++ for compiling C++ modules
# - python3, python3-pip for the orchestrator
# - nodejs, npm for the dashboard
# - curl for installing 'uv'
RUN dnf install -y \
    gcc-c++ \
    python3 \
    python3-pip \
    nodejs \
    npm \
    curl \
    && dnf clean all

# Install 'uv' (Python package manager recommended in README)
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/root/.cargo/bin:${PATH}"

# Copy the project files
COPY . .

# Compile C++ modules
RUN g++ -o modbus_reader     modbus.cpp && \
    g++ -o receivermodule    recievermodule.cpp && \
    g++ -o transmittermodule transmittermodule.cpp && \
    g++ -o nema_l298n        nema_l298n.cpp

# Set up Dashboard Server
WORKDIR /app/dashboard/server
RUN npm install

# Set up Dashboard Web
WORKDIR /app/dashboard/web
RUN npm install && npm run build

# Return to root app directory
WORKDIR /app

# The main program requires access to /dev/ttyAML6 and /sys/class/gpio.
# These must be mapped at runtime (e.g., --privileged or --device).
# Default command runs the main orchestrator.
CMD ["uv", "run", "main.py"]
