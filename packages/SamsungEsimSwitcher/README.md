# SamsungEsimSwitcher

Privileged Settings entry that flips Samsung's **tsds2** hybrid SIM tray between
the physical SIM and the built-in eUICC.

Package: `com.samsung.android.esimswitcher`

Module: `SamsungEsimSwitcher` (`system_ext`, platform-signed, `android.uid.system`)

It does **not** download or activate eSIM profiles itself. That is the platform
LPA (GoogleEuicc, OpenEUICC). This app only exposes the hardware slot switch.

## Why it exists

On these devices the second tray slot is a hybrid port: a physical nano-SIM
**or** the on-board eUICC. Stock Samsung switches it from TelephonyUI; AOSP has
no equivalent control.

## How the switch works

The same way stock TelephonyUI's `SimSwitchManager` does it:
`TelephonyManager.setSimSlotMapping()` with one of two fixed mappings. The
stock RIL handles tsds2 inside its `SET_LOGICAL_TO_PHYSICAL_SLOT_MAPPING`
handler and rewrites the eUICC entry to port 1 itself.

Physical slots in the mapping request, as stock numbers them: `0` and `1` are
the tray, `2` is the eUICC. Slot status only ever reports two slots, so the
framework sees the hybrid tray or the eUICC, whichever is muxed in, as slot
`1`. That is why `non_removable_euicc_slots` is `1`.

The modem keeps the mapping across reboots, so nothing is re-applied at boot.

| Target | Mapping `(port, physical, logical)` | Stock name |
|--------|-------------------------------------|------------|
| pSIM | `(0,0,0)`, `(0,1,1)` | `PS1_PS2` |
| eSIM | `(0,0,0)`, `(0,2,1)` | `PS1_EP1` |

`setSimSlotMapping()` blocks until the RIL answers and throws if the request is
rejected, so there is nothing to poll. The current state is read back from
`ril.simslottype1` / `ril.simslottype2`, as stock does.

## Components

| Class | Role |
|-------|------|
| `EsimSettingsActivity` | Collapsing-toolbar Settings host; registered under Wireless via `IA_SETTINGS` |
| `EsimSettingsFragment` | Main switch + footer and the confirmation dialogs |
| `EsimController` | Slot mapping request and state helpers |

There is no runtime support check: the only product that ships this app is
tsds2, and `persist.ril.esim.slotswitch` comes from the extracted vendor
`floating_feature.xml`, so the answer is fixed at build time.

## Toggle flow

1. User flips **Enable eSIM**.
2. Turning it on while a **card is present in the hybrid tray** asks for
   confirmation, since the switch disconnects that SIM.
3. Turning it off needs no confirmation, even with an eSIM profile enabled —
   stock does the same. The profile stays enabled on the eUICC and comes back
   when the slot is switched back.
4. The switch runs on an executor owned by the `EsimController` singleton, so it
   finishes even if the user leaves Settings. On re-entry the fragment restores
   the “Switching…” state from `isSwitchInProgress()`, and a concurrent request
   is rejected rather than queued.

## Related

ES10 fixups that AOSP's LPA path needs (session-id CLA strip, MEP-A1 port
inject) live in the device tree's `libsec-ril` shim, e.g.
`device/samsung/sm8650-common/shims/libsec-ril`.
