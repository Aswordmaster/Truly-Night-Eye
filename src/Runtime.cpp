#include "Menu.h"
#include "NightEye.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace night_eye
{
	namespace
	{
		constexpr float kQueryInterval = 0.1f;
		constexpr float kMaximumFrameStep = 0.1f;
		constexpr std::uint32_t kMaximumActorHandles = 4096;
		using Affiliations = std::unordered_map<RE::FormID, unsigned int>;
		std::atomic<std::shared_ptr<const Affiliations>> affiliations;
		std::shared_ptr<const Affiliations> frameAffiliations;
		std::vector<const RE::EffectSetting*> effects;
		std::vector<const RE::TESImageSpaceModifier*> nightEyeModifiers;
		std::atomic<bool> active{ false };
		std::atomic<bool> queryPending{ false };
		std::atomic<bool> nativeBlurDesired{ false };
		std::atomic<bool> nativeBlurApplied{ false };
		std::atomic<bool> nativeBlurPending{ false };
		struct NativeBlurCurve
		{
			RE::NiPointer<RE::NiFloatInterpolator>* field;
			RE::NiPointer<RE::NiFloatInterpolator> original;
			RE::NiPointer<RE::NiFloatInterpolator> replacement;
		};
		std::vector<NativeBlurCurve> nativeBlurCurves;
		Settings frameSettings;
		float activation = 0.0f;
		float elapsed = 0.0f;
		float queryElapsed = kQueryInterval;

		void CacheNativeBlurCurve(RE::NiPointer<RE::NiFloatInterpolator>& field, RE::FormID formId)
		{
			if (!field)
				return;
			RE::NiPointer<RE::NiObject> cloned{ field->Clone() };
			auto* replacement = cloned ? netimmerse_cast<RE::NiFloatInterpolator*>(cloned.get()) : nullptr;
			if (!replacement || replacement == field.get()) {
				SKSE::log::warn("Night Eye modifier {:08X} blur curve could not be cloned; leaving it unchanged.", formId);
				return;
			}
			replacement->floatData.reset();
			replacement->floatValue = 0.0f;
			replacement->lastTime = std::numeric_limits<float>::lowest();
			replacement->lastIndex = 0;
			nativeBlurCurves.push_back({ &field, field, RE::NiPointer<RE::NiFloatInterpolator>{ replacement } });
		}

		void InitializeNightEyeModifiers(RE::TESDataHandler& handler)
		{
			struct ModifierForm
			{
				RE::FormID localId;
				bool activatesNightEye;
			};
			constexpr std::array forms{ ModifierForm{ 0x6B108, true }, ModifierForm{ 0x6B10A, true }, ModifierForm{ 0x6B10B, false } };
			for (const auto& form : forms) {
				auto* modifier = handler.LookupForm<RE::TESImageSpaceModifier>(form.localId, "Skyrim.esm");
				if (!modifier) {
					SKSE::log::warn("Night Eye modifier {:08X} is unavailable; visual detection and blur control skipped.", form.localId);
					continue;
				}
				if (form.activatesNightEye)
					nightEyeModifiers.push_back(modifier);
				for (auto* field : { &modifier->blurRadius, &modifier->radialBlur.strength })
					CacheNativeBlurCurve(*field, modifier->GetFormID());
			}
		}

		bool HasActiveNightEyeModifier()
		{
			auto* world = RE::TES::GetSingleton();
			if (!world)
				return false;
			for (const auto& instance : world->activeImageSpaceModifiers) {
				if (!instance || !std::isfinite(instance->strength) || instance->strength <= 0.0f)
					continue;
				const auto* form = instance->IsForm();
				if (form && std::ranges::find(nightEyeModifiers, form->imod) != nightEyeModifiers.end())
					return true;
			}
			return false;
		}

		void ApplyNativeBlur(bool remove)
		{
			for (auto& curve : nativeBlurCurves) {
				if (remove) {
					if (curve.field->get() == curve.original.get())
						*curve.field = curve.replacement;
				} else if (curve.field->get() == curve.replacement.get()) {
					*curve.field = curve.original;
				}
			}
			nativeBlurApplied.store(remove, std::memory_order_release);
		}

		void RequestNativeBlur(bool remove)
		{
			nativeBlurDesired.store(remove, std::memory_order_release);
			if (nativeBlurApplied.load(std::memory_order_acquire) == remove)
				return;
			if (auto* tasks = SKSE::GetTaskInterface(); tasks && !nativeBlurPending.exchange(true)) {
				tasks->AddTask([] {
					ApplyNativeBlur(nativeBlurDesired.load(std::memory_order_acquire));
					nativeBlurPending.store(false, std::memory_order_release);
				});
			}
		}

		class EffectVisitor : public RE::MagicTarget::ForEachActiveEffectVisitor
		{
		public:
			RE::BSContainer::ForEachResult Accept(RE::ActiveEffect* effect) override
			{
				if (effect && !effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
					const auto* base = effect->GetBaseObject();
					if (base && (std::ranges::find(effects, base) != effects.end() || base->HasArchetype(RE::EffectArchetypes::ArchetypeID::kNightEye))) {
						found = true;
						return RE::BSContainer::ForEachResult::kStop;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			}
			bool found = false;
		};

		void QueryGame(bool actorVision)
		{
			EffectVisitor visitor;
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* ui = RE::UI::GetSingleton();
			const bool loading = ui && (ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME));
			if (player && player->GetParentCell() && !loading) {
				if (auto* target = player->GetMagicTarget())
					target->VisitEffects(visitor);
				visitor.found = visitor.found || HasActiveNightEyeModifier();
			}
			std::shared_ptr<const Affiliations> snapshot;
			if (visitor.found && actorVision) {
				if (auto* lists = RE::ProcessLists::GetSingleton()) {
					auto values = std::make_shared<Affiliations>();
					const auto count = std::min(lists->highActorHandles.size(), kMaximumActorHandles);
					values->reserve(count);
					for (std::uint32_t index = 0; index < count; ++index) {
						auto actor = lists->highActorHandles[index].get();
						if (!actor || actor.get() == player || !actor->Is3DLoaded() || actor->IsDead())
							continue;
						unsigned int affiliation = 0;
						if (actor->IsHostileToActor(player))
							affiliation = 1;
						else if (actor->IsPlayerTeammate() || actor->IsSummonedByPlayer() || actor->GetFactionReaction(player) == RE::FIGHT_REACTION::kAlly)
							affiliation = 2;
						values->emplace(actor->GetFormID(), affiliation);
					}
					snapshot = std::move(values);
				}
			}
			affiliations.store(std::move(snapshot), std::memory_order_release);
			active.store(visitor.found, std::memory_order_release);
			queryPending.store(false, std::memory_order_release);
		}
	}

	void Initialize()
	{
		LoadSettings();
		if (auto* handler = RE::TESDataHandler::GetSingleton()) {
			struct EffectForm
			{
				RE::FormID localId;
				const char* plugin;
			};
			constexpr std::array forms{ EffectForm{ 0x6B10C, "Skyrim.esm" },
				EffectForm{ 0x803, "ccBGSSSE037-curios.esl" },
				EffectForm{ 0xBC4F8, "Ordinator - Perks of Skyrim.esp" } };
			for (const auto& form : forms) {
				if (auto* effect = handler->LookupForm<RE::EffectSetting>(form.localId, form.plugin))
					effects.push_back(effect);
			}
			InitializeNightEyeModifiers(*handler);
		}
		const auto settings = GetSettings();
		const bool removeBlur = settings.enabled && settings.removeNightEyeBlur;
		nativeBlurDesired.store(removeBlur, std::memory_order_release);
		ApplyNativeBlur(removeBlur);
		InitializeRenderer();
	}

	void OnGameLoaded()
	{
		menu::Close();
		active.store(false, std::memory_order_release);
		affiliations.store(nullptr, std::memory_order_release);
	}

	void BeginFrame()
	{
		const float frameDelta = RE::GetSecondsSinceLastFrame();
		const float delta = std::isfinite(frameDelta) ? std::clamp(frameDelta, 0.0f, kMaximumFrameStep) : 0.0f;
		frameSettings = GetSettings();
		RequestNativeBlur(frameSettings.enabled && frameSettings.removeNightEyeBlur);
		auto* ui = RE::UI::GetSingleton();
		const bool blocked = ui && (ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
									   ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || ui->IsMenuOpen(RE::StatsMenu::MENU_NAME));
		if (!frameSettings.enabled || blocked) {
			activation = 0.0f;
			frameAffiliations.reset();
			queryElapsed = kQueryInterval;
			return;
		}
		elapsed += delta;
		queryElapsed += delta;
		if (queryElapsed >= kQueryInterval) {
			if (auto* tasks = SKSE::GetTaskInterface(); tasks && !queryPending.exchange(true)) {
				queryElapsed = 0.0f;
				const bool actorVision = frameSettings.visionMode != 0 && frameSettings.actorVisionAmount > 0.0f;
				tasks->AddTask([actorVision] { QueryGame(actorVision); });
			}
		}
		const float target = active.load(std::memory_order_acquire) ? 1.0f : 0.0f;
		const float step = frameSettings.transitionSeconds > 0.0f ? delta / frameSettings.transitionSeconds : 1.0f;
		activation += std::clamp(target - activation, -step, step);
		auto snapshot = affiliations.load(std::memory_order_acquire);
		if (snapshot || activation <= 0.0f)
			frameAffiliations = std::move(snapshot);
	}

	float GetActivation() { return activation; }
	float GetElapsed() { return elapsed; }
	bool IsEffectActive() { return active.load(std::memory_order_acquire); }

	GeometryData GetGeometryData(RE::BSRenderPass* pass)
	{
		const auto& settings = frameSettings;
		GeometryData result{
			{ settings.eyeshineColor[0], settings.eyeshineColor[1], settings.eyeshineColor[2], activation * settings.eyeshineAmount },
			{ static_cast<float>(settings.visionMode), activation * settings.actorVisionAmount, settings.actorVisionBrightness, 0.0f },
			{ settings.enemyColor[0], settings.enemyColor[1], settings.enemyColor[2], 0.0f },
			{ settings.allyColor[0], settings.allyColor[1], settings.allyColor[2], 0.0f },
			{ settings.neutralColor[0], settings.neutralColor[1], settings.neutralColor[2], 0.0f }, {}
		};
		if (!pass || !pass->geometry || activation <= 0.0f)
			return result;
		if (frameAffiliations) {
			const auto* reference = pass->geometry->GetUserData();
			const auto* actor = reference ? reference->As<RE::Actor>() : nullptr;
			if (actor) {
				if (const auto found = frameAffiliations->find(actor->GetFormID()); found != frameAffiliations->end()) {
					result.Flags[2] = 1;
					result.Actor[3] = static_cast<float>(found->second);
				}
			}
		}
		return result;
	}
}
