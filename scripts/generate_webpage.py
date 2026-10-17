Import("env")

import gzip
import re
from pathlib import Path


project_dir = Path(env.subst("$PROJECT_DIR"))
source_path = project_dir / "include" / "WebPage.h"
target_path = project_dir / "include" / "WebPageGz.h"
source = source_path.read_bytes()
match = re.search(rb'R"HTML\((.*)\)HTML";', source, re.DOTALL)
if not match:
    raise RuntimeError("HTML-Inhalt fehlt in include/WebPage.h")

compressed = gzip.compress(match.group(1), compresslevel=9, mtime=0)
rows = []
for offset in range(0, len(compressed), 16):
    values = ", ".join(f"0x{value:02x}" for value in compressed[offset:offset + 16])
    rows.append(f"  {values},")

generated = (
    "#pragma once\n\n"
    "#include <Arduino.h>\n\n"
    "// Automatisch aus WebPage.h erzeugt. WebPage.h bearbeiten, nicht diese Datei.\n"
    f"constexpr size_t WEB_PAGE_GZ_LEN = {len(compressed)};\n"
    "const uint8_t WEB_PAGE_GZ[] PROGMEM = {\n"
    + "\n".join(rows)
    + "\n};\n"
)

if not target_path.exists() or target_path.read_text(encoding="utf-8") != generated:
    target_path.write_text(generated, encoding="utf-8", newline="\n")
    print(f"Komprimierte Weboberfläche erzeugt: {target_path} ({len(compressed)} Bytes)")
