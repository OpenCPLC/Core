# .tests/data/forge.py

"""
Key and signature Forge makes for a test image, written into `forge.h` beside this file.

The image is the one `image_make` lays out for `SIZE` and `IMAGE_SEED`,
so the bootloader built for the host checks what Forge signs, byte for byte.

  py opencplc/develop/.tests/data/forge.py
"""

import struct
from xaeian import FILE, PATH
from opencplc.utils import ed25519

# `BOOT_CHIP` and `BOOT_PAGES` of the tests: a G081 with 4 bootloader pages of 2kB
APP_ADDR = 0x08000000 + 4 * 2048
CHIP = 0x460
EPOCH = 0 # `PRO_BOOT_EPOCH` of the tests
MAGIC = 0x4E45504F # "OPEN"
HEADER_OFFSET = 0x200
SIZE = 9000
IMAGE_SEED = 7
KEY_SEED = bytes(range(32)) # a test key, public by design

def image() -> bytes:
  """Bytes `image_make` lays out: a counting pattern with the header at `HEADER_OFFSET`."""
  data = bytearray((i * 7 + IMAGE_SEED) & 0xFF for i in range(SIZE))
  header = struct.pack("<5I", MAGIC, SIZE, APP_ADDR, CHIP, EPOCH)
  data[HEADER_OFFSET:HEADER_OFFSET + len(header)] = header
  return bytes(data)

def c_array(name:str, data:bytes) -> str:
  """`static const uint8_t` array, twelve bytes a row."""
  rows = [", ".join(f"0x{b:02X}" for b in data[i:i + 12]) for i in range(0, len(data), 12)]
  return f"static const uint8_t {name}[{len(data)}] = {{\n  " + ",\n  ".join(rows) + ",\n};\n"

def main() -> None:
  """Rewrite `forge.h` from `SIZE`, `IMAGE_SEED` and `KEY_SEED`."""
  key = ed25519.public_key(KEY_SEED)
  signature = ed25519.sign(KEY_SEED, image())
  parts = [
    "// .tests/data/forge.h",
    "// Written by `forge.py`: a test key and the signature Forge makes over a test image",
    "",
    "#ifndef FORGE_H_",
    "#define FORGE_H_",
    "",
    "#include <stdint.h>",
    "",
    f"#define FORGE_SIZE {SIZE}u",
    f"#define FORGE_IMAGE_SEED {IMAGE_SEED}",
    "",
    c_array("FORGE_KEY_SEED", KEY_SEED),
    c_array("FORGE_KEY", key),
    c_array("FORGE_SIGNATURE", signature),
    "#endif",
  ]
  text = "\n".join(parts) + "\n"
  # Core keeps CRLF
  FILE.save(f"{PATH.dirname(PATH.resolve(__file__))}/forge.h", text.replace("\n", "\r\n"))

if __name__ == "__main__":
  main()
