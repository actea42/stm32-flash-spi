#!/usr/bin/env python3
# tools/audit/audit_repo.py
# STM32 + SPI Flash audit:
# - HAL handle definitions vs externs
# - SPI init mode/NSS lines
# - CS/WP/HOLD GPIO usage
# - Flash command tokens (WREN/RDSR/PP/READ/ERASE)
# - .ioc SPI lines (mode/NSS/baud)
#
# Outputs a Markdown report to stdout. No external packages required.

import os
import re
from typing import List, Tuple

ROOT = "."

# Regex patterns
RE_HANDLE_DEF     = re.compile(r'^\s*(SPI|UART|PCD)_HandleTypeDef\s+(h\w+)\s*;', re.M)
RE_HANDLE_EXTERN  = re.compile(r'^\s*extern\s+(SPI|UART|PCD)_HandleTypeDef\s+(h\w+)\s*;', re.M)
RE_SPI_INIT_FUNC  = re.compile(r'void\s+MX_SPI(\d+)_Init\s*\(')
RE_SPI_MODE_LINE  = re.compile(r'hspi\d+\.Init\.(CLKPolarity|CLKPhase|NSS)\s*=\s*(\w+);')
RE_GPIO_WRITE     = re.compile(r'HAL_GPIO_WritePin\(([^,]+),\s*([^,]+),\s*(GPIO_PIN_(?:RESET|SET))\)')
RE_FLASH_CMD      = re.compile(r'(0x06|0x05|0x02|0x03|0x20|0xD8|WREN|WRDI|RDSR|PP|READ|SECTOR_ERASE|BLOCK_ERASE)', re.I)

def list_source_files() -> List[str]:
    files = []
    for dirpath, _, filenames in os.walk(ROOT):
        if ".git" in dirpath or "audit" in dirpath:
            continue
        for fn in filenames:
            if fn.endswith((".c", ".h", ".ioc", ".s", ".ld", ".yaml", ".yml", ".md", ".txt")):
                files.append(os.path.join(dirpath, fn))
    return files

def parse_ioc_spi_lines(path: str) -> List[str]:
    out = []
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                if "SPI" in line and ("NSS" in line or "CLKPolarity" in line or "CLKPhase" in line or "BaudRatePrescaler" in line):
                    out.append(line.strip())
    except Exception:
        pass
    return out

def analyze_file(path: str):
    """Return all findings for a single file as tuples/lists."""
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            text = f.read()
    except Exception:
        return ([], [], [], [], [], [], [])

    ioc = [path] if path.endswith(".ioc") else []

    defs = [(path, m.group(1), m.group(2)) for m in RE_HANDLE_DEF.finditer(text)]
    exts = [(path, m.group(1), m.group(2)) for m in RE_HANDLE_EXTERN.finditer(text)]
    spi_funcs = [(path, m.group(1)) for m in RE_SPI_INIT_FUNC.finditer(text)]
    spi_modes = [(path, m.group(1), m.group(2)) for m in RE_SPI_MODE_LINE.finditer(text)]
    gpio_toggles = [(path, m.group(1).strip(), m.group(2).strip(), m.group(3)) for m in RE_GPIO_WRITE.finditer(text)]
    flash_tokens = [(path, m.group(1)) for m in RE_FLASH_CMD.finditer(text)]

    return (ioc, defs, exts, spi_funcs, spi_modes, gpio_toggles, flash_tokens)

def main():
    ioc_files: List[str] = []
    handle_defs: List[Tuple[str, str, str]] = []
    handle_exts: List[Tuple[str, str, str]] = []
    spi_init_funcs: List[Tuple[str, str]] = []
    spi_mode_lines: List[Tuple[str, str, str]] = []
    cs_toggles: List[Tuple[str, str, str, str]] = []
    flash_cmds: List[Tuple[str, str]] = []

    for path in list_source_files():
        ioc, defs, exts, spif, spim, gpio, fltoks = analyze_file(path)
        ioc_files.extend(ioc)
        handle_defs.extend(defs)
        handle_exts.extend(exts)
        spi_init_funcs.extend(spif)
        spi_mode_lines.extend(spim)
        cs_toggles.extend(gpio)
        flash_cmds.extend(fltoks)

    # ---- Output Markdown report ----
    print("# STM32 Project Audit Report\n")

    # 1) HAL handle definitions vs extern
    print("## 1) HAL handle definitions (exactly ONE owner per handle expected)\n")
    if not handle_defs:
        print("- No HAL handle definitions found.\n")
    else:
        by_handle = {}
        for p, typ, h in handle_defs:
            by_handle.setdefault(h, []).append((p, typ))
        for h, items in by_handle.items():
            print(f"- `{h}` defined in {len(items)} file(s):")
            for p, typ in items:
                print(f"  - {p} ({typ})")
        print()

    print("## 2) Extern declarations\n")
    if not handle_exts:
        print("- No extern declarations found.\n")
    else:
        by_handle_e = {}
        for p, typ, h in handle_exts:
            by_handle_e.setdefault(h, []).append((p, typ))
        for h, items in by_handle_e.items():
            print(f"- `{h}` extern in {len(items)} file(s):")
            for p, typ in items:
                print(f"  - {p} ({typ})")
        print()

    # 2) SPI init & mode
    print("## 3) SPI init functions & mode settings\n")
    if spi_init_funcs:
        for p, idx in spi_init_funcs:
            print(f"- Found: {p} (MX_SPI{idx}_Init)")
    else:
        print("- No MX_SPIx_Init() functions found.")
    if spi_mode_lines:
        for p, field, value in spi_mode_lines:
            print(f"  {p}: {field} = {value}")
    else:
        print("- No explicit CLKPolarity/CLKPhase/NSS lines found.")
    print()

    # 3) CS / WP / HOLD toggles
    print("## 4) Chip-select (CS) & WP/HOLD toggles (GPIO_WritePin)\n")
    if cs_toggles:
        for p, pin, state_pin, state_val in cs_toggles[:200]:
            print(f"- {p}: HAL_GPIO_WritePin(<GPIO>, {pin}, {state_val})")
        if len(cs_toggles) > 200:
            print(f"... ({len(cs_toggles)-200} more)")
    else:
        print("- None detected.")
    print()

    # 4) Flash command tokens
    print("## 5) External flash command tokens found (heuristic)\n")
    if flash_cmds:
        per_file = {}
        for p, tok in flash_cmds:
            per_file.setdefault(p, set()).add(tok)
        for p, toks in per_file.items():
            print(f"- {p}: {', '.join(sorted(toks))}")
    else:
        print("- None detected.")
    print()

    # 5) .ioc SPI lines
    print("## 6) .ioc SPI lines (mode/NSS/baud)\n")
    if ioc_files:
        for ioc in ioc_files:
            print(f"- {ioc}")
            for line in parse_ioc_spi_lines(ioc):
                print(f"  {line}")
    else:
        print("- No .ioc files found.")
    print()

    # 6) Recommendations (concise)
    print("## 7) Recommendations\n")
    print("- Define each HAL handle exactly once (typically in `Core/Src/main.c`); use `extern` everywhere else.")
    print("- Prefer **NSS = Software** and toggle CS via a single GPIO helper for external flash.")
    print("- Verify SPI mode (CPOL/CPHA) matches the flash datasheet; Winbond W25Q typically supports Mode 0 or 3.")
    print("- Ensure WP and HOLD are high (inactive) during program/erase.")
    print("- Follow flash sequence: WREN → operation (PP/Erase) → poll SR1.WIP; erase sector before programming.")

if __name__ == "__main__":
    main()
