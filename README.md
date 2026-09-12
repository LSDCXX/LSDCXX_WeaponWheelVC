# LSDCXX_WeaponWheelVC

A GTA Vice City weapon wheel ASI plugin built with [Plugin-SDK](https://github.com/DK22Pac/plugin-sdk) and rendered via native RenderWare 2D primitives. Features a custom 80s synthwave neon visual style (cyan & magenta) tailored specifically for Vice City.

Hold a key, swipe with the mouse to highlight a weapon, and release to switch.

---

## Features

- **Vice City 80s Synthwave Aesthetics**: Built with dual-tone neon glowing rings (Cyan `#00EBEF` & Magenta `#E6148C`), smooth sector highlighting, and no seams or banding artifacts.
- **Hardware-Accelerated 2D Rendering**: Uses native RenderWare 2D immediate mode primitives (`RwIm2DVertex` / `RwIm2DRenderPrimitive`) for seamless geometry and anti-aliased arcs.
- **Standalone Custom Icons**: Loads external PNG textures (`models\weapvww\w00.png` ~ `w36.png`) via `stb_image`, preventing conflicts with `hud.txd`.
- **Bullet Time**: Optional slow-motion effect while the wheel is open (default `0.15x`).
- **Fully Customizable Localization**: Weapon display names can be configured directly in `LSDCXX_WeaponWheelVC.ini` without modifying source code or GXT tables.
- **Seamless Control Interception**: Disables original weapon cycling, suppresses combat keys/clicks, and locks mouse look-around while selecting.
- **Smart Category Filtering**: Option to hide empty weapon slots (`SkipEmptySlots`) to keep the wheel compact.

---

## Requirements

- **GTA Vice City** (Compatible with **1.0 EN**, **1.1 EN**, and **Steam**)
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (`scripts/` or `plugins/` folder)
- Custom weapon icons located at: `Grand Theft Auto Vice City\models\weapvww\` (`w00.png` - `w36.png`)

---

## Installation

1. Place `LSDCXX_WeaponWheelVC.asi` and `LSDCXX_WeaponWheelVC.ini` into your game's `scripts\` (or `plugins\`) directory.
2. Ensure custom weapon icons are placed in `models\weapvww\` inside your GTA Vice City directory.
3. Launch the game.

---

## Controls (Default)

| Action | Input |
|---|---|
| **Open Wheel** | Hold **Q** |
| **Select Slot** | Move **Mouse** |
| **Equip & Close** | Release **Q** |

---

## Configuration (`LSDCXX_WeaponWheelVC.ini`)

### `[configs]`

| Setting | Default | Description |
|---|---|---|
| `PrimaryKey` | `81` (`Q`) | Virtual key code to open the wheel. |
| `SecondaryKey` | `0` | Alternate key (set `0` to disable). |
| `EnableSlowMotion` | `1` | Enables bullet time while the wheel is held (`1` = On, `0` = Off). |
| `SlowMotionSpeed` | `0.15` | Game timescale multiplier while active (`0.01` ~ `1.0`). |
| `InvertMouseVertical` | `0` | Inverts vertical mouse aim direction (`1` = Inverted, `0` = Normal). |
| `SkipEmptySlots` | `1` | Hides empty categories (`1` = Compact, `0` = Show all 10 slots). |
| `AllowInVehicle` | `0` | Allows opening the wheel while driving (`1` = Allowed, `0` = Blocked). |
| `ShowAmmo` | `1` | Displays remaining ammo in the wheel center (`1` = Show, `0` = Hide). |
| `ShowWeaponName` | `1` | Displays weapon name in the wheel center (`1` = Show, `0` = Hide). |
| `DisableVanillaCycle`| `1` | Disables vanilla mouse scroll / key cycling (`1` = Disabled). |
| `WheelRadius` | `280.0` | Outer radius of the wheel. |
| `WheelInnerRadius` | `110.0` | Inner cutout radius of the wheel center. |
| `SelectDeadzone` | `30.0` | Mouse deadzone distance before changing the highlighted slot. |

### `[names]`

Customize display strings for any language (uses native system encoding / `CP_ACP`):

```ini
[names]
Weapon00=Unarmed
Weapon01=Brass Knuckles
Weapon02=Screwdriver
...
Weapon26=M4
Weapon33=Minigun