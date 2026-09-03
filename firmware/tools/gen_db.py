# Gera include/tigertag_db.h a partir de data_src/id_material.json e id_brand.json
# (tabelas de referencia TigerTag). Uso:
#   ~/.platformio/python3/python.exe gen_db.py
import json, os

here = os.path.dirname(os.path.abspath(__file__))
src  = os.path.join(here, "data_src")
out  = os.path.join(here, "include", "tigertag_db.h")

with open(os.path.join(src, "id_material.json"), encoding="utf-8") as f:
    materials = json.load(f)
with open(os.path.join(src, "id_brand.json"), encoding="utf-8") as f:
    brands = json.load(f)

mats = sorted(((int(m["id"]), str(m["label"])) for m in materials), key=lambda x: x[0])
brs  = sorted(((int(b["id"]), str(b["name"]))  for b in brands),    key=lambda x: x[0])

def esc(s): return s.replace('\\', '\\\\').replace('"', '\\"')

with open(out, "w", encoding="utf-8") as f:
    f.write("// GERADO por gen_db.py - NAO editar a mao.\n")
    f.write("// Tabelas de referencia TigerTag (id -> label), ordenadas por id.\n")
    f.write("#pragma once\n#include <Arduino.h>\n\n")
    f.write("struct TTEntry { uint16_t id; const char* label; };\n\n")

    f.write(f"static const TTEntry TT_MATERIALS[] = {{\n")
    for i, lab in mats:
        f.write(f'  {{ {i}, "{esc(lab)}" }},\n')
    f.write("};\n")
    f.write(f"static const size_t TT_MATERIALS_N = {len(mats)};\n\n")

    f.write(f"static const TTEntry TT_BRANDS[] = {{\n")
    for i, name in brs:
        f.write(f'  {{ {i}, "{esc(name)}" }},\n')
    f.write("};\n")
    f.write(f"static const size_t TT_BRANDS_N = {len(brs)};\n\n")

    f.write("""static inline const char* tt_lookup(const TTEntry* t, size_t n, uint16_t id) {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (t[mid].id == id) return t[mid].label;
    if (t[mid].id < id) lo = mid + 1; else hi = mid;
  }
  return nullptr;
}
static inline const char* tt_material(uint16_t id) { return tt_lookup(TT_MATERIALS, TT_MATERIALS_N, id); }
static inline const char* tt_brand(uint16_t id)    { return tt_lookup(TT_BRANDS,    TT_BRANDS_N,    id); }
""")

print(f"OK -> {out}   ({len(mats)} materiais, {len(brs)} marcas)")
