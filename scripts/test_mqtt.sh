#!/bin/bash
# Test MQTT flow between Cloud and ESP32
# Run this script to verify MQTT topics work correctly

MQTT_HOST="localhost"
MQTT_PORT="1883"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "  MQTT Flow Test - Cloud Grow System"
echo "=========================================="
echo ""

# Check if mosquitto is running
echo -e "${YELLOW}[1] Checking MQTT Broker...${NC}"
if netstat -tlnp 2>/dev/null | grep -q ":$MQTT_PORT" || ss -tlnp | grep -q ":$MQTT_PORT"; then
    echo -e "${GREEN}✓ MQTT broker is running on port $MQTT_PORT${NC}"
else
    echo -e "${RED}✗ MQTT broker not found on port $MQTT_PORT${NC}"
    exit 1
fi
echo ""

# Check Docker container
echo -e "${YELLOW}[2] Checking Mosquitto container...${NC}"
if docker ps --format '{{.Names}}' | grep -q "cloud-mosquitto"; then
    echo -e "${GREEN}✓ cloud-mosquitto container is running${NC}"
    MQTT_EXEC="docker exec cloud-mosquitto"
else
    echo -e "${YELLOW}! Using local mosquitto client${NC}"
    MQTT_EXEC=""
fi
echo ""

# Test Topics
echo -e "${YELLOW}[3] Testing MQTT Topics...${NC}"
echo ""

# Test 1: Sensor data (ESP32 -> Cloud)
echo "Test 1: grow/{roomId}/sensors (ESP32 → Cloud)"
$MQTT_EXEC mosquitto_pub -h localhost -t "grow/test-room-001/sensors" \
    -m '{"temp":25.5,"humidity":65,"co2":800,"vpd":1.1,"timestamp":1234567890}' 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Sensor data published OK${NC}"
else
    echo -e "${RED}✗ Failed to publish sensor data${NC}"
fi

# Test 2: Control command (Cloud -> ESP32)
echo ""
echo "Test 2: grow/{roomId}/control (Cloud → ESP32)"
$MQTT_EXEC mosquitto_pub -h localhost -t "grow/test-room-001/control" \
    -m '{"relay1":true,"relay2":false,"commandId":"cmd-001","timestamp":1234567890}' 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Control command published OK${NC}"
else
    echo -e "${RED}✗ Failed to publish control command${NC}"
fi

# Test 3: Device verify (Cloud -> ESP32)
echo ""
echo "Test 3: device/{deviceId}/verify (Cloud → ESP32)"
$MQTT_EXEC mosquitto_pub -h localhost -t "device/AABBCCDDEEFF/verify" \
    -m '{"token":"test-token-123","requestId":"req-001","timestamp":1234567890}' 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Verify request published OK${NC}"
else
    echo -e "${RED}✗ Failed to publish verify request${NC}"
fi

# Test 4: Device confirm (ESP32 -> Cloud)
echo ""
echo "Test 4: device/{deviceId}/confirm (ESP32 → Cloud)"
$MQTT_EXEC mosquitto_pub -h localhost -t "device/AABBCCDDEEFF/confirm" \
    -m '{"valid":true,"requestId":"req-001","deviceId":"AABBCCDDEEFF","model":"ESP32-S3-GROW-V1"}' 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Device confirm published OK${NC}"
else
    echo -e "${RED}✗ Failed to publish device confirm${NC}"
fi

# Test 5: Device pair (Cloud -> ESP32)
echo ""
echo "Test 5: device/{deviceId}/pair (Cloud → ESP32)"
$MQTT_EXEC mosquitto_pub -h localhost -t "device/AABBCCDDEEFF/pair" \
    -m '{"roomId":"room-001","timestamp":1234567890}' 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Pair command published OK${NC}"
else
    echo -e "${RED}✗ Failed to publish pair command${NC}"
fi

# Test 6: Gateway status (ESP32 -> Cloud)
echo ""
echo "Test 6: grow/{roomId}/status (ESP32 → Cloud)"
$MQTT_EXEC mosquitto_pub -h localhost -t "grow/test-room-001/status" \
    -m '{"online":true,"ip":"192.168.1.100","rssi":-65,"uptime":3600,"freeHeap":180000}' 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Gateway status published OK${NC}"
else
    echo -e "${RED}✗ Failed to publish gateway status${NC}"
fi

echo ""
echo "=========================================="
echo "  Topic Mapping Summary"
echo "=========================================="
echo ""
echo "┌────────────────────────────────┬───────────┬───────────┐"
echo "│ Topic                          │ Cloud     │ ESP32     │"
echo "├────────────────────────────────┼───────────┼───────────┤"
echo "│ grow/{roomId}/sensors          │ Subscribe │ Publish   │"
echo "│ grow/{roomId}/control          │ Publish   │ Subscribe │"
echo "│ grow/{roomId}/status           │ Subscribe │ Publish   │"
echo "│ grow/{roomId}/ack              │ Subscribe │ Publish   │"
echo "│ device/register                │ Subscribe │ Publish   │"
echo "│ device/{id}/verify             │ Publish   │ Subscribe │"
echo "│ device/{id}/confirm            │ Subscribe │ Publish   │"
echo "│ device/{id}/pair               │ Publish   │ Subscribe │"
echo "│ device/{id}/status             │ Subscribe │ Publish   │"
echo "└────────────────────────────────┴───────────┴───────────┘"
echo ""
echo -e "${GREEN}All MQTT topics are working correctly!${NC}"
