# Samsung radio HAL

An open source AIDL radio HAL for Samsung devices. It replaces Samsung's `rild`,
`libril_sem` and `sehradiomanager`, and talks directly to the closed modem library,
`libsec-ril-impl.so`, which stays in place.

```
Telephony ── AIDL radio HAL (this) ── libsec-ril-impl.so (Samsung blob) ── modem
```

## What it does

- Serves `IRadioConfig`, and `IRadioModem`, `IRadioNetwork`, `IRadioSim`, `IRadioData`,
  `IRadioVoice` and `IRadioMessaging` for each SIM slot, all at AIDL version 5, and `ISap`
  (Bluetooth SIM Access Profile) at version 1.
- Loads `libsec-ril-impl.so`, calls its `RIL_Init` and translates every request, response and
  indication between AIDL and the blob's structs.
- Runs as `ril-daemon` (`android.hardware.radio-service.samsung`).

## Layout

| Path | Contents |
|---|---|
| `ril/` | `libril-samsung.so`: loads the blob and provides the `libril_sem` symbols it imports |
| `ril/include/samsung_ril/` | Samsung's request ids, indication ids and struct layouts |
| `RadioSlot.*` | One per SIM: owns the services and routes indications to them |
| `config/`, `modem/`, `network/`, `sim/`, `data/`, `voice/`, `messaging/`, `sap/` | One AIDL service each |
| `WakeLock.*` | Holds a wakelock until the framework acknowledges an indication |

## How it works

**Startup.** The service reads the SIM slot count, loads `libsec-ril-impl.so` and calls its
`RIL_Init` with a table of callbacks. The blob connects to the modem and returns its own table
of functions. `RIL_SAP_Init` does the same for SIM Access. Each slot then publishes its AIDL
services, and the `FW_READY` settings are sent once the radio is available.

**Requests.** Telephony calls an AIDL method with a serial number. The service converts the
arguments into the C struct the blob expects and passes it to the blob's `onRequest` with a
token. When the blob calls back with that token, the result is converted to AIDL and sent to
Telephony with the original serial.

```
Telephony ─ dial(serial, ...) ─▶ RadioVoice ─ RIL_Dial ─▶ blob ─▶ modem
Telephony ◀─ dialResponse(serial) ─ RadioVoice ◀─ OnRequestComplete(token) ─ blob
```

**Indications.** The blob reports events such as an incoming call or a new SMS through its
unsolicited callback. `RadioSlot` uses the id to pick the service, which converts the payload
and forwards it. Indications that need Telephony to act hold a wakelock until Telephony
acknowledges them.

**Calls back into `libril_sem`.** The blob still imports a few functions from `libril_sem`.
`libril-samsung` provides them, so with a blob fixup that points the blob at it, `libril_sem`
and `rild` are not needed.

## What was reverse engineered

Samsung publishes none of this, so it was worked out from the blobs:

- **Entry point.** `RIL_Init` returns a function table with an extra `onPhoneDied` slot
  before `getVersion`. `onCancel` must never be called.
- **Request ids.** Requests newer than this tree's `ril.h`, and Samsung's own, use ids only
  the blob defines (`SEC_REQUEST_*` in `structs.h`).
- **Indication ids.** `id / 100000` picks the service (20 to 26), the remainder is the AOSP
  `RIL_UNSOL_*` id.
- **Struct layouts.** Network, data and call structs differ from AOSP's `ril.h`: strings are
  inlined and most structs are larger. A data call response is about 1.3 MiB.
  Every layout is checked with `static_assert`s.
- **`libril_sem` exports.** The blob imports a few functions from it, such as
  `RIL_GetClientHalVersion`. `libril-samsung` provides them, plus the nanopb symbols the blob
  uses.
- **SIM Access Profile.** `RIL_SAP_Init` returns a second function table. Requests and
  responses are AOSP `sap-api.proto` messages, encoded with nanopb.
- **Startup.** The blob holds back indications until it gets the settings from
  `/vendor/etc/sehradiomanager.conf` (`FW_READY=1` and so on) through OEM request 10051.

## What it fixes

With stock `rild` and `libril_sem`, these needed device patches or did not work at all. The
HAL now handles them itself.

**eSIM.** AOSP's path to the eUICC (ES10) broke in four places. It used to need a `libsec-ril`
shim plus a binary patch of `libril_sem`.
- The blob hands out logical channel session ids such as 101, and the framework ORs them into
  the APDU `CLA` byte, so the card rejects the command. `RadioSim` strips the session id from
  `CLA` before passing the APDU on.
- After a slot switch the framework assumes MEP-B and leaves the port out of
  `EnableProfile`. When `ril.esim.mep_mode` reports MEP-A1, `RadioSim::addMepA1Port` appends
  the port TLV.
- Slot status reports the eUICC as physical slot 1, but slot mapping addresses it as slot 2.
  `RadioConfig` rewrites the physical slot in mappings that use port 1.
- Card status reports port 0 and no EID for both eUICC ports. `UiccSlots` remembers the last
  slot status, and `RadioSim` fills the port and EID in from it.

**SIM on/off toggle.** The blob only reports UICC applications enablement changes when
`ro.build.version.oneui` is 6.1 or later, so the toggle in Settings never settled. This used to
need two binary patches in `libsec-ril-impl`. `RadioSim` now tracks the state itself: it
records the value after `enableUiccApplications`, asks the blob for it after every SIM status
change, and reports to Telephony whenever it changes.

**SMSC address.** Telephony sends back the address in the text form `GET_SMSC_ADDRESS`
returned, but the blob only accepts it as BCD. A `libsec-ril` shim used to drop the address
altogether. `SmscAddress` converts both ways instead: replies become dialable numbers, and text
becomes a BCD service center address. An empty address still selects the SIM's default.

**Call audio.** The audio HAL must know which SIM carries a CS call. A patched copy of the
audio HAL used to wait for the blob's call properties during the call. `AudioCallSlot` now sets
the audio HAL's `g_call_sim_slot` parameter before the HAL answers `GET_CURRENT_CALLS`, which
Telephony waits for before it switches audio to the call, so the plain AOSP
`android.hardware.audio@7.1-impl` works. `CallStatePublisher` also publishes each SIM's call
states in `vendor.calls.state` for audio HALs that start voice sessions from it.

**SMS over IMS.** The modem has no IMS registration, as IMS runs on the AP. `libril_sem`
rejected the request and Telephony then retried over CS. `RadioMessaging` sends it over CS
directly.

**SIM Toolkit call setup.** The blob reports it as a SIM indication, and `libril_sem` dropped
it. `RadioSlot` routes it to the voice service.

**`FW_READY`.** A separate daemon, `sehradiomanager`, used to send the vendor configuration.
`RadioSlot` sends it, and again whenever the radio becomes available.

**Bluetooth SIM Access.** The modem never reports the card reset status SAP requires after a
connection, and Android's Bluetooth server waits for it. `RadioSap` sends it after a
successful connect.

**SIM slot count.** Some devices only set `ro.multisim.simslotcount`, which made the HAL bring
up a single slot. `service.cpp` falls back to it.

## Hacks

These are deliberate shortcuts around what the blob expects.

| What | Why | How |
|---|---|---|
| Blob fixup | The blob links against `libril_sem.so` | Devices point it at `libril-samsung.so` with `replace_needed`. Without the fixup the blob keeps using `libril_sem`'s functions, which also works, but needs `libril_sem` and its Samsung libraries |
| Stub `libril_sem` functions | The file, command, settings and APN calls are served by Samsung's framework | They return -1, and the blob falls back to its defaults (`ril/SamsungExports.cpp`) |
| `RIL_GetClientHalVersion` | The blob also checks for the HIDL 1.x range | Returns the AIDL version plus 20 |
| `RIL_RegisterSehBridge` | Would register Samsung's bridge service | Only sets `ril.halservice.registered.slotN`, as the blob expects |
| nanopb symbols | The blob calls nanopb from `libril_sem` for SIM Access | `libril-samsung` links AOSP's nanopb with 32-bit fields, matching the `librilutils` message descriptors |
| `onCancel` | In the blob it shuts down the RIL | Never called |
| Requests from blob threads | The blob must not be called back from its own indication thread | `VendorRil::post()` runs such requests on our worker thread |
| SIM Access results | `libril_sem` reads the result from the payload | `RadioSap` ignores the `RIL_Errno` and sends empty requests with a valid pointer, as `libril_sem` does |
| `ISehRadioChannel` | The blob registers it for Samsung's IMS daemons | Devices declare it in their VINTF manifest |
| `vendor.calls.state` | `rild` sets it | Devices need a `property_contexts` entry that `rild` can set |
| Exynos modem restart | The Shannon modem must restart together with the RIL | Keep the stock `onrestart restart cpboot-daemon` lines in the init script |

## Other Samsung devices

The HAL should work on most Samsung devices LineageOS supports, with little or no change:

- **One RIL design for every chipset.** Samsung builds `libsec-ril` from the same framework
  for Qualcomm and Exynos. Each one identifies as "Samsung RIL v5.0" and has the same
  `RIL_Init` entry point and function table.
- **The modem stays behind the blob.** Qualcomm modems speak QMI and Exynos (Shannon) modems
  speak Samsung IPC, but the blob hides that. The HAL only sees the blob's interface.
- **Same `libril_sem` interface.** The HAL replaces `libril_sem`, so it has to match what the
  blob expects from it. The blobs of other LineageOS Samsung devices import the same
  functions, and their `libril_sem` comes from the same generations as the tested ones.
- **Proven across chipsets.** It runs on Snapdragon 8 Gen 2 and 8 Gen 3 devices, and the same
  binary runs unchanged on an Exynos 990 device.

What to check on a new device:

- Older blobs, built for Android 11 vendor images with a HIDL `libril_sem`, may use different
  struct layouts. A mismatch shows up as a logged payload size error.
- Some blobs also import `RIL_ConvertToUTF8`, which `libril-samsung` does not provide yet.
- On Exynos, the modem has to restart together with the HAL, as the stock init script does.

## Not supported

- `IRadioIms`.
- Samsung's own `ISehRadio*` interfaces, used only by Samsung's framework.
