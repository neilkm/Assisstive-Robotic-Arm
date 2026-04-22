from pathlib import Path


def serial_ports():
    try:
        from serial.tools import list_ports
    except ImportError:
        return []

    return [port.device for port in list_ports.comports()]


def collect_device_status(config):
    known_ports = set(serial_ports())
    statuses = {}

    for name, device in config.get("devices", {}).items():
        serial_port = device.get("serial_port")
        exists = False
        if serial_port:
            exists = Path(serial_port).exists() or serial_port in known_ports

        statuses[name] = {
            "enabled": bool(device.get("enabled", True)),
            "usbipd_busid": device.get("usbipd_busid"),
            "serial_port": serial_port,
            "serial_present": exists,
        }

    return statuses

