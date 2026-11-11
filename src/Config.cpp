#include "PCH.h"
#include "Config.h"

#include <Windows.h>

namespace EnchantersHandbook::Config
{
    namespace
    {
        using FileTime = std::optional<std::filesystem::file_time_type>;

        std::mutex g_mutex;
        Settings   g_settings{};
        FileTime   g_defaultsTime;
        FileTime   g_userTime;

        std::filesystem::path DataDirectory()
        {
            // The DLL is in <Data>\SKSE\Plugins; derive <Data> from its location.
            HMODULE module = nullptr;
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&DataDirectory),
                &module);

            char buffer[MAX_PATH]{};
            if (module && GetModuleFileNameA(module, buffer, MAX_PATH) != 0) {
                return std::filesystem::path(buffer).parent_path().parent_path().parent_path();
            }
            return std::filesystem::path("Data");
        }

        std::filesystem::path DefaultsPath()
        {
            return DataDirectory() / "MCM" / "Config" / "EnchantersHandbook" / "settings.ini";
        }

        std::filesystem::path UserPath()
        {
            return DataDirectory() / "MCM" / "Settings" / "EnchantersHandbook.ini";
        }

        FileTime StampOf(const std::filesystem::path& a_path)
        {
            std::error_code ec;
            const auto      time = std::filesystem::last_write_time(a_path, ec);
            return ec ? FileTime{} : FileTime{ time };
        }

        // GetPrivateProfile* resolves relative paths against the Windows directory, so always pass
        // absolute paths. Returns nothing if the key is not present in the file.
        std::optional<std::string> ReadText(const std::filesystem::path& a_path, const char* a_section, const char* a_key)
        {
            char text[128]{};
            GetPrivateProfileStringA(a_section, a_key, "\x01", text, sizeof(text), a_path.string().c_str());
            if (text[0] == '\x01') {
                return std::nullopt;
            }
            return std::string(text);
        }

        // The user's value wins over the shipped default, which wins over the built-in default.
        std::string Text(const char* a_section, const char* a_key, const std::string& a_builtIn)
        {
            if (auto user = ReadText(UserPath(), a_section, a_key)) {
                return *user;
            }
            if (auto shipped = ReadText(DefaultsPath(), a_section, a_key)) {
                return *shipped;
            }
            return a_builtIn;
        }

        bool Flag(const char* a_section, const char* a_key, bool a_builtIn)
        {
            const auto text = Text(a_section, a_key, a_builtIn ? "1" : "0");
            return !text.empty() && text != "0";
        }

        Settings Read()
        {
            Settings s;
            s.enabled = Flag("General", "bEnabled", s.enabled);
            s.showOnOwnItems = Flag("General", "bShowOnOwnItems", s.showOnOwnItems);
            s.suffix = Flag("Marker", "bSuffix", s.suffix);
            s.marker = Text("Marker", "sMarker", s.marker);
            s.markBlocked = Flag("Marker", "bMarkBlocked", s.markBlocked);
            s.blockedMarker = Text("Marker", "sBlockedMarker", s.blockedMarker);
            s.tintIcons = Flag("Marker", "bTintIcons", s.tintIcons);
            s.debug = Flag("General", "bDebug", s.debug);
            return s;
        }
    }

    void Load()
    {
        std::scoped_lock lock(g_mutex);
        g_settings = Read();
        g_defaultsTime = StampOf(DefaultsPath());
        g_userTime = StampOf(UserPath());
    }

    void ReloadIfChanged()
    {
        const auto defaults = StampOf(DefaultsPath());
        const auto user = StampOf(UserPath());

        std::scoped_lock lock(g_mutex);
        if (defaults == g_defaultsTime && user == g_userTime) {
            return;
        }
        g_settings = Read();
        g_defaultsTime = defaults;
        g_userTime = user;
    }

    Settings Get()
    {
        std::scoped_lock lock(g_mutex);
        return g_settings;
    }
}
