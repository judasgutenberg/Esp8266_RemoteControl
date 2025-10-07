import meshtastic.serial_interface
import json
import time

# Connect to the node (USB or BLE)
interface = meshtastic.serial_interface.SerialInterface()

# Give it a second to initialize
time.sleep(1)

# Get the full node config
config = interface.getNodeConfig()  # returns a dict

# Print JSON
print(json.dumps(config, indent=2))

# Save to a file
with open("node_config.json", "w") as f:
    json.dump(config, f, indent=2)

print("Config saved to node_config.json")

with open("node_config.json", "w") as f:
    json.dump(config, f, indent=2)
print("Config saved to node_config.json")