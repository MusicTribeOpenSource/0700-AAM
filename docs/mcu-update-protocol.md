# MCU update protocol

This is the update path as I implemented it in `Firmware/kick-mcu` for issue #5, and the host behaviour it has to live with. Nothing here has run on a real KICK. The MCU part is not public, so the flash map is a placeholder and every claim below is about the simulator and the host harness.

## What is sent, and by whom

The host updater is `Kick/modules/hardware/source/kick_FirmwareUpdate.cpp` (`MCUUpdate`), and its receive side is `SWUpdateScreen::handleCommand` in `kick_ui/swupdate/kick_SWUpdateScreen.cpp`. Every frame is SYSTEM, cmd 3 (`EN_SYS_SW_UPDATE`), and the first payload byte is the subcommand.

```
host -> [0]                                 INITIATE
MCU  -> [1]                                 BEGIN_TRANSFER, once the idle slot is erased
host -> [3, ulbaHi, ulbaLo]                 DATABASE, an Intel HEX type 04 record
MCU  -> [5]                                 ACK
host -> [2, addrHi, addrLo, d0..dn-1, x]    DATA, one type 00 record, x is a stale byte
MCU  -> [5]                                 ACK
 ...
host -> [4]                                 END
MCU  -> [5] if the image verified and the trial is armed, [6] if not
MCU  -> [6]                                 FAILURE, at any point; the transfer is over
```

Host behaviour I did not choose and worked around:

- Any MCU reply that is not `[1]` or `[6]` counts as an ACK. The MCU sends `[5]`.
- The host sends one byte more than the record: for an n-byte record it sends `binaryData[0..n+2]`, so the last byte is left over from an earlier record. The MCU drops it, so a record's data length is payload length minus 4.
- Records are at most 29 bytes, because `binaryData` is 32 bytes and holds the two address bytes. The host also stops understanding the file at any record type other than 00, 01 and 04.
- The host has no END acknowledgement and no timeout. It sends END and returns success without reading the MCU's answer, and it waits for an ACK for ever.
- The host never sends IDLE (7); it is internal to its state machine.

## Flash map

```
bootloader | slot A | slot B | boot-control copy 0 | boot-control copy 1
```

The application never writes the bootloader. Only the slot that is not running is erased or written. The numbers are placeholders (`makeLayout()` in `flash_layout.h`; the simulator uses 1 KiB blocks, a 4 KiB bootloader and 32 KiB slots), and only that struct changes when the part is known.

## Image format

A 64-byte little-endian header, then the payload. `tools/mcu-image-header.py` stamps it onto a hex or a binary:

```
python3 tools/mcu-image-header.py stamp --hex app.hex -o app-stamped.hex --version 1.4.2 --revision 7
python3 tools/mcu-image-header.py verify app-stamped.hex
python3 tools/mcu-image-header.py info app-stamped.hex
```

The header holds the `FLASH_VALID` marker (0x12345678, the host's `typeSoftwareInfo` value), the image length, a CRC-32 of the payload, the version, the revision and a CRC-32 of the header. The tool emits only record types 04, 00 and 01, writes the upper address first, and refuses records over 29 bytes. The byte-level layout is in `image.h`.

Record addresses are offsets from the start of the image, not flash addresses. The MCU adds the base of the idle slot. The header is at offset 0, so the host sends it as ordinary data.

## Boot control

Two 16-byte records, each with a sequence number and a CRC-32, in two erase blocks. A write goes to the copy that does not hold the newest valid record, so a power cut at any byte leaves the old record or the new one. Sequence numbers compare with wraparound.

The state machine:

- `Confirmed`: `good` runs.
- `TrialPending`: END verified an image into `trial`; the next boot should try it once.
- `TrialBooted`: the bootloader started `trial`. It writes this before it jumps, so a trial that never confirms is reverted by the next reset.

The application confirms a trial after the host handshake completes (`COMM_INIT [2]` then `CHECK_COMM_STATUS [2]`). That is the only evidence the MCU has that the new image can talk to the host. Until then a reset returns to the old slot.

If the recorded good slot no longer verifies, the bootloader falls back to the other slot if that verifies, and reports `fellBack`. If nothing verifies it reports not bootable and stays in recovery. It never guesses.

## Update sequence on the MCU

1. INITIATE: refuse unless the running image is the confirmed one (otherwise there is no safe fallback), then erase the idle slot, one block per `poll()`. Reply BEGIN when done.
2. DATABASE and DATA: write into the idle slot. A record that matches what is already there is acknowledged and not rewritten. A record that would need a cleared bit set again is refused.
3. END: read the whole image back from flash and check the header, the length and the payload CRC-32. Only if all of that passes, write the `TrialPending` record. Reply ACK.
4. After `resetDelayMs` (500 ms) so the ACK has left the UART, request a reset. The bootloader runs the trial.
5. If the host goes quiet for `idleTimeoutMs` (5 s), the MCU sends FAILURE and returns to idle. That FAILURE is what frees the host from its wait.

Nothing is armed before END, so a transfer that stops anywhere before then leaves the old image running and a half-written idle slot that does not verify.

## Failure matrix

| Event | What the MCU does | State afterwards | Host sees |
|---|---|---|---|
| Bad payload CRC (a byte wrong anywhere) | END read-back fails, FAILURE, nothing armed | Old image confirmed, idle slot unusable | Success. It does not read the END reply. Harness: `ImageWithABadCrcIsRefused...` |
| Truncated transfer (records missing, END sent) | Length or CRC check fails at END, FAILURE | Old image | Success, same reason |
| Header that lies about its length | Rejected at END, FAILURE | Old image | Success, same reason |
| Dropped ACK | MCU waits for the next record, times out after 5 s, FAILURE | Old image, idle slot partly written | FAILURE, the transfer ends. Harness: `ADroppedAckIsEndedByTheMcuTimeout...` |
| Dropped BEGIN or lost INITIATE | No transfer in progress; any DATA is refused with FAILURE | Old image | FAILURE, or a wait the operator must end (the host has no timeout) |
| Record sent twice | Acknowledged and not rewritten if identical | Unchanged | ACK |
| Different record at an address already written | Refused, FAILURE | Old image, transfer over | FAILURE |
| Record addressed outside the slot | Refused, FAILURE | Old image | FAILURE |
| Record too short or too long | Refused, FAILURE | Old image | FAILURE |
| DATA, DATABASE or END with no INITIATE | Refused, FAILURE | Idle | FAILURE |
| New INITIATE mid transfer | Restarts: erases again | Old image, fresh transfer | BEGIN |
| INITIATE after a committed update, before the reset | Refused (Busy), the commit stands | Trial armed | FAILURE |
| INITIATE while running an unconfirmed trial | Refused | Unchanged | FAILURE |
| Reset mid transfer | Boot control untouched | Old image runs, a fresh update works | n/a, the link is gone |
| Power cut while erasing or writing the idle slot | Nothing armed | Old image runs; the slot fails its CRC | The host waits; the operator stops it. Harness: `PowerLostInTheMiddleOfTheTransfer...` |
| Power cut while erasing a boot-control copy | The newest record is the other copy | Old image runs | Same |
| Power cut in the middle of the new boot-control record | The record fails its CRC and is ignored | Old image runs | Same. Harness: `PowerLostInTheMiddleOfTheBootControlRecord...` |
| Power cut after the record is written, before the reset | Record is valid | Next boot runs the new image as a trial | n/a |
| New image boots, never reaches the handshake | Not confirmed | Next reset reverts to the old image | n/a |
| New image boots, confirms | `Confirmed` record written | New image is the good one | Version in the handshake changes. Harness: `HappyPath` |
| Boot-control write fails when confirming | Retried after `confirmRetryMs` | Trial stays a trial; a reset before it lands reverts | n/a |
| One boot-control copy damaged | The other copy wins | Unchanged | n/a |
| Both copies damaged | Boot picks the first slot that verifies and writes a record | One valid image runs | n/a |
| Good slot damaged | Falls back to the other slot if it verifies | Other image runs, `fellBack` reported | Version may differ |
| No slot verifies | Not bootable, recovery | No application | n/a |

`UpdatePowerCut.CuttingPowerAtEveryFlashByteOperationAlwaysLeavesABootableImage` cuts power at every flash byte operation of a full update plus confirm and checks that the unit boots something that verifies. A second sweep never confirms and checks the unit never ends on the new image. `BootRig.ARecordWriteCutAtAnyByteLeavesEitherTheOldRecordOrTheNewOne` covers the record write on its own.

## Image authenticity

A checksum catches corruption, not tampering. I did not implement signing. The MCU part is not public, so I do not know if it has secure boot, an image signature check in ROM, or a hardware crypto block, and I did not want to pick an algorithm for hardware I cannot see.

What is in place for it: the header has 32 reserved zero bytes (the `signature` field, offset 32) meant for a signature over the first 32 header bytes and the payload CRC. Nothing writes or checks it. Adding signing means deciding who holds the key, verifying in the bootloader (not the application, which is the thing being replaced), and refusing an unsigned image at `verifyImage`. Until then anyone who can reach the update path can install an image of their choosing, as long as its CRC matches.

## Other SYSTEM commands

- `GET_SW_VER`: the handshake reply is `[major, minor]`, taken from the running image's header when it verifies. A host request for it gets the 15-byte extended report: version (3 bytes), status, `FLASH_VALID`, the computed payload CRC-32, the revision and flags. This works whether or not an update is ever attempted.
- `BPM`: a big-endian 16-bit tempo. It is stored; the one consumer is the blink clock, behind `blinkFollowsBpm`, off by default.
- `SHUTDOWN`: acknowledged, LEDs off, no more input events, then a power-off request once the queue has drained. The power call is a stub with a `TODO(hardware)`. The host does not send this over the wire today.
- `RESET_GD` (0): not handled. Nothing in the host sends it and its meaning is not written down.

## Testing

```
cmake -S Firmware/kick-mcu -B build/mcu -G Ninja
cmake --build build/mcu
ctest --test-dir build/mcu --output-on-failure
```

The unit tests cover the image and boot-control formats, the updater and the power-cut sweeps. `kick-mcu-sim` takes `--flash FILE`, `--factory-hex FILE`, `--boot-report -` and `--inspect` to run the firmware against a flash that persists across runs.

### The host updater harness

`-DKICK_MCU_HOST_UPDATER=ON` (with `-DKICK_JUCE_DIR=<JUCE checkout>`, found automatically under `.3rdPartySourceCache/juce`) adds `kick_host_updater_tests`. It compiles `kick_FirmwareUpdate.cpp` without changes and runs its `MCUUpdate` against the firmware, over frames the host's own `PktMgr` encodes and decodes. What stands in for the rest of the host is listed at the top of `tests/host_updater/host_shim.cpp`: the DCL signals, `HwPref`'s constructor, a stub for the USB gadget, and a copy of the receive side of `handleCommand`, since that class is a JUCE component.

Each scenario runs in its own process, because the host updater cannot run twice in one: `boDataTransferPending` is never set true again after a run, and `MCUUpdate` keeps its parse position in function statics. The harness is off by default because it builds JUCE's core module. It is verified on macOS. The Linux-only parts of the host file are excluded by its own `#if` elsewhere, and I have not built it on Linux.

This stands in for running the host in the emulator. The emulator's Surface replaces the MCU, so the host's updater cannot reach the new state machine through it without more work than the harness.

## Not verified on hardware

- Real flash timing, erase block size, slot size and address map.
- That a power cut on a real part leaves the same prefix of a write as `SimFlash`. I assumed a write that stops partway leaves a prefix and a block erase that stops leaves the rest of the block as it was.
- That the bootloader split matches how the real MCU boots. `selectBoot` is the bootloader's half and runs in the same process as the application in the simulator.
- The reset and power-off calls.
- Timing with the real UART: 230400 baud, and the host's own pacing of DATA frames.
