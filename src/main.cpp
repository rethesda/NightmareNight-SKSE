#include "FrenzyMenu.h"
#include "Serialize.h"

namespace NightmareNight
{
	struct Update
	{
		static void thunk(RE::ActorValueMeter& meter)
		{
			if (meter.actorValue != RE::ActorValue::kMagicka)
				return func(meter);

			if (IsTransformed()) {
				const auto pct = GetFrenzyPct();
				if (!pct) {
					// Set Magicka Meter to 0 on first tick after transformation
					if (!_transformed) {
						_transformed = true;
						SetMeterPct(meter, 0);
						FrenzyMenu::Show();
					} else if (_frenzied) {
						_frenzied = false;
						SetMeterPct(meter, 0);
						meter.root.Invoke(meter.setBlinkingName.c_str());
						FrenzyMenu::FadeOut();
					}
					return;
				} else if (auto alpha = meter.view->GetVariableDouble(MagicaAlpha); alpha < 100) {
					meter.view->SetVariableDouble(MagicaAlpha, 100);
				}
				if (!_frenzied) {
					_frenzied = true;
					FrenzyMenu::FadeIn();
				}
				SetMeterPct(meter, *pct);
				return;
			} else if (_transformed) {
				_frenzied = false;
				_transformed = false;
				FrenzyMenu::ForceHide();
			}
			return func(meter);
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t size = 0x1;

	private:
		static constexpr const char* MagicaAlpha{ "_root.HUDMovieBaseInstance.Magica._alpha" };
		static inline bool _transformed{ false };
		static inline bool _frenzied{ false };

		static void SetMeterPct(RE::ActorValueMeter& meter, double percent)
		{
			std::array<RE::GFxValue, 2> args{ percent, false };
			meter.root.Invoke(meter.setPctName.c_str(), nullptr, args);
		}

		static inline bool IsTransformed()
		{
			static const auto werebeast = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x8F2, "NightmareNight.esp"sv);
			static const auto requiem = RE::TESDataHandler::GetSingleton()->LookupForm<RE::EffectSetting>(0x909, "NightmareNight.esp"sv);
			const auto player = RE::PlayerCharacter::GetSingleton();
			return player->HasKeyword(werebeast) || player->HasMagicEffect(requiem);
		}

		static std::optional<float> GetFrenzyPct()
		{
			static const auto bloodfrenzy = RE::TESDataHandler::GetSingleton()->LookupForm<RE::EffectSetting>(0x897, "NightmareNight.esp"sv);
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto effects = player->GetActiveEffectList();
			if (!effects)
				return std::nullopt;
			for (auto& e : *effects) {
				if (!e || e->GetBaseObject() != bloodfrenzy)
					continue;
				else if (e->flags.any(RE::ActiveEffect::Flag::kDispelled, RE::ActiveEffect::Flag::kInactive))
					continue;
				const float dur = e->duration - e->elapsedSeconds;
				return (dur / e->duration) * 100.0f;
			}
			return std::nullopt;
		}
	};

	inline void Install()
	{
		stl::write_vfunc<RE::ActorValueMeter, Update>();
	}
}

#ifdef SKYRIM_SUPPORT_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.AuthorName("Scrab Joséline"sv);
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });
	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION.pack();
	return true;
}
#endif

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	const auto InitLogger = []() -> bool {
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path)
			return false;
		*path /= fmt::format(FMT_STRING("{}.log"), Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
#ifndef NDEBUG
		log->set_level(spdlog::level::trace);
		log->flush_on(spdlog::level::trace);
#else
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
#endif
		spdlog::set_default_logger(std::move(log));
#ifndef NDEBUG
		spdlog::set_pattern("%s(%#): [%T] [%^%l%$] %v"s);
#else
		spdlog::set_pattern("[%T] [%^%l%$] %v"s);
#endif

		logger::info("{} v{}", Plugin::NAME, Plugin::VERSION.string());
		return true;
	};

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible");
		return false;
	} else if (!InitLogger()) {
		logger::critical("Failed to initialize logger");
		return false;
	}

	SKSE::Init(a_skse);
	logger::info("{} loaded", Plugin::NAME);

	NightmareNight::Install();
	NightmareNight::FrenzyMenu::Register();

	const auto papyrus = SKSE::GetPapyrusInterface();
	papyrus->Register(NightmareNight::Papyrus::Register);

	const auto serialization = SKSE::GetSerializationInterface();
	serialization->SetUniqueID('YKud');
	serialization->SetSaveCallback(Serialization::Serialize::SaveCallback);
	serialization->SetLoadCallback(Serialization::Serialize::LoadCallback);
	// serialization->SetRevertCallback(Serialization::Serialize::RevertCallback);
	// serialization->SetFormDeleteCallback(Serialization::Serialize::FormDeleteCallback);

	logger::info("Initialization complete");

	return true;
}
