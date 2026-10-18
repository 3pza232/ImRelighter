#!/usr/bin/env python3
"""Fail when an i18n key is defined but never referenced from src/."""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def load_keys(path: Path) -> set[str]:
    return set(json.loads(path.read_text(encoding="utf-8")))


def source_text() -> str:
    parts: list[str] = []
    for path in (ROOT / "src").rglob("*"):
        if path.suffix in {".cpp", ".h"}:
            parts.append(path.read_text(encoding="utf-8"))
    return "\n".join(parts)


def main() -> int:
    locales = sorted((ROOT / "assets" / "i18n").glob("*.json"))
    if not locales:
        print("No locale files found", file=sys.stderr)
        return 1

    source = source_text()
    failed = False
    for locale in locales:
        unused = sorted(
            key for key in load_keys(locale) if f'"{key}"' not in source
        )
        if unused:
            failed = True
            print(f"{locale.name} has unused keys:", file=sys.stderr)
            for key in unused:
                print(f"  {key}", file=sys.stderr)

    if failed:
        return 1
    print("all i18n keys are referenced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
