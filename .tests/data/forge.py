# .tests/data/forge.py

"""
Keys, signature and encryption Forge makes for a test image.

`forge.h` beside this file takes them.
The image is the one `image_make` lays out for `SIZE` and `IMAGE_SEED`,
so the bootloader built for the host checks what Forge signs and encrypts, byte for byte.

  py opencplc/develop/.tests/data/forge.py
"""

import struct
from xaeian import FILE, PATH
from xaeian.crc import crc32_iso
from opencplc.cipher import encrypt
from opencplc.utils import ed25519, keys

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
CIPHER_KEY = bytes(range(0x80, 0xA0)) # cipher key of the tests, public by design too

def image() -> bytes:
  """Bytes `image_make` lays out: a counting pattern with the header at `HEADER_OFFSET`."""
  data = bytearray((i * 7 + IMAGE_SEED) & 0xFF for i in range(SIZE))
  header = struct.pack("<3I4HI", MAGIC, SIZE, APP_ADDR, CHIP, BOARD, EPOCH, KEY, 0)
  data[HEADER_OFFSET:HEADER_OFFSET + len(header)] = header
  return bytes(data)

def update_image(signature:bytes) -> bytes:
  """Test image as `make dist` leaves it: its bytes, then the CRC32, a gap and the signature."""
  body = image()
  return body + crc32_iso.checksum(body).to_bytes(4, "little") + b"\xFF" * 4 + signature

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
  """Rewrite `forge.h` from `SIZE`, `IMAGE_SEED`, `KEY_SEEDS` and `CIPHER_KEY`."""
  publics = [ed25519.public_key(seed) for seed in KEY_SEEDS]
  signature = ed25519.sign(KEY_SEEDS[KEY], image())
  encrypted = encrypt(update_image(signature), CIPHER_KEY)[:SIZE]
  parts = [
    "// .tests/data/forge.h",
    "",
    "// Written by `forge.py`: test keys, signature and encryption Forge makes over a test image",
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
    c_keys("FORGE_KEYS", publics),
    "// The development key of the tests is the signing key, for the bootloader of builds",
    c_keys("FORGE_DEV_KEY", [publics[KEY]]),
    c_array("FORGE_KEY", publics[KEY]),
    c_array("FORGE_SIGNATURE", signature),
    "// Test cipher key, its fingerprint and the CRC32 of the image Forge encrypts with it",
    c_array("FORGE_CIPHER_KEY", CIPHER_KEY),
    c_array("FORGE_CIPHER_ID", bytes.fromhex(keys.cipher_id(CIPHER_KEY))),
    f"#define FORGE_CIPHER_CRC 0x{crc32_iso.checksum(encrypted):08X}u",
    "",
    "#endif",
  ]
  text = "\n".join(parts) + "\n"
  # Core keeps CRLF
  FILE.save(f"{PATH.dirname(PATH.resolve(__file__))}/forge.h", text.replace("\n", "\r\n"))

if __name__ == "__main__":
  main()
