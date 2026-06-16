#pragma once

#include <cstdint>

// HWiNFO64 shared memory structures.
// These layouts must match HWiNFO's shared memory ABI exactly.
#pragma pack(push, 1)

static constexpr uint32_t HWiNFO_SENSORS_SM2_SIGNATURE = 0x534D3248;

enum HwInfoReadingType : int32_t {
    SENSOR_TYPE_NONE        = 0,
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

struct HwInfoSensorsSharedMem {
    uint32_t signature;
    uint32_t version;
    uint32_t size;
    uint32_t numSensorElements;
    uint32_t numReadingElements;
    uint64_t pollTime;
};

static constexpr int HWiNFO_SENSOR_ELEMENT_NAME_LEN = 128;
static constexpr int HWiNFO_SENSOR_ELEMENT_LABEL_LEN = 128;

struct HwInfoSensorElement {
    uint32_t sensorId;
    uint32_t sensorInst;
    wchar_t sensorName[HWiNFO_SENSOR_ELEMENT_NAME_LEN];
    wchar_t sensorLabel[HWiNFO_SENSOR_ELEMENT_LABEL_LEN];
};

static constexpr int HWiNFO_READING_LABEL_LEN = 128;
static constexpr int HWiNFO_READING_UNIT_LEN = 16;

struct HwInfoReadingElement {
    uint32_t sensorId;
    uint32_t sensorInst;
    HwInfoReadingType readingType;
    double value;
    wchar_t label[HWiNFO_READING_LABEL_LEN];
    wchar_t unit[HWiNFO_READING_UNIT_LEN];
};

#pragma pack(pop)

inline const HwInfoSensorElement* GetSensorElements(
    const HwInfoSensorsSharedMem* header)
{
    return reinterpret_cast<const HwInfoSensorElement*>(
        reinterpret_cast<const uint8_t*>(header) + sizeof(HwInfoSensorsSharedMem));
}

inline const HwInfoReadingElement* GetReadingElements(
    const HwInfoSensorsSharedMem* header)
{
    return reinterpret_cast<const HwInfoReadingElement*>(
        reinterpret_cast<const uint8_t*>(header)
        + sizeof(HwInfoSensorsSharedMem)
        + header->numSensorElements * sizeof(HwInfoSensorElement));
}
