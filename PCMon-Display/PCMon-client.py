import serial
import json
import time
import psutil
import pynvml
import requests
import wmi

# --- CONFIG ---
SERIAL_PORT = 'COM7' 
BAUD_RATE = 115200
UPDATE_INTERVAL = 1

# --- Global Trackers for IO ---
last_net_recv = psutil.net_io_counters().bytes_recv
last_net_sent = psutil.net_io_counters().bytes_sent
last_disk_rw = psutil.disk_io_counters().read_bytes + psutil.disk_io_counters().write_bytes

# GPU Init
try:
    pynvml.nvmlInit()
    gpu_available = True
    gpu_handle = pynvml.nvmlDeviceGetHandleByIndex(0)
except:
    gpu_available = False

def get_cpu_temp():
    """Tries multiple methods to get CPU Temperature on Windows"""
    # Method 1: Open Hardware Monitor Web Server (Most Reliable)
    try:
        response = requests.get("http://localhost:8085/data.json", timeout=0.1)
        if response.status_code == 200:
            data = response.json()
            # Recursively find the CPU Temperature sensor
            def find_temp(nodes):
                for node in nodes:
                    if "Temperature" in node.get("Text", "") and "CPU" in node.get("Text", ""):
                        # Return value of first child if available
                        return float(node['Children'][0]['Value'].replace(' °C', '').replace(',', '.'))
                    res = find_temp(node.get("Children", []))
                    if res: return res
                return None
            
            ohm_temp = find_temp(data.get("Children", []))
            if ohm_temp: return int(ohm_temp)
    except:
        pass

    # Method 2: WMI (Requires Administrator privileges, works on some motherboards)
    try:
        w = wmi.WMI(namespace="root\\wmi")
        temperature_info = w.MSAcpi_ThermalZoneTemperature()[0]
        return int(temperature_info.CurrentTemperature / 10.0 - 273.15)
    except:
        pass

    return 0

def get_stats():
    global last_net_recv, last_net_sent, last_disk_rw
    
    # Network Activity
    net_now = psutil.net_io_counters()
    net_d = (net_now.bytes_recv - last_net_recv) / 1024
    net_u = (net_now.bytes_sent - last_net_sent) / 1024
    last_net_recv, last_net_sent = net_now.bytes_recv, net_now.bytes_sent

    # Disk Activity
    disk_now = psutil.disk_io_counters()
    disk_total_now = disk_now.read_bytes + disk_now.write_bytes
    disk_diff = (disk_total_now - last_disk_rw) / 1024 / 1024
    last_disk_rw = disk_total_now

    stats = {
        "cpu": int(psutil.cpu_percent()),
        "cput": get_cpu_temp(),
        "mem": int(psutil.virtual_memory().percent),
        "gpu": 0, "gput": 0, "gpum": 0,
        "netu": min(100, int(net_u / 5)), 
        "netd": min(100, int(net_d / 10)),
        "disk": min(100, int(disk_diff * 2)) 
    }

    # GPU
    if gpu_available:
        try:
            util = pynvml.nvmlDeviceGetUtilizationRates(gpu_handle)
            stats["gpu"] = util.gpu
            stats["gput"] = pynvml.nvmlDeviceGetTemperature(gpu_handle, 0)
            mem = pynvml.nvmlDeviceGetMemoryInfo(gpu_handle)
            stats["gpum"] = int((mem.used / mem.total) * 100)
        except: pass

    return stats

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Streaming to {SERIAL_PORT}...")
        print("Tip: If CPU Temp is 0, ensure Open Hardware Monitor is running as Admin with Web Server enabled.")
        
        while True:
            data = get_stats()
            json_str = json.dumps(data)
            ser.write((json_str + "\n").encode('utf-8'))
            print(f"Sent: {json_str}")
            time.sleep(UPDATE_INTERVAL)
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if gpu_available:
            pynvml.nvmlShutdown()

if __name__ == "__main__":
    main()