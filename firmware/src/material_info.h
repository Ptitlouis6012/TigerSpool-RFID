#pragma once
#include <cstdint>

// What the TigerTag material table says about one material, beyond its label.
//
// A plain struct with no Arduino in it, on purpose: tt_db fills it from the
// downloaded or the compiled table, and filament_resolve.cpp reads it without
// knowing which - so the resolver compiles and is tested off the device.
//
// Every field may be absent. A string is absent when it is null, "" or "-"; a
// number is absent when it is 0. The resolver decides what absent means.
struct MaterialInfo {
    uint16_t    id = 0;
    const char* materialType = nullptr; // material_type, the family: "PLA" for
                                        // "PLA High Speed"
    const char* crealityId = nullptr;   // metadata.crealityID, e.g. "00001"
    double      pressure = 0;           // metadata.crealityPressureAdvance
    uint16_t    nozMin = 0, nozMax = 0; // recommended.nozzleTempMin / Max
};
