#!/bin/bash
# Flash CPU2 on STM32WB55 (1M) → BLE Stack full v1.24.0.3 @ 0x080D0000 (SFSA 0xD0).
# Delete and install run every time, so the CPU2 carries the image next to this script.
# The FUS writes SFSA, SBRSA and SNBRSA as part of the install,
# and the stack allocates its links and attributes from SRAM2 within those boundaries.
# A board carrying them from an earlier image reads as provisioned and runs something else,
# which is what an install skipped on the strength of one option byte leaves behind.
# --fast: start what is installed when SFSA already reads 0xD0, no install.
#   It saves a couple of minutes and proves nothing, and falls through to the install
#   when the option byte reads anything else.
# --fus: one-time factory provisioning, 0.5.3 bridge → 2.2.0 (IRREVERSIBLE);
#   both steps no-op with an error when the FUS is already current.
# RECOVERY: a failed install leaves CPU2 with no stack, not a brick, re-run the script.
# COST: `-startwirelessstack` has CubeProgrammer download its FUS operator to 0x08000000,
#   which erases the first pages of user flash, so CPU1 loses the application every run.
# Run it through `make stack`, with `FUS=1` for the one-time provisioning or `FAST=1`
# for the shortcut.

set -euo pipefail

CLI="STM32_Programmer_CLI"
STACK="stm32wb5x_BLE_1.24.0.3.bin" # image and the address the FUS writes it to
STACK_ADDR="0x080D0000"
STACK_SFSA="d0" # option byte the install leaves behind
SN=""   # --sn=<serial> picks the probe when several are attached
FUS=0   # --fus runs the one-time factory provisioning
FAST=0  # --fast trusts an SFSA that already reads the installed value
for arg in "$@"; do
  case "$arg" in
    --fus) FUS=1 ;;
    --fast) FAST=1 ;;
    --sn=*) SN="${arg#--sn=}" ;;
    *) echo "ERROR: unknown argument: $arg" >&2; exit 1 ;;
  esac
done
SWD="-c port=swd mode=UR${SN:+ sn=$SN}"

REFLASH="NOTE: FUS operator overwrote user flash, put the application back with 'make run'"

die() { echo "ERROR: $*" >&2; exit 1; }
warn() { echo "WARNING: $*" >&2; }

# One dump per reading, parsed from the copy:
# the probe is touched once and every byte comes from the same moment
OB=""
ob_read() { OB=$($CLI $SWD -ob displ) || return 1; }
ob() { printf '%s\n' "$OB" | sed -nE "s/.*$1[^0-9A-Fx]*0x([0-9A-Fa-f]+).*/\L\1/Ip;T;q"; }
# A byte the dump does not carry prints as `??` rather than an empty `0x`
ob_show() { local v; v=$(ob "$1"); printf '0x%s' "${v:-??}"; }

cd "$(dirname "$0")"
command -v "$CLI" >/dev/null || die "$CLI not on PATH"
[[ -f "$STACK" ]] || die "$STACK missing next to the script"

ob_read || die "cannot read option bytes (no probe or no target?)"
[[ -n "$(ob SFSA)" ]] || die "option byte dump carries no SFSA"

if [[ "$FAST" == 1 ]]; then
  if [[ "$(ob SFSA)" == "$STACK_SFSA" ]]; then
    echo "SFSA=$(ob_show SFSA) and --fast given, starting whatever is installed"
    $CLI $SWD -startwirelessstack || die "startwirelessstack failed"
    echo "$REFLASH"
    exit 0
  fi
  echo "--fast given but SFSA reads $(ob_show SFSA), installing instead"
fi

# Anything below needs the FUS running.
# The first attempt often lands mid transition from the M4, so a second one follows.
fus=0
$CLI $SWD -startfus && fus=1 || true
sleep 3
if [[ "$fus" == 0 ]]; then
  $CLI $SWD -startfus && fus=1 || true
  sleep 2
fi
[[ "$fus" == 1 ]] || warn "FUS never reported ready, the steps below will likely refuse"

if [[ "$FUS" == 1 ]]; then
  $CLI $SWD -fwupgrade stm32wb5x_FUS_0.5.3.bin 0x080EC000 firstinstall=0 || true
  sleep 2
  $CLI $SWD -fwupgrade stm32wb5x_FUS_2.2.0.bin 0x080EE000 firstinstall=0 || true
  sleep 2
fi

# A slot holding no stack has nothing to delete, and that is exactly the state a failed
# install leaves behind. Treating the refusal as fatal would block the retry.
$CLI $SWD -fwdelete || warn "delete refused, going on to the install"
sleep 2
$CLI $SWD -fwupgrade "$STACK" "$STACK_ADDR" firstinstall=1 \
  || die "stack install failed → CPU2 has no stack, re-run this script"
sleep 2
# The install is done and survives a power cycle; only the running image is at stake here
$CLI $SWD -startwirelessstack || die "$STACK is installed but did not start, re-run to start it"
sleep 3

ob_read || die "cannot read option bytes back"
[[ "$(ob SFSA)" == "$STACK_SFSA" ]] || die "SFSA reads $(ob_show SFSA) after the install"
echo "OK: $STACK installed"
# The FUS writes the three together, so they name the install that ran
echo "    SFSA=$(ob_show SFSA) SBRSA=$(ob_show SBRSA) SNBRSA=$(ob_show SNBRSA)"
echo "    power-cycle the board, the boot log names the image the CPU2 reports"
echo "$REFLASH"
