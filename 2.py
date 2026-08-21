import collections
import configparser
import os
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from typing import List, Optional, Tuple

try:
    import clr
except ImportError as exc:
    print(f"Missing dependency: pythonnet ({exc})")
    print("Install with: pip install pythonnet")
    raise SystemExit(1)

try:
    import serial
    import serial.tools.list_ports
except ImportError as exc:
    print(f"Missing dependency: pyserial ({exc})")
    print("Install with: pip install pyserial")
    raise SystemExit(1)

try:
    import pystray
except ImportError as exc:
    print(f"Missing dependency: pystray ({exc})")
    print("Install with: pip install pystray")
    raise SystemExit(1)

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:
    print(f"Missing dependency: pillow ({exc})")
    print("Install with: pip install pillow")
    raise SystemExit(1)

DEFAULT_COM_PORT = "COM5"
DEFAULT_BAUDRATE = 115200
DEFAULT_UPDATE_INTERVAL = 2.0
DEFAULT_RECONNECT_INTERVAL = 3.0
DEFAULT_WAKE_GAP_SECONDS = 8.0
DEFAULT_WAKE_RECOVERY_SECONDS = 20.0
DEFAULT_ACK_TIMEOUT_SECONDS = 10.0
DEFAULT_TRAY_UPDATE_INTERVAL = 1.5
DEFAULT_HISTORY_SIZE = 30
DEFAULT_PORT_SCAN_KEYWORDS = "bluetooth,蓝牙,bthenum"
DEFAULT_TARGET_HWID_CONTAINS = ""
DEFAULT_TARGET_DESCRIPTION_CONTAINS = ""

CONFIG_FILE = "com.ini"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MIN_VALID_TEMP = 0.0
MAX_VALID_TEMP = 120.0
TEMP_COLORS = {
    "low": (0, 220, 0),
    "medium": (255, 165, 0),
    "high": (255, 45, 45),
}
_DEFAULT_FONT = ImageFont.load_default()

if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)


def log(message: str) -> None:
    try:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] {message}")
    except OSError:
        pass


def normalize_port_name(port: str) -> str:
    return port.strip().upper()


@dataclass
class PortInfo:
    device: str
    description: str
    hwid: str
    manufacturer: str

    @property
    def search_blob(self) -> str:
        return " ".join([self.device, self.description, self.hwid, self.manufacturer]).lower()


def get_serial_ports() -> List[PortInfo]:
    ports: List[PortInfo] = []
    for port_info in serial.tools.list_ports.comports():
        if not port_info.device:
            continue
        ports.append(
            PortInfo(
                device=normalize_port_name(port_info.device),
                description=(port_info.description or "").strip(),
                hwid=(port_info.hwid or "").strip(),
                manufacturer=(getattr(port_info, "manufacturer", "") or "").strip(),
            )
        )
    return ports


def load_config(path: str) -> Tuple[str, int, float, float, float, float, float, int, List[str], str, str]:
    if not os.path.exists(path):
        config = configparser.ConfigParser()
        config["SERIAL"] = {"port": DEFAULT_COM_PORT, "baudrate": str(DEFAULT_BAUDRATE)}
        config["SETTINGS"] = {
            "update_interval": str(DEFAULT_UPDATE_INTERVAL),
            "reconnect_interval": str(DEFAULT_RECONNECT_INTERVAL),
            "wake_gap_seconds": str(DEFAULT_WAKE_GAP_SECONDS),
            "wake_recovery_seconds": str(DEFAULT_WAKE_RECOVERY_SECONDS),
            "ack_timeout_seconds": str(DEFAULT_ACK_TIMEOUT_SECONDS),
            "tray_update_interval": str(DEFAULT_TRAY_UPDATE_INTERVAL),
            "history_size": str(DEFAULT_HISTORY_SIZE),
        }
        config["DISCOVERY"] = {
            "port_scan_keywords": DEFAULT_PORT_SCAN_KEYWORDS,
            "target_hwid_contains": DEFAULT_TARGET_HWID_CONTAINS,
            "target_description_contains": DEFAULT_TARGET_DESCRIPTION_CONTAINS,
        }
        with open(path, "w", encoding="utf-8") as f:
            config.write(f)

    config = configparser.ConfigParser()
    config.read(path, encoding="utf-8")
    port = normalize_port_name(config.get("SERIAL", "port", fallback=DEFAULT_COM_PORT))
    baudrate = config.getint("SERIAL", "baudrate", fallback=DEFAULT_BAUDRATE)
    update_interval = config.getfloat("SETTINGS", "update_interval", fallback=DEFAULT_UPDATE_INTERVAL)
    reconnect_interval = config.getfloat("SETTINGS", "reconnect_interval", fallback=DEFAULT_RECONNECT_INTERVAL)
    wake_gap_seconds = config.getfloat("SETTINGS", "wake_gap_seconds", fallback=DEFAULT_WAKE_GAP_SECONDS)
    wake_recovery_seconds = config.getfloat("SETTINGS", "wake_recovery_seconds", fallback=DEFAULT_WAKE_RECOVERY_SECONDS)
    ack_timeout_seconds = config.getfloat("SETTINGS", "ack_timeout_seconds", fallback=DEFAULT_ACK_TIMEOUT_SECONDS)
    tray_update_interval = config.getfloat("SETTINGS", "tray_update_interval", fallback=DEFAULT_TRAY_UPDATE_INTERVAL)
    history_size = max(10, config.getint("SETTINGS", "history_size", fallback=DEFAULT_HISTORY_SIZE))
    port_scan_keywords = [
        item.strip().lower()
        for item in config.get("DISCOVERY", "port_scan_keywords", fallback=DEFAULT_PORT_SCAN_KEYWORDS).split(",")
        if item.strip()
    ]
    target_hwid_contains = config.get("DISCOVERY", "target_hwid_contains", fallback=DEFAULT_TARGET_HWID_CONTAINS).strip().lower()
    target_description_contains = config.get("DISCOVERY", "target_description_contains", fallback=DEFAULT_TARGET_DESCRIPTION_CONTAINS).strip().lower()
    return (
        port,
        baudrate,
        update_interval,
        reconnect_interval,
        wake_gap_seconds,
        max(5.0, wake_recovery_seconds),
        max(4.0, ack_timeout_seconds),
        history_size,
        port_scan_keywords,
        target_hwid_contains,
        target_description_contains,
    )


class HardwareTemperatureReader:
    def __init__(self) -> None:
        self.computer = None
        self.hardware_type = None
        self.sensor_type = None
        self.gpu_types: List[object] = []
        self.target_hardware: List[object] = []

    def _add_reference(self, dll_name: str) -> None:
        dll_path = os.path.join(SCRIPT_DIR, dll_name)
        if os.path.exists(dll_path):
            clr.AddReference(dll_path)
        else:
            clr.AddReference(os.path.splitext(dll_name)[0])

    def open(self) -> bool:
        try:
            if hasattr(os, "add_dll_directory"):
                try:
                    os.add_dll_directory(SCRIPT_DIR)
                except Exception:
                    pass
            self._add_reference("HidSharp.dll")
            self._add_reference("LibreHardwareMonitorLib.dll")
            from LibreHardwareMonitor.Hardware import Computer, HardwareType, SensorType

            self.hardware_type = HardwareType
            self.sensor_type = SensorType
            self.gpu_types = [
                hw for hw in [
                    getattr(HardwareType, "GpuNvidia", None),
                    getattr(HardwareType, "GpuAmd", None),
                    getattr(HardwareType, "GpuIntel", None),
                ] if hw is not None
            ]
            self.computer = Computer()
            self.computer.IsCpuEnabled = True
            self.computer.IsGpuEnabled = True
            self.computer.Open()
            self.target_hardware = [
                hw for hw in self.computer.Hardware
                if hw.HardwareType == self.hardware_type.Cpu or hw.HardwareType in self.gpu_types
            ]
            log("Hardware monitor initialized.")
            return True
        except Exception as exc:
            log(f"Hardware monitor init failed: {exc}")
            self.computer = None
            self.target_hardware = []
            return False

    def close(self) -> None:
        if self.computer is not None:
            try:
                self.computer.Close()
            except Exception:
                pass
        self.computer = None
        self.target_hardware = []

    def reopen(self) -> bool:
        self.close()
        return self.open()

    def read_temperatures(self) -> Tuple[Optional[float], Optional[float]]:
        if not self.target_hardware:
            return None, None
        cpu_temp: Optional[float] = None
        gpu_temp: Optional[float] = None
        try:
            for hardware in self.target_hardware:
                hardware.Update()
                is_cpu = hardware.HardwareType == self.hardware_type.Cpu
                is_gpu = hardware.HardwareType in self.gpu_types
                for sensor in hardware.Sensors:
                    if sensor.SensorType != self.sensor_type.Temperature or sensor.Value is None:
                        continue
                    value = float(sensor.Value)
                    if not (MIN_VALID_TEMP <= value <= MAX_VALID_TEMP):
                        continue
                    if is_cpu:
                        cpu_temp = value if cpu_temp is None else max(cpu_temp, value)
                    elif is_gpu:
                        gpu_temp = value if gpu_temp is None else max(gpu_temp, value)
        except Exception as exc:
            log(f"Read temperature error: {exc}")
            return None, None
        return cpu_temp, gpu_temp


class SerialBridge:
    def __init__(self, preferred_port: str, baudrate: int, ack_timeout_seconds: float, port_scan_keywords: List[str], target_hwid_contains: str, target_description_contains: str) -> None:
        self.preferred_port = normalize_port_name(preferred_port)
        self.port = self.preferred_port
        self.baudrate = baudrate
        self.ack_timeout_seconds = ack_timeout_seconds
        self.port_scan_keywords = port_scan_keywords
        self.target_hwid_contains = target_hwid_contains
        self.target_description_contains = target_description_contains
        self.conn: Optional[serial.Serial] = None
        self.last_tx_monotonic = 0.0
        self.last_rx_monotonic = 0.0

    @property
    def connected(self) -> bool:
        return self.conn is not None and self.conn.is_open

    def disconnect(self) -> None:
        if self.conn is not None:
            try:
                if self.conn.is_open:
                    self.conn.close()
            except Exception:
                pass
        self.conn = None
        self.last_tx_monotonic = 0.0
        self.last_rx_monotonic = 0.0

    def is_auto_mode(self) -> bool:
        return self.preferred_port in {"", "AUTO"}

    def port_matches_target(self, port: PortInfo) -> bool:
        return (
            (self.target_hwid_contains and self.target_hwid_contains in port.hwid.lower())
            or (self.target_description_contains and self.target_description_contains in port.description.lower())
        )

    def candidate_ports(self) -> List[PortInfo]:
        ports = get_serial_ports()
        targeted = [port for port in ports if self.port_matches_target(port)]
        preferred = [port for port in ports if port.device == self.preferred_port]
        matched = [
            port for port in ports
            if port not in targeted
            and port.device != self.preferred_port
            and any(keyword in port.search_blob for keyword in self.port_scan_keywords)
        ]
        ordered: List[PortInfo] = []
        seen = set()
        for group in ([targeted, preferred, matched] if not self.is_auto_mode() else [targeted, matched, ports]):
            for port in group:
                if port.device in seen:
                    continue
                ordered.append(port)
                seen.add(port.device)
        return ordered

    def connect(self) -> bool:
        self.disconnect()
        candidates = self.candidate_ports()
        if not candidates:
            log("No matching serial port found yet.")
            return False
        for candidate in candidates:
            try:
                self.conn = serial.Serial(
                    port=candidate.device,
                    baudrate=self.baudrate,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    bytesize=serial.EIGHTBITS,
                    timeout=1,
                    write_timeout=1,
                )
                time.sleep(0.1)
                self.conn.reset_input_buffer()
                self.conn.reset_output_buffer()
                self.port = candidate.device
                self.last_rx_monotonic = time.monotonic()
                log(f"Serial connected: {candidate.device}")
                return True
            except Exception as exc:
                self.conn = None
                log(f"Serial connect failed on {candidate.device}: {exc}")
        return False

    def poll_incoming(self) -> None:
        if not self.connected:
            return
        try:
            while self.conn.in_waiting > 0:
                line = self.conn.readline().decode("utf-8", errors="ignore").strip()
                self.last_rx_monotonic = time.monotonic()
                if line:
                    log(f"Serial RX: {line}")
        except Exception as exc:
            log(f"Serial read failed: {exc}")
            self.disconnect()

    def health_check(self) -> bool:
        if not self.connected:
            return False
        self.poll_incoming()
        now = time.monotonic()
        if self.last_tx_monotonic and now - self.last_tx_monotonic >= self.ack_timeout_seconds:
            if self.last_rx_monotonic < self.last_tx_monotonic:
                log("Serial link stale: forcing reconnect.")
                self.disconnect()
                return False
        return self.connected

    def send_temperatures(self, cpu: Optional[float], gpu: Optional[float]) -> bool:
        if not self.connected:
            return False
        payload = ""
        if cpu is not None:
            payload += f"CPU{cpu:.1f}\n"
        if gpu is not None:
            payload += f"GPU{gpu:.1f}\n"
        if not payload:
            return True
        try:
            self.conn.write(payload.encode("ascii"))
            self.conn.flush()
            self.last_tx_monotonic = time.monotonic()
            return True
        except Exception as exc:
            log(f"Serial write failed: {exc}")
            self.disconnect()
            return False


class FanControllerApp:
    def __init__(self) -> None:
        (
            self.com_port,
            self.baudrate,
            self.update_interval,
            self.reconnect_interval,
            self.wake_gap_seconds,
            self.wake_recovery_seconds,
            self.ack_timeout_seconds,
            history_size,
            self.port_scan_keywords,
            self.target_hwid_contains,
            self.target_description_contains,
        ) = load_config(CONFIG_FILE)
        self.reader = HardwareTemperatureReader()
        self.serial_bridge = SerialBridge(
            self.com_port,
            self.baudrate,
            self.ack_timeout_seconds,
            self.port_scan_keywords,
            self.target_hwid_contains,
            self.target_description_contains,
        )
        self.stop_event = threading.Event()
        self.last_loop_monotonic = time.monotonic()
        self.last_reconnect_attempt = 0.0
        self.last_tray_update = 0.0
        self.current_cpu_temp = 0.0
        self.current_gpu_temp = 0.0
        self.temp_history = collections.deque([40.0] * history_size, maxlen=history_size)
        self.tray_lock = threading.Lock()
        self.tray_icon = None

    def build_wave_icon(self) -> Image.Image:
        image = Image.new("RGB", (64, 64), (0, 0, 0))
        draw = ImageDraw.Draw(image)
        with self.tray_lock:
            history = list(self.temp_history)
            cpu_temp = self.current_cpu_temp
            gpu_temp = self.current_gpu_temp
        draw.rectangle((0, 0, 63, 63), outline=(30, 30, 30), fill=(0, 0, 0))
        if not history:
            draw.text((24, 24), "--", fill=(180, 180, 180), font=_DEFAULT_FONT)
            return image
        min_temp = min(history)
        max_temp = max(history)
        if max_temp - min_temp < 1:
            min_temp -= 1
            max_temp += 1
        points = []
        for index, temp in enumerate(history):
            x = 4 if len(history) == 1 else int(4 + index * (56 / (len(history) - 1)))
            y = int(54 - (temp - min_temp) * (46 / (max_temp - min_temp)))
            points.append((x, max(8, min(54, y))))
        for index in range(len(points) - 1):
            avg = (history[index] + history[index + 1]) / 2.0
            color = TEMP_COLORS["low"] if avg <= 50 else TEMP_COLORS["medium"] if avg <= 70 else TEMP_COLORS["high"]
            draw.line((points[index], points[index + 1]), fill=color, width=2)
        draw.text((2, 56), f"{int(round(cpu_temp)):02d}", fill=(190, 220, 255), font=_DEFAULT_FONT)
        draw.text((36, 56), f"{int(round(gpu_temp)):02d}", fill=(255, 200, 170), font=_DEFAULT_FONT)
        return image

    def build_tray_menu(self) -> pystray.Menu:
        return pystray.Menu(
            pystray.MenuItem(f"CPU: {self.current_cpu_temp:.1f} C", None, enabled=False),
            pystray.MenuItem(f"GPU: {self.current_gpu_temp:.1f} C", None, enabled=False),
            pystray.MenuItem(f"PORT: {self.serial_bridge.port} ({'OK' if self.serial_bridge.connected else 'ERR'})", None, enabled=False),
            pystray.MenuItem("Exit", self.on_tray_exit),
        )

    def on_tray_exit(self, icon, item) -> None:
        self.stop_event.set()
        if self.tray_icon:
            self.tray_icon.stop()

    def create_tray_icon(self) -> None:
        self.tray_icon = pystray.Icon("esp32_fan", self.build_wave_icon(), "ESP32 Fan Controller", self.build_tray_menu())
        threading.Thread(target=self.tray_icon.run, daemon=True).start()

    def update_tray_icon(self) -> None:
        if not self.tray_icon:
            return
        now = time.monotonic()
        if now - self.last_tray_update < DEFAULT_TRAY_UPDATE_INTERVAL:
            return
        self.last_tray_update = now
        self.tray_icon.icon = self.build_wave_icon()
        self.tray_icon.title = f"CPU: {self.current_cpu_temp:.1f}C | GPU: {self.current_gpu_temp:.1f}C"
        self.tray_icon.menu = self.build_tray_menu()

    def handle_wake_resume(self, gap_seconds: float) -> None:
        log(f"Wake detected ({gap_seconds:.1f}s).")
        self.serial_bridge.disconnect()
        self.reader.reopen()
        deadline = time.monotonic() + self.wake_recovery_seconds
        while not self.stop_event.is_set() and time.monotonic() < deadline:
            if self.serial_bridge.connect():
                return
            self.stop_event.wait(1.0)

    def ensure_serial_connected(self) -> None:
        self.serial_bridge.health_check()
        if self.serial_bridge.connected:
            return
        now = time.monotonic()
        if now - self.last_reconnect_attempt < self.reconnect_interval:
            return
        self.last_reconnect_attempt = now
        self.serial_bridge.connect()

    def run(self) -> None:
        log("=" * 60)
        log("ESP32 fan controller started")
        log("=" * 60)
        if not self.reader.open():
            return
        self.serial_bridge.connect()
        self.create_tray_icon()
        try:
            while not self.stop_event.is_set():
                now = time.monotonic()
                gap = now - self.last_loop_monotonic
                self.last_loop_monotonic = now
                if gap >= self.wake_gap_seconds:
                    self.handle_wake_resume(gap)
                self.ensure_serial_connected()
                cpu, gpu = self.reader.read_temperatures()
                if cpu is not None or gpu is not None:
                    with self.tray_lock:
                        if cpu is not None:
                            self.current_cpu_temp = cpu
                        if gpu is not None:
                            self.current_gpu_temp = gpu
                        self.temp_history.append(self.current_cpu_temp if self.current_cpu_temp > 0 else self.current_gpu_temp)
                    sent = self.serial_bridge.send_temperatures(cpu, gpu)
                    log(f"{'SENT' if sent else 'WAIT'} CPU={cpu or 'NA'}C GPU={gpu or 'NA'}C")
                self.update_tray_icon()
                self.stop_event.wait(self.update_interval)
        finally:
            self.serial_bridge.disconnect()
            self.reader.close()
            if self.tray_icon:
                self.tray_icon.stop()


if __name__ == "__main__":
    FanControllerApp().run()
