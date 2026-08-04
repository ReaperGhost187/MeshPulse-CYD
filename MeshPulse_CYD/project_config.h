#pragma once

// Short labels work best in the compact 320x240 header.
#define MESHPULSE_HEADER_LABEL "MESH"
#define MESHCORE_REGION "ABC"
#define MESHCORE_REPEATER "repeater-id"

// Topics published by your MeshCore-to-MQTT bridge.
#define MESHCORE_STATUS_TOPIC "meshcore/" MESHCORE_REGION "/" MESHCORE_REPEATER "/status"
#define MESHCORE_PACKETS_TOPIC "meshcore/" MESHCORE_REGION "/" MESHCORE_REPEATER "/packets"
