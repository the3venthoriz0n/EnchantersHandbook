#pragma once

// Settings live in MCM Helper's files, so they can be edited in the SkyUI Mod Configuration Menu:
//   defaults  Data\MCM\Config\EnchantersHandbook\settings.ini   (shipped with the mod)
//   user      Data\MCM\Settings\EnchantersHandbook.ini          (written by the MCM; overrides the defaults)
// Without MCM Helper the same files can be edited by hand. Changes are picked up within a second.
namespace EnchantersHandbook::Config
{
    struct Settings
    {
        bool        enabled{ true };
        bool        showOnOwnItems{ true };  // also mark items in your own inventory, not only merchants and containers
        bool        suffix{ true };          // marker after the item name (keeps name sorting intact); false = before
        std::string marker{ "[v]" };         // known enchantment; plain ASCII: Skyrim's fonts lack most symbols
        bool        markBlocked{ true };     // also flag enchanted items the game will not let you disenchant
        std::string blockedMarker{ "[x]" };
        bool        tintIcons{ false };      // experimental: tint the item icon green (known) / red (cannot disenchant)
        bool        debug{ false };
    };

    void Load();

    // Re-reads the files if either changed on disk. Cheap enough to call about once a second.
    void ReloadIfChanged();

    // A copy of the current settings (thread-safe).
    [[nodiscard]] Settings Get();
}
