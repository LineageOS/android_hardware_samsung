# SamsungEsimSwitcher

Privileged Settings entry that flips Samsung's **tsds2** hybrid SIM tray between
the physical SIM and the built-in eUICC, then makes sure telephony sees a valid
EID so Google LPA can manage profiles.

Package: `com.samsung.android.esimswitcher`  
Module: `SamsungEsimSwitcher` (`system_ext`, platform-signed, `android.uid.system`)

It does **not** download or activate eSIM profiles itself. That is Google
EuiccGoogle / the platform LPA. This app only exposes the hardware slot switch
and kicks EuiccCard into loading the EID.

## Why it exists

On these devices, slot 2 is a hybrid port:

- Physical nano-SIM **or**
- On-board eUICC

Stock Samsung uses OEM RIL hooks (`SEC_SIM_LOW_LEVEL_CONTROL`) to switch. AOSP
has no UI for that. EsimSwitcher is the user-facing control; the actual modem
work lives in the [`libsec-ril` shim](../../shims/libsec-ril/README.md).

## Architecture

```text
Settings → EsimSwitcher
             │  set persist.sys.esim_switch = 0|1
             ▼
init.esim_switch.rc
             │  set vendor.calls.esim_switch = 0|1
             ▼
libsec-ril shim (rild)
             │  OEM 0x10 / 0x11 on RIL_SOCKET_1
             ▼
ril.simslottype2 = 1 (eSIM) or 0 (pSIM)
             │
EsimSwitcher (after flip)
             │  setSimPowerStateForSlot(1, DOWN/UP)
             ▼
EuiccCard.loadEid → shim GetEID synth from /efs/FactoryApp/eID
             │
Google LPA can download / enable / disable profiles
```

## Components

| Class | Role |
|-------|------|
| `EsimSettingsActivity` | Collapsing-toolbar Settings host; registered under Wireless via `IA_SETTINGS` |
| `EsimSettingsFragment` | Main switch + footer; runs switch work off the UI thread |
| `EsimController` | Prop I/O, wait for slot type, SIM power-cycle for EID refresh |
| `BootCompletedReceiver` | Enables/disables the Settings activity on supported devices; refreshes EID after boot if eSIM is on |

### Manifest / privileges

- `sharedUserId="android.uid.system"`, `persistent`, direct-boot aware
- Permissions: `RECEIVE_BOOT_COMPLETED`, `MODIFY_PHONE_STATE`,
  `READ_PRIVILEGED_PHONE_STATE` (+ privapp allowlist)
- Activity category: `com.android.settings.category.ia.wireless` (Network &
  internet), order `-10`

## Behaviour in detail

### Support gate

On `BOOT_COMPLETED`, the activity is enabled only if:

- `persist.ril.esim.slotswitch` equals `tsds2` (case-insensitive), **or**
- `config_nonRemovableEuiccSlots` (`non_removable_euicc_slots`) is non-empty

Otherwise the Settings tile stays disabled/hidden via component state.

### Toggle flow (`EsimSettingsFragment`)

1. User flips **Enable eSIM**.
2. If an **embedded subscription is already active** and the user is trying to
   turn the hybrid slot off, show a blocking dialog
   (“disable or remove your current eSIM profile first”) and abort.
3. Otherwise disable the switch UI, set footer to “Switching…”, and call
   `EsimController.setEsimEnabled` on a background executor.
4. When done, re-enable the switch to match `persist.sys.esim_switch` and restore
   the footer (or show a timeout message).

### `EsimController.setEsimEnabled`

1. Write `persist.sys.esim_switch` to `1` or `0`.
2. Init bridges that into `vendor.calls.esim_switch`; the RIL shim performs the
   OEM slot switch.
3. Poll up to **90s** (200 ms steps) until `ril.simslottype1` or
   `ril.simslottype2` is `1` (enable) or neither is (disable).
4. On successful **enable**, call `refreshEuiccCardEid`:
   - `TelephonyManager.setSimPowerStateForSlot(1, CARD_POWER_DOWN)`
   - sleep 1.5 s
   - `CARD_POWER_UP`
   - poll up to **45 s** until `UiccSlotInfo.cardId` for physical slot **1** is a
     valid 32-char hex EID

Power-cycling slot 1 forces telephony to recreate `EuiccCard` and re-run
`loadEid`, which hits the shim's synthetic GetEID response. Without that,
LPA may see the eUICC slot but no usable card id.

### Boot path

If eSIM was left enabled and the slot type is already eSIM,
`onBootCompleted` starts a background EID refresh so LPA works after reboot
without another toggle.

### State helpers

| API | Meaning |
|-----|---------|
| `getEsimEnabled()` | Persist prop is `1` |
| `isSlotTypeEsim()` | `ril.simslottype1` or `ril.simslottype2` is `1` |
| `isEsimReady()` | Slot type is eSIM **and** `vendor.calls.esim_ready=1` |
| `getEsimActive()` | Any active subscription with `isEmbedded` |
| `getEuiccSlotCardId()` | Valid EID from `uiccSlotsInfo[1].cardId` |

## Properties

| Property | Writer | Purpose |
|----------|--------|---------|
| `persist.sys.esim_switch` | EsimSwitcher | User intent; survives reboot |
| `vendor.calls.esim_switch` | init | Vendor-side copy for rild |
| `vendor.calls.esim_ready` | libsec-ril shim | Hardware switch completed |
| `ril.simslottype2` | RIL | `1` = hybrid slot is eUICC |
| `persist.ril.esim.slotswitch` | device tree / vendor | Must be `tsds2` for support detection |

## What it does **not** do

- No ES10 APDU crafting (CLA fix / MEP port inject live in the RIL shim).
- No profile download / enable / delete UI (Google LPA).
- No framework patches to `EuiccCard` / telephony-common.
- Does not switch slot 0 (primary SIM).
