# Enchanter's Handbook

Quality-of-life tools for enchanters in Skyrim Special Edition / Anniversary Edition.

Status: **0.1.2 prototype.** It compiles; it has not been played yet. See "Known unknowns".

## Features

### Known enchantment marker

Enchanted items whose enchantment you have already learned (by disenchanting an item that has it) get a
marker in item lists, so you can tell at a glance while shopping or looting which enchantments you
still need. It uses the same "known" flag the enchanting table uses.

Works in merchant (barter), container, gift and inventory menus. Example: `Iron Sword of Fire [E]`.

## Requirements

- Skyrim SE / AE 1.6.1170 or 1.7.104, with SKSE and Address Library
- SkyUI (item lists are refreshed through its list)
- MCM Helper, for the settings menu (optional; the files can be edited by hand)

## Settings

In the SkyUI Mod Configuration Menu under "Enchanter's Handbook": on/off, mark your own inventory,
the marker text, and whether it goes before or after the name. Without MCM Helper edit:

- defaults, shipped with the mod: `Data\MCM\Config\EnchantersHandbook\settings.ini`
- your overrides: `Data\MCM\Settings\EnchantersHandbook.ini` (wins over the default)

The marker is plain text because Skyrim's fonts lack most symbols. A real graphical icon needs a
modified SkyUI interface file and is a possible future step.

## Known unknowns (test these first)

- Whether the marker shows in each menu and the list redraws without flicker or losing your selection.
- Whether items you enchanted yourself count as "known" (they use a player-made enchantment form).
- If the marker never appears, check `Documents\My Games\Skyrim Special Edition\SKSE\EnchantersHandbook.log`.

## Build

Needs Visual Studio (C++), CMake and vcpkg (`VCPKG_ROOT`, default `C:\dev\vcpkg`).
Run `build_release.bat` from a Visual Studio x64 developer prompt, then `make_archive.bat` for the zip.

## License

CommonLibSSE-NG is GPL-3.0-or-later and is linked into the plugin, so this plugin is GPL too (see COPYING).
