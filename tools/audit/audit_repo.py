#!/usr/bin/env python3
import os, re

ROOT = "."
handles_defs, handles_externs, spi_modes, cs_toggles, flash_cmds = [], [], [], [], []
ioc_files = []

def walk_files():
    for dirpath, _, filenames in os.walk(ROOT):
        if ".git" in dirpath or "audit" in dirpath: continue
        for fn in filenames:
            yield os.path.join(dirpath, fn)

def analyze(path):
    try:
        text = open(path, encoding="utf-8", errors="ignore").read()
    except: return
    if path.endswith(".ioc"): ioc_files.append(path)
    handles_defs += [(path,m.group(2)) for m in re.finditer(r'(SPI|UART|PCD)_HandleTypeDef\s+(h\w+);', text)]
    handles_externs += [(path,m.group(2)) for m in re.finditer(r'extern\s+(SPI|UART|PCD)_HandleTypeDef\s+(h\w+);', text)]
    spi_modes += [(path,m.group(1),m.group(2)) for m in re.finditer(r'hspi\d+\.Init\.(CLKPolarity|CLKPhase|NSS)\s*=\s*(\w+);', text)]
    cs_toggles += [(path,m.group(2),m.group(3)) for m in re.finditer(r'HAL_GPIO_WritePin\([^,]+,\s*([^,]+),\s*(GPIO_PIN_(?:RESET|SET))\)', text)]
    flash_cmds += [(path,m.group(0)) for m in re.finditer(r'0x06|0x05|0x02|0x20|0xD8|WREN|PP|READ|ERASE', text)]

for f in walk_files():
    if f.endswith((".c",".h",".ioc")): analyze(f)

print("# STM32 Project Audit Report\n")
print("## HAL handle definitions:", handles_defs)
print("## extern declarations:", handles_externs)
print("## SPI mode lines:", spi_modes)
print("## CS toggles:", cs_toggles)
print("## Flash commands:", flash_cmds)
print("## IOC files:", ioc_files)
