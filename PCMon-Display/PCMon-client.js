const si = require('systeminformation');
const { SerialPort } = require('serialport');

// --- CONFIG ---
const SERIAL_PORT_PATH = 'COM8'; 
const BAUD_RATE = 115200;
const UPDATE_INTERVAL = 1000; // Milliseconds

// Initialize Serial Port
const port = new SerialPort({
    path: SERIAL_PORT_PATH,
    baudRate: BAUD_RATE,
    autoOpen: false
});

// Global trackers for differential calculations (Network/Disk)
let lastStats = {
    rx: 0,
    tx: 0,
    diskIO: 0,
    time: Date.now()
};

/**
 * Main Data Collection Loop
 */
async function getStats() {
    try {
        // Fetch data in parallel for efficiency
        const [cpu, temp, mem, gpu, net, fs] = await Promise.all([
            si.currentLoad(),
            si.cpuTemperature(),
            si.mem(),
            si.graphics(),
            si.networkStats(),
            si.fsStats()
        ]);

        // Calculate Network & Disk "Activity" Percentages (Heuristic-based like your Python script)
        const now = Date.now();
        const secondsElapsed = (now - lastStats.time) / 1000;

        // Network (KB/s)
        const netData = net[0]; // Primary interface
        const netU = ((netData.tx_bytes - lastStats.tx) / 1024) / secondsElapsed;
        const netD = ((netData.rx_bytes - lastStats.rx) / 1024) / secondsElapsed;

        // Disk (MB/s)
        const currentDiskIO = fs.wx_bytes + fs.rx_bytes;
        const diskDiff = ((currentDiskIO - lastStats.diskIO) / 1024 / 1024) / secondsElapsed;

        // Update trackers
        lastStats = {
            rx: netData.rx_bytes,
            tx: netData.tx_bytes,
            diskIO: currentDiskIO,
            time: now
        };

        // GPU Logic (Handles first controller found)
        const mainGpu = gpu.controllers[0] || {};

        const stats = {
            cpu: Math.round(cpu.currentLoad),
            cput: Math.round(temp.main || 0),
            mem: Math.round(mem.used / mem.total * 100),
            gpu: mainGpu.utilizationGpu || 0,
            gput: mainGpu.temperatureGpu || 0,
            gpum: mainGpu.utilizationMemory || 0,
            netu: Math.min(100, Math.round(netU / 5)), 
            netd: Math.min(100, Math.round(netD / 10)),
            disk: Math.min(100, Math.round(diskDiff * 2))
        };

        return stats;

    } catch (err) {
        console.error("Error collecting stats:", err);
        return null;
    }
}

/**
 * Execution Logic
 */
port.open((err) => {
    if (err) {
        return console.log('Error opening port: ', err.message);
    }
    console.log(`Streaming to ${SERIAL_PORT_PATH}...`);

    setInterval(async () => {
        const data = await getStats();
        if (data) {
            const jsonStr = JSON.stringify(data);
            port.write(jsonStr + "\n", (err) => {
                if (err) return console.log('Error on write: ', err.message);
                console.log('Sent:', jsonStr);
            });
        }
    }, UPDATE_INTERVAL);
});