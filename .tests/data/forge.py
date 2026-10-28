# .tests/data/forge.py

"""
Keys and signature Forge makes for a test image, written into `forge.h` beside this file.

The image is the one `image_make` lays out for `SIZE` and `IMAGE_SEED`,
so the bootloader built for the host checks what Forge signs, byte for byte.

  py opencplc/develop/.tests/data/forge.py
"""

import struct
from xaeian import FILE, PATH
from opencplc.utils import ed25519

# `BOOT_CHIP` and `BOOT_PAGES` of the tests: a G081 with 6 bootloader pages of 2kB
APP_ADDR = 0x08000000 + 6 * 2048
CHIP = 0x460
BOARD = 1 # `BOOT_BOARD_VARIANT` of the tests
EPOCH = 0 # `PRO_SECURITY_EPOCH` of the tests
KEY = 0 # `PRO_SIGNING_KEY` of the tests
MAGIC = 0x4E45504F # "OPEN"
HEADER_OFFSET = 0x200
SIZE = 9000
IMAGE_SEED = 7
# Three test keys, public by design; key `KEY` signs the test image
KEY_SEEDS = [bytes(range(32)), bytes([2]) + bytes(31), bytes([3]) + bytes(31)]

def image() -> bytes:
  """Bytes `image_make` lays out: a counting pattern with the header at `HEADER_OFFSET`."""
  data = bytearray((i * 7 + IMAGE_SEED) & 0xFF for i in range(SIZE))
  header = struct.pack("<3I4HI", MAGIC, SIZE, APP_ADDR, CHIP, BOARD, EPOCH, KEY, 0)
  data[HEADER_OFFSET:HEADER_OFFSET + len(header)] = header
  return bytes(data)

def c_rows(data:bytes) -> list[str]:
  """Bytes as C literals, twelve a row."""
  return [", ".join(f"0x{b:02X}" for b in data[i:i + 12]) for i in range(0, len(data), 12)]

def c_array(name:str, data:bytes) -> str:
  """`static const uint8_t` array."""
  rows = ",\n  ".join(c_rows(data))
  return f"static const uint8_t {name}[{len(data)}] = {{\n  {rows},\n}};\n"

def c_seeds(name:str, seeds:list[bytes]) -> str:
  """`static const uint8_t` array of 32-byte seeds, a block of braces each."""
  blocks = ["{\n    " + ",\n    ".join(c_rows(seed)) + ",\n  }" for seed in seeds]
  body = ",\n  ".join(blocks)
  return f"static const uint8_t {name}[{len(seeds)}][32] = {{\n  {body},\n}};\n"

def c_keys(name:str, keys:list[bytes]) -> str:
  """Public keys the way `BOOT_KEYS` takes them: one string, 64 hex digits a key."""
  lines = [f'  "{key.hex()}"' for key in keys]
  return f"#define {name} \\\n" + " \\\n".join(lines) + "\n"

def main() -> None:
  """Rewrite `forge.h` from `SIZE`, `IMAGE_SEED` and `KEY_SEEDS`."""
  keys = [ed25519.public_key(seed) for seed in KEY_SEEDS]
  signature = ed25519.sign(KEY_SEEDS[KEY], image())
  parts = [
    "// .tests/data/forge.h",
    "",
    "// Written by `forge.py`: test keys and the signature Forge makes over a test image",
    "",
    "#ifndef FORGE_H_",
    "#define FORGE_H_",
    "",
    "#include <stdint.h>",
    "",
    f"#define FORGE_SIZE {SIZE}u",
    f"#define FORGE_IMAGE_SEED {IMAGE_SEED}",
    "",
    c_seeds("FORGE_KEY_SEEDS", KEY_SEEDS),
    c_keys("FORGE_KEYS", keys),
    "// The development key of the tests is the signing key, for the bootloader of builds",
    c_keys("FORGE_DEV_KEY", [keys[KEY]]),
    c_array("FORGE_KEY", keys[KEY]),
    c_array("FORGE_SIGNATURE", signature),
    "#endif",
  ]
  text = "\n".join(parts) + "\n"
  # Core keeps CRLF
  FILE.save(f"{PATH.dirname(PATH.resolve(__file__))}/forge.h", text.replace("\n", "\r\n"))

if __name__ == "__main__":
  main()
