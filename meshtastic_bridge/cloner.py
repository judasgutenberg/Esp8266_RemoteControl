import meshtastic.serial_interface
import meshtastic.util
import time
import yaml


# Load your YAML configuration
with open("config_backup.yaml", "r") as f:
    cfg = yaml.safe_load(f)

# Connect to the radio (port as first positional argument)
iface = meshtastic.serial_interface.SerialInterface("/dev/ttyUSB0")

# Wait a few seconds for handshake
time.sleep(2)
