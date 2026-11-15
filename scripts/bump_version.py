#!/usr/bin/env python3
"""Actualiza las macros de version de src/app.rc.

Lo usa el workflow XPE Bot Auto Release para que la version del recurso
VERSIONINFO (la que lee el auto-update del cliente) coincida con el tag.

Uso:
    python3 scripts/bump_version.py --version 2.1.0 --path src/app.rc
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

MACROS = ("VER_FILEVERSION", "VER_FILEVERSION_STR", "VER_PRODUCTVERSION", "VER_PRODUCTVERSION_STR")


def normalize(version: str) -> str:
    text = version.strip()
    if text[:1] in ("v", "V"):
        text = text[1:]
    parts = [part for part in text.split(".") if part != ""]
    if not parts or any(not part.isdigit() for part in parts):
        raise ValueError(f"version invalida: {version!r}")
    while len(parts) < 3:
        parts.append("0")
    return ".".join(parts[:3])


def patch_text(text: str, version: str) -> str:
    clean = normalize(version)
    parts = clean.split(".")
    numeric = ",".join(parts + ["0"])
    string_value = f'"{clean}.0\\0"'

    values = {
        "VER_FILEVERSION": numeric,
        "VER_FILEVERSION_STR": string_value,
        "VER_PRODUCTVERSION": numeric,
        "VER_PRODUCTVERSION_STR": string_value,
    }

    updated = text
    for macro in MACROS:
        pattern = re.compile(rf"^#define\s+{macro}\s+.*$", re.MULTILINE)
        if not pattern.search(updated):
            raise ValueError(f"no se encontro la macro {macro} en el archivo")
        # Reemplazo por funcion: evita que re interprete la barra invertida de \0.
        replacement = f"#define {macro.ljust(28)}{values[macro]}"
        updated = pattern.sub(lambda _match, value=replacement: value, updated)
    return updated


def main() -> int:
    parser = argparse.ArgumentParser(description="Actualiza la version de src/app.rc")
    parser.add_argument("--version", required=True, help="Version nueva, ej. 2.1.0")
    parser.add_argument("--path", default="src/app.rc", help="Ruta del app.rc")
    args = parser.parse_args()

    path = pathlib.Path(args.path)
    try:
        original = path.read_text(encoding="utf-8")
        updated = patch_text(original, args.version)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    path.write_text(updated, encoding="utf-8", newline="")
    print(f"{path} actualizado a {normalize(args.version)}")
    for line in updated.splitlines()[:8]:
        print("  " + line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
