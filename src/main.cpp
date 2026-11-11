#include "PCH.h"

#include "Config.h"
#include "Markers.h"

namespace
{
    void SetupLog()
    {
        auto logDirectory = SKSE::log::log_directory();
        if (!logDirectory) {
            return;
        }

        auto logPath = *logDirectory / "EnchantersHandbook.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto logger = std::make_shared<spdlog::logger>("EnchantersHandbook", std::move(sink));

        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [EnchantersHandbook] [%l] %v");
        spdlog::set_default_logger(std::move(logger));
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (a_message && a_message->type == SKSE::MessagingInterface::kDataLoaded) {
            EnchantersHandbook::Markers::Install();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    SetupLog();
    spdlog::info("EnchantersHandbook 0.2.3 loading");

    EnchantersHandbook::Config::Load();
    if (EnchantersHandbook::Config::Get().debug) {
        spdlog::set_level(spdlog::level::debug);
    }

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        spdlog::critical("Could not register SKSE messaging listener");
        return false;
    }

    return true;
}
