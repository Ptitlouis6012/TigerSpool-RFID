#pragma once
#include <cstddef>
#include <cstdint>
#include "material_info.h"

// Which value a printer is sent for a spool, when three places can answer.
//
// A TigerTag+ spool's product is described online by TigerTag's product
// endpoint, with the maker's real temperatures and the printer's own material
// id. The chip carries temperatures and a material id but never a printer's
// material id. The material table carries a generic entry for the material.
// Each field is taken from the FIRST of those that gives a valid value, and
// each field on its own: an answer from the endpoint that lacks the Creality id
// still supplies its temperatures, and only the id falls through to the table.
//
// Nothing here touches the network or the Arduino core. The endpoint's answer
// is fetched elsewhere (product_api.cpp) and handed in, which is what lets the
// order be tested with injected answers on a computer.
namespace filament {

constexpr uint32_t PROTOCOL_TIGERTAG      = 1542820452u;   // TIGER_TAG_V1.0
constexpr uint32_t PROTOCOL_TIGERTAG_PLUS = 3155151767u;   // TIGER_TAG_PRO_V1.0

constexpr uint16_t DEFAULT_NOZ_MIN = 190;
constexpr uint16_t DEFAULT_NOZ_MAX = 240;
constexpr double   DEFAULT_PRESSURE = 0.04;
constexpr const char* DEFAULT_CREALITY_ID = "0";

enum Source : uint8_t { SRC_API, SRC_CHIP, SRC_DB, SRC_DEFAULT };
const char* sourceName(Source s);        // "api", "chip", "db", "default"

// What the product endpoint said, field by field. An empty string or a 0 is a
// field it did not give.
struct ProductData {
    uint32_t id = 0;
    uint16_t nozMin = 0, nozMax = 0;     // nozzle.temp_min / temp_max
    char     crealityId[16] = {0};       // metadata.crealityID
    char     crealityLabel[48] = {0};    // metadata.crealityLabel
    double   pressure = 0;               // metadata.crealityPressureAdvance
};

// Reads one product/get answer. False means the answer does not count at all:
// it does not parse, is not an object, or describes another product than
// `expectedId`. True with fields missing is an ordinary partial answer.
bool parseProduct(const char* json, size_t len, uint32_t expectedId, ProductData& out);

// What the chip itself carries, as plain values.
struct ChipFacts {
    uint32_t    protocol = 0;
    uint32_t    idProduct = 0;
    uint16_t    idMaterial = 0;
    uint16_t    nozMin = 0, nozMax = 0;
    const char* material = "";           // the resolved label, e.g. "PLA"
};

struct ResolvedFilament {
    uint16_t nozMin = 0, nozMax = 0;  Source tempSrc = SRC_DEFAULT;
    char     materialType[24] = {0};  Source typeSrc = SRC_DEFAULT;
    char     crealityId[16] = {0};    Source idSrc = SRC_DEFAULT;
    char     crealityName[64] = {0};  Source nameSrc = SRC_DEFAULT;
    double   pressure = 0;            Source pressureSrc = SRC_DEFAULT;
};

// `api` is the endpoint's answer or nullptr; it is ignored for anything but a
// TigerTag+ and for any other product. `db` is the table's entry for the chip's
// material or nullptr.
ResolvedFilament resolve(const ChipFacts& chip, const ProductData* api, const MaterialInfo* db);

}  // namespace filament
