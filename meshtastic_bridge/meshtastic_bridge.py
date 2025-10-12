#!/usr/bin/env python3
import meshtastic
import meshtastic.serial_interface
import random
import json
import time
import re
from urllib.parse import quote
import requests
from pubsub import pub

# -------------------------------
# Load configuration
# -------------------------------
with open("config.json", "r") as f:
    CONFIG = json.load(f)

# -------------------------------
# Connect to the local serial node
# -------------------------------
SERIAL_PORT = '/dev/ttyUSB0'
iface = meshtastic.serial_interface.SerialInterface(SERIAL_PORT)
print("Connected device info:", iface.myInfo)

# -------------------------------
# Column lists
# -------------------------------
weatherDataColumnList  = "temperature,pressure,humidity,gas_metric,wind_direction,wind_speed,wind_increment,precipitation,radiation_count,lightning_count,illumination,color_metric,satsInView,sats_in_view,reserved3,reserved4,sensor_id,device_feature_id,sensor_name,consolidate"
whereAndWhenDataColumnList = "nada,time,rx_time,hop_limit,latitude,longitude,altitude,ground_speed,PDOP"
energyDataColumnList = "batteryLevel,voltage,amperage"

# -------------------------------
# Helper: safely convert any object to JSON-serializable
# -------------------------------
def make_json_serializable(obj):
    if isinstance(obj, (str, int, float, bool)) or obj is None:
        return obj
    if isinstance(obj, bytes):
        return obj.hex()
    if isinstance(obj, dict):
        return {k: make_json_serializable(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [make_json_serializable(v) for v in obj]
    # fallback for unknown types like Telemetry, DeviceMetrics, etc.
    return str(obj)

# -------------------------------
# Misc helper functions
# -------------------------------
def calculateChecksum(s: str) -> int:
    return sum(ord(c) for c in s) % 256

def countZeroes(s: str) -> int:
    return sum(1 for c in s if ord(c) == 48) % 256

def rotateRight(value: int, count: int) -> int:
    return ((value >> count) | (value << (8 - count))) & 0xFF

def rotateLeft(value: int, count: int) -> int:
    return ((value << count) | (value >> (8 - count))) & 0xFF

def countSetBitsInString(s: str) -> int:
    bit_count = 0
    for c in s:
        v = ord(c)
        while v:
            bit_count += v & 1
            v >>= 1
    return bit_count % 256

# -------------------------------
# Core encryption/decryption
# -------------------------------
def generateDecryptedByte(counter, nibble, byte_storage, byte_data, byte_timestamp,
                          checksum_data, checksum_timestamp,
                          setbits_data, setbits_timestamp,
                          zerocount_data, zerocount_timestamp):
    if nibble == 1: return byte_storage ^ zerocount_data
    if nibble == 2: return byte_storage ^ byte_timestamp
    if nibble == 3: return byte_storage ^ byte_data
    if nibble == 4: return byte_storage ^ rotateLeft(byte_data, 1)
    if nibble == 5: return byte_storage ^ rotateRight(byte_data, 1)
    if nibble == 6: return byte_storage ^ checksum_data
    if nibble == 7: return byte_storage ^ checksum_timestamp
    if nibble == 8: return byte_storage ^ setbits_data
    if nibble == 9: return byte_storage ^ setbits_timestamp
    if nibble == 10: return rotateLeft(byte_storage, 1)
    if nibble == 11: return rotateRight(byte_storage, 1)
    if nibble == 12: return rotateLeft(byte_storage, 2)
    if nibble == 13: return rotateRight(byte_storage, 2)
    if nibble == 14: return ~byte_storage & 0xFF
    if nibble == 15: return byte_storage ^ zerocount_timestamp
    return byte_storage

def encryptStoragePassword(data_string: str, storage_password: str, encryption_scheme: int) -> str:
    timestamp_epoch = int(time.time())
    timestamp_string_full = str(timestamp_epoch)
    timestamp_string = timestamp_string_full[1:8]

    nibbles = [(encryption_scheme >> (4 * (15 - i))) & 0xF for i in range(16)]

    data_checksum = calculateChecksum(data_string)
    timestamp_checksum = calculateChecksum(timestamp_string)
    data_setbits = countSetBitsInString(data_string)
    timestamp_setbits = countSetBitsInString(timestamp_string)
    data_zero_count = countZeroes(data_string)
    timestamp_zero_count = countZeroes(timestamp_string)

    processed_chars = []
    counter = 0
    this_byte_result = 0

    for i in range(len(storage_password) * 2):
        byte_data = ord(data_string[counter % len(data_string)])
        byte_timestamp = ord(timestamp_string[counter % len(timestamp_string)])

        if i % 2 == 0:
            byte_storage = ord(storage_password[counter])
        else:
            byte_storage = this_byte_result

        nibble = nibbles[i % 16]
        this_byte_result = generateDecryptedByte(counter, nibble, byte_storage, byte_data, byte_timestamp,
                                                 data_checksum, timestamp_checksum,
                                                 data_setbits, timestamp_setbits,
                                                 data_zero_count, timestamp_zero_count)
        if i % 2 == 1:
            processed_chars.append(f"{this_byte_result:02X}")
            counter += 1

    return ''.join(processed_chars)

# -------------------------------
# Flatten decoded packet
# -------------------------------

def flatten_dict(d):
    flat = {}
    for k, v in d.items():
        if isinstance(v, dict):
            flat.update(flatten_dict(v))  # recursively merge nested dicts
        else:
            flat[k] = v
    return flat

def flattenDecoded(decoded):
    if isinstance(decoded, dict):
        return flatten_dict(decoded)
    elif isinstance(decoded, str):
        return {"text": decoded}
    else:
        return {"raw": str(decoded)}

# -------------------------------
# Build delimited string
# -------------------------------
def buildDelimitedDataString(decoded, columnList):
    keys = columnList.split(",")
    return "*".join(str(decoded.get(k, "")) for k in keys)


MAX_RETRIES = 10
RETRY_DELAY = 5  # seconds
pendingAcks = {}

def sendLoraMessages(backendJson):
    global iface, pendingAcks
    messageObject = json.loads(backendJson)
    print(messageObject)
    if "messages" in messageObject:
        messages = messageObject["messages"]
        for message in messages:
            messageText = message["content"]
            messageId = message["message_id"]
            packetInfo = iface.sendText(messageText, wantAck=True)
            loraId = packetInfo.id
            pendingAcks[loraId] = messageId  
  

def sendWithRetries(flat, nodeId, loraMessageId, messageId = 0):
    for attempt in range(1, MAX_RETRIES + 1):
        try:
            if messageId:
                url = buildUrl(False, nodeId, loraMessageId, messageId)
            else:
                url = buildUrl(flat, nodeId, loraMessageId)
            response = requests.get(url, timeout=10)
            response.raise_for_status()
            print("Server responded with status:", response.status_code)
            print("\n\n", response.text)
            sendLoraMessages(response.text)
            # Check for "insufficient permissions"
            if "you lack permissions" in response.text.lower():
                raise PermissionError("Server responded with insufficient permissions")

            return True  # Success, stop retrying

        except (requests.RequestException, PermissionError) as e:
            print(f"Attempt {attempt} failed: {e}")
            if attempt < MAX_RETRIES:
                print(f"Retrying in {RETRY_DELAY} seconds...")
                time.sleep(RETRY_DELAY)
            else:
                print("Max retries reached, giving up.")
                return False

def updateLostAndFoundCoordinates(flat_obj):
    """
    If flat_obj['text'] contains a Lost & Found GPS string, 
    update or add 'latitude' and 'longitude' properties.
    
    Returns the updated flat_obj.
    """
    text = flat_obj.get("text")
    if not text:
        return flat_obj
    if "I'm lost!" in flat_obj.get("text", ""):
        # Match "Lat / Lon: 43.119885, -74.342039" with optional trailing characters
        match = re.search(r"Lat / Lon:\s*([-\d.]+),\s*([-\d.]+)", text)
        if match:
            flat_obj['latitude'] = float(match.group(1))
            flat_obj['longitude'] = float(match.group(2))
    return flat_obj

def buildUrl(flat, loraNodeId, loraMessageId, messageId = 0):
    if flat:
        weatherDataString = buildDelimitedDataString(flat, weatherDataColumnList)
        whereAndWhenString = buildDelimitedDataString(flat, whereAndWhenDataColumnList)
        energyString = buildDelimitedDataString(flat, energyDataColumnList)
        dataToSend = weatherDataString + "|" + whereAndWhenString + "|" + energyString
    else:
        dataToSend = str(random.random()) + "*" + str(messageId) + "*special*message_id"

    encryption_scheme_int = int(CONFIG['encryption_scheme'], 16)
    encryptedStoragePassword = encryptStoragePassword( dataToSend, CONFIG['storage_password'], encryption_scheme_int)
    loraMessageIdStr = str(loraMessageId)
    url = f"http://{CONFIG['host_get']}{CONFIG['url_get']}?k2={encryptedStoragePassword}&manufacture_id={loraNodeId}&mode=saveData&data={quote(dataToSend)}&lora_id={quote(loraMessageIdStr)}"
    if flat:
        if "text" in flat:
          url = url + "&message=" + quote(flat["text"])
    print(url)
    return url
        
        
            
def onReceive(packet, interface):
    global pendingAcks
    #print("----------------")
    #print(packet)
    #print("++++++++++++++++")
    packet_safe = make_json_serializable(packet)

    if 'decoded' in packet_safe:
        decoded = packet_safe['decoded']
        nodeId = packet_safe.get('from')
        if decoded.get("portnum") == "ROUTING_APP":
            errorReason = decoded.get("errorReason")
            if errorReason is None or errorReason == "NONE":
                
                loraMessageId = decoded.get('requestId')
                print(str(loraMessageId) + "GOT AN ACK!!!!\n\n\n");
                #do a special hacky update to the backend
                messageId = pendingAcks[loraMessageId]
                sendWithRetries("", nodeId, loraMessageId, messageId)
        else:
            loraMessageId = packet_safe.get('id')
            flat = flattenDecoded(decoded)
            flat = updateLostAndFoundCoordinates(flat)
            print("\n\nDecoded from node", nodeId, ":", flat)
            sendWithRetries(flat, nodeId, loraMessageId)

# -------------------------------
# Subscribe to incoming packets
# -------------------------------
pub.subscribe(onReceive, "meshtastic.receive")

# -------------------------------
# Main loop
# -------------------------------
if __name__ == "__main__":
    print("Listening for Meshtastic packets...")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nInterrupted, closing interface...")
        iface.close()
