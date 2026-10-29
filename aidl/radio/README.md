# Samsung radio HAL

An open source AIDL radio HAL for Samsung devices. It replaces Samsung's `rild`,
`libril_sem` and `sehradiomanager`, and talks directly to the closed modem library,
`libsec-ril-impl.so`, which stays in place.

```
Telephony ── AIDL radio HAL (this) ── libsec-ril-impl.so (Samsung blob) ── modem
```

## What it does

- Serves `IRadioConfig`, and `IRadioModem`, `IRadioNetwork`, `IRadioSim`, `IRadioData`,
  `IRadioVoice` and `IRadioMessaging` for each SIM slot, all at AIDL version 5.
- Loads `libsec-ril-impl.so`, calls its `RIL_Init` and translates every request, response and
  indication between AIDL and the blob's structs.
- Runs as `ril-daemon` (`android.hardware.radio-service.samsung`).

## Layout

| Path | Contents |
|---|---|
| `ril/` | `libril-samsung.so`: loads the blob and provides the `libril_sem` symbols it imports |
| `ril/include/samsung_ril/` | Samsung's request ids, indication ids and struct layouts |
| `RadioSlot.*` | One per SIM: owns the services and routes indications to them |
| `config/`, `modem/`, `network/`, `sim/`, `data/`, `voice/`, `messaging/` | One AIDL service each |
| `WakeLock.*` | Holds a wakelock until the framework acknowledges an indication |

## How it works

**Startup.** The service reads the SIM slot count, loads `libsec-ril-impl.so` and calls its
`RIL_Init` with a table of callbacks. The blob connects to the modem and returns its own table
of functions. Each slot then publishes its AIDL services, and the `FW_READY` settings are
sent once the radio is available.

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
`libril-samsung` provides them, so `libril_sem` and `rild` are not needed.

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
- **Startup.** The blob holds back indications until it gets the settings from
  `/vendor/etc/sehradiomanager.conf` (`FW_READY=1` and so on) through OEM request 10051.

## Workarounds for the blob

- Reports UICC applications enablement itself: the blob only does this when
  `ro.build.version.oneui` is 6.1 or later.
- Fixes eSIM (eUICC) access: strips the blob's channel session id from APDU `CLA`, adds the
  port to `EnableProfile` in MEP-A1 mode, reports the eUICC port and EID, and translates its
  slot numbering.
- Converts the SMSC address between text and the BCD form the blob requires.
- Sends SMS over IMS through CS instead, as the modem has no IMS registration.
- Routes STK call setup, which the blob reports as a SIM indication, to the voice service.

## Call audio (Qualcomm)

Before answering `GET_CURRENT_CALLS`, the HAL sets the audio HAL's `g_call_sim_slot`
parameter to the SIM with the active call, and publishes each SIM's call states in
`vendor.calls.state`. This is what lets the plain AOSP `android.hardware.audio@7.1-impl` be
used instead of a patched one.

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

- `IRadioIms` and `ISap` (Bluetooth SIM Access Profile).
- Samsung's own `ISehRadio*` interfaces, used only by Samsung's framework.
