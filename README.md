# VoidWeaponWheel.VC

GTA Vice City weapon wheel ASI, based on the VoidWeaponWheel (SA) concept and built with Plugin-SDK.

Hold a key, aim the radial wheel with mouse / right stick, release to equip.

## Features

- Radial wheel for all 10 VC weapon slots (unarmed, melee, thrown, pistol, shotgun, SMG, assault, sniper, heavy, special)
- Icons from the game's HUD TXD (`CHud::Sprites`)
- Weapon name + ammo in the center / under each icon
- Optional slow motion while the wheel is open
- Disables vanilla weapon cycling so the wheel fully replaces it
- Blocks firing / camera stick while the wheel is open
- Optional GInputVC support (L2+R2 chord + right-stick aim)
- `VoidWeaponWheel.ini` configuration

## Requirements

- GTA Vice City **1.0** (10EN). 11EN / Steam addresses are also wired via Plugin-SDK `ADDRESS_BY_VERSION`.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (`scripts/` or `plugins/` folder)
- [Plugin-SDK](https://github.com/DK22Pac/plugin-sdk) with `PLUGIN_SDK_DIR` environment variable set, built for VC (`Plugin_VC.lib`)
- Optional: [GInputVC](https://gtaforums.com/topic/824734-ginput/) for gamepad

## Install

1. Build `Release GTA-VC` (or drop a prebuilt `VoidWeaponWheel.VC.asi`).
2. Copy `VoidWeaponWheel.VC.asi` and `VoidWeaponWheel.ini` into `GTA VC\scripts\`.
3. Launch the game.

## Controls (defaults)

| Action | Input |
|--------|--------|
| Open wheel | Hold **Q** |
| Aim | Mouse / right stick |
| Equip | Release **Q** |
| Gamepad open | Hold **L2 + R2** (with GInputVC) |

## Build (MSVC)

```bat
set PLUGIN_SDK_DIR=Q:\path\to\plugin-sdk
msbuild ASI\VoidWeaponWheelVC\VoidWeaponWheelVC.sln /p:Configuration="Release GTA-VC" /p:Platform=Win32
```

Plugin-SDK must already be built for GTA VC (`output\lib\Plugin_VC.lib`).

## Config (`VoidWeaponWheel.ini`)

| Key | Default | Meaning |
|-----|---------|---------|
| PrimaryKey | 81 (`Q`) | Virtual-key held to open |
| SecondaryKey | 0 | Alternate key (0 = off) |
| EnableSlowMotion | 1 | Slow the game while open |
| SlowMotionSpeed | 0.15 | Time scale while open |
| SkipEmptySlots | 0 | Hide unused categories |
| AllowInVehicle | 0 | Open while driving |
| DisableVanillaCycle | 1 | Replace original weapon cycling |
| WheelRadius / WheelInnerRadius | 120 / 55 | Ring size (640x480 base) |
| GInputOpenButton | 0 | 0 = L2+R2, 2 = Circle+L1 |

## Notes vs SA version

The SA mod splits UI (CLEO) and input (ASI). This VC port is a **single ASI**: input, hooks, and drawing all live in `VoidWeaponWheelVC.cpp` using Plugin-SDK events (`gameProcessEvent`, `drawHudEvent`, `initRwEvent`).
