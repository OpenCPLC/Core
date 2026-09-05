#!/bin/bash
# Flash CPU2 on STM32WB55 (1M) → BLE Stack full v1.24.0.3 @ 0x080D0000 (SFSA 0xD0).
# An already-installed stack is left alone: only `-startwirelessstack` runs, which also
# heals a board left latched in FUS by CubeProgrammer.
# --fus: one-time factory provisioning, 0.5.3 bridge → 2.2.0 (IRREVERSIBLE);
#   both steps no-op with an error when the FUS is already current.
# RECOVERY: a failed install leaves CPU2 with no stack, not a brick, re-run the script.
# COST: `-startwirelessstack` has CubeProgrammer download its FUS operator to 0x08000000,
#   which erases the first pages of user flash, so CPU1 loses the application every run.
# Run it through `make stack`, or `make stack FUS=1` for the one-time provisioning.

set -euo pipefail

CLI="STM32_Programmer_CLI"
SN=""    # --sn=<serial> picks the probe when several are attached
FUS=0    # --fus runs the one-time factory provisioning
for arg in "$@"; do
  case "$arg" in
    --fus) FUS=1 ;;
    --sn=*) SN="${arg#--sn=}" ;;
    *) echo "ERROR: unknown argument: $arg" >&2; exit 1 ;;
  esac
done
SWD="-c port=swd mode=UR${SN:+ sn=$SN}"

REFLASH="NOTE: FUS operator overwrote user flash, put the application back with 'make run'"

die() { echo "ERROR: $*" >&2; exit 1; }
sfsa() {
  $CLI $SWD -ob displ | sed -nE 's/.*SFSA[^0-9A-Fx]*0x([0-9A-Fa-f]+).*/\L\1/Ip' | head -n1
}

cd "$(dirname "$0")"
command -v "$CLI" >/dev/null || die "$CLI not on PATH"

S=$(sfsa) || true
[[ -n "$S" ]] || die "cannot read option bytes (no probe or no target?)"
if [[ "$S" == "d0" ]]; then
  echo "Stack already installed (SFSA=0xD0), starting it"
  $CLI $SWD -startwirelessstack || die "startwirelessstack failed"
  echo "$REFLASH"
  exit 0
fi

# Anything below needs the FUS running (retry helps right after the M4→FUS transition)
$CLI $SWD -startfus || true; sleep 3
$CLI $SWD -startfus || true; sleep 2

if [[ "$FUS" == 1 ]]; then
  $CLI $SWD -fwupgrade stm32wb5x_FUS_0.5.3.bin 0x080EC000 firstinstall=0 || true
  sleep 2
  $CLI $SWD -fwupgrade stm32wb5x_FUS_2.2.0.bin 0x080EE000 firstinstall=0 || true
  sleep 2
fi

$CLI $SWD -fwdelete || die "stack delete failed"
sleep 2
$CLI $SWD -fwupgrade stm32wb5x_BLE_1.24.0.3.bin 0x080D0000 firstinstall=1 \
  || die "stack install failed → CPU2 has no stack, re-run this script"
sleep 2
$CLI $SWD -startwirelessstack || die "startwirelessstack failed"
sleep 3

[[ "$(sfsa)" == "d0" ]] || die "SFSA mismatch, install did not take"
echo "OK: BLE Stack full v1.24.0.3 installed, power-cycle the board (cold reboot)"
echo "$REFLASH"
