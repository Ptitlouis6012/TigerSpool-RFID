#pragma once
#include "filament_resolve.h"
#include "reader.h"

// TigerTag's product endpoint, asked about a TigerTag+ spool without holding up
// the interface.
//
// The request starts the moment a TigerTag+ is read, on a task of its own, so
// the answer is usually in by the time somebody presses Send. It is given
// BUDGET_MS from that moment. Whatever has not arrived by then is resolved
// without - and a later answer is kept for the next time that spool is shown,
// never sent as a second frame.
//
// A plain TigerTag never reaches the network from here.
namespace product_api {

constexpr uint32_t BUDGET_MS = 3000;

// Starts the fetch for this spool if it is a TigerTag+ and the last answer is
// not already about its product. False when nothing was started: not a
// TigerTag+, answer already cached, no network, or another fetch running.
bool request(const TagInfo& t);

// A fetch is running. main.cpp opens no new printer link meanwhile.
bool busy();

// A fetch is running and the heap had no piece big enough for its handshake
// when it started - the only case in which background links are asked to
// step aside for it.
bool needsRoom();

// A fetch for THIS product is running and still inside its budget - the one
// condition under which a send should wait.
bool waiting(uint32_t idProduct);

// The last answer, if it was about this product.
bool cached(uint32_t idProduct, filament::ProductData& out);

// Chip, cached answer and material table, through the resolver. No network.
filament::ResolvedFilament resolveFor(const TagInfo& t);

}  // namespace product_api
