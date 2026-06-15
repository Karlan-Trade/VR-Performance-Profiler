#pragma once

#include <cstdint>

// HWiNFO64 shared memory structures
// Reference: https://www.hwinfo.com/info/ (Shared Memory section)
//
// IMPORTANT: These structs must match HWiNFO's byte layout exactly.
// Use #pragma pack(push, 1) to prevent compiler padding.

#pragma pack(push, 1)

// Shared memory signature
static constexpr uint32_t HWiNFO_SENSORS_SM2_SIGNATURE = 0x534D3248; // "SM2H" in little-endian

// Reading type IDs (as reported by HWiNFO)
enum HwInfoReadingType : int32_t {
    SENSOR_TYPE_NONE       = 0,
    SENSOR_TYPE_TEMPERATURE = 1,
    SENSOR_TYPE_VOLTAGE     = 2,
    SENSOR_TYPE_FAN         = 3,
    SENSOR_TYPE_CURRENT     = 4,
    SENSOR_TYPE_POWER       = 5,
    SENSOR_TYPE_CLOCK       = 6,
    SENSOR_TYPE_LOAD        = 7,
    SENSOR_TYPE_ELECTRIC    = 8,
    SENSOR_TYPE_CONTROL     = 9,
    SENSOR_TYPE_LEVEL       = 10,
    SENSOR_TYPE_FACTOR      = 11,
    SENSOR_TYPE_POWER_DISS  = 12,
    SENSOR_TYPE_DATA        = 13,
    SENSOR_TYPE_SMALL_DATA  = 14,
    SENSOR_TYPE_THROUGHPUT  = 15,
};

// Top-level shared memory header
struct HwInfoSensorsSharedMem {
    uint32_t signature;          // Must be HWiNFO_SENSORS_SM2_SIGNATURE
    uint32_t version;            // Structure version
    uint32_t size;               // Total size of shared memory
    uint32_t numSensorElements;  // Number of sensor elements
    uint32_t numReadingElements; // Number of reading elements
    uint64_t pollTime;           // Timestamp of last update (100ns units since 1601-01-01)
};

// Per-sensor element (hardware device)
static constexpr int HWiNFO_SENSOR_ELEMENT_NAME_LEN = 128;
static constexpr int HWiNFO_SENSOR_ELEMENT_LABEL_LEN = 128;

struct HwInfoSensorElement {
    uint32_t sensorId;
    uint32_t sensorInst;  // Instance index
    wchar_t sensorName[HWiNFO_SENSOR_ELEMENT_NAME_LEN];
    wchar_t sensorLabel[HWiNFO_SENSOR_ELEMENT_LABEL_LEN];
};

// Per-reading element (individual measurement)
static constexpr int HWiNFO_READING_LABEL_LEN = 128;
static constexpr int HWiNFO_READING_UNIT_LEN  = 16;

struct HwInfoReadingElement {
    uint32_t sensorId;
    uint32_t sensorInst;
    HwInfoReadingType readingType;
    double value;
    wchar_t label[HWiNFO_READING_LABEL_LEN];
    wchar_t unit[HWiNFO_READING_UNIT_LEN];
};

#pragma pack(pop)

// Compute offsets for accessing sensor and reading arrays
inline const HwInfoSensorElement* GetSensorElements(const HwInfoSensorsSharedMem* header)
{
    return reinterpret_cast<const HwInfoSensorElement*>(
        reinterpret_cast<const uint8_t*>(header) + sizeof(HwInfoSensorsSharedMem)
    );
}

inline const HwInfoReadingElement* GetReadingElements(const HwInfoSensorsSharedMem* header)
{
    return reinterpret_cast<const HwInfoReadingElement*>(
        reinterpret_cast<const uint8_t*>(header)
        + sizeof(HwInfoSensorsSharedMem)
        + header->numSensorElements * sizeof(HwInfoSensorElement)
    );
}
