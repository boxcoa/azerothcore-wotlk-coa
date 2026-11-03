// mod-coa-challenges: CoA.Challenges.Curses.cpp
// "Hardcore - Cursed!" (617 / 618). The trial aura 943601 ticks every five
// seconds into a dummy spell nothing handled, so the trial never cursed
// anyone. This drives it: a minor and a major curse slot, each on its own
// cooldown, major curses announced five seconds before they land. The table,
// weights and timings live in CoAChallengeCurses.h (unit tested).
#include "CoA.Challenges.Review.h"
#include "CoAChallengeCurses.h"
#include "Random.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include <limits>

namespace CoAChallenges
{
    namespace
    {
        constexpr char const* CURSE_DEATH_PREFIX = "Cursed: ";

        // Curses whose own damage can kill the player.
        constexpr uint32 SELF_DAMAGING_CURSES[] =
        {
            901823, 901824, 901825, 901827, 901845, 901852, 901853, 901854,
            CoACurses::DOOM, CoACurses::STARVATION, 901876
        };

        CoACurses::Context ContextFor(Player* player)
        {
            CoACurses::Context context;
            context.level = uint8(player->GetLevel());
            context.inCombat = player->IsInCombat();
            context.inInstance = player->GetMap() && player->GetMap()->Instanceable();
            context.swimming = player->IsInWater();
            context.airborne = player->IsFlying() || player->IsFalling();
            return context;
        }

        CoACurses::Pause PauseFor(Player* player)
        {
            CoACurses::Pause pause;
            pause.dead = !player->IsAlive();
            pause.onTaxi = player->IsInFlight();
            pause.resting = player->HasPlayerFlag(PLAYER_FLAGS_RESTING);
            pause.teleporting = player->IsBeingTeleported() || !player->IsInWorld();
            pause.battleground = player->GetMap() && player->GetMap()->IsBattlegroundOrArena();
            return pause;
        }

        bool HasCurseIn(Player* player, CoACurses::Slot slot)
        {
            for (CoACurses::Curse const& curse : CoACurses::CURSES)
                if (CoACurses::SlotOf(curse.tier) == slot && player->HasAura(curse.spellId))
                    return true;
            return false;
        }

        // Real food and drink are removed by standing up; buffs that merely
        // regenerate health or mana (Grove Instinct, Mana Module, ...) are not.
        bool IsConsuming(Player* player, AuraType type)
        {
            for (AuraEffect const* effect : player->GetAuraEffectsByType(type))
                if (effect->GetSpellInfo()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
                    return true;
            return false;
        }

        std::string CurseLink(CoACurses::Curse const& curse)
        {
            return Acore::StringFormat("|cffa335ee|Hspell:{}|h[{}]|h|r", curse.spellId, curse.name);
        }

        void SendBossEmote(Player* player, std::string const& message)
        {
            if (g_testQuiet || !player->GetSession())
                return;
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, nullptr, message);
            player->GetSession()->SendPacket(&data);
        }

        // Name the curse in the death announcement while one can kill the
        // player; a creature or player kill still overwrites it as usual.
        void UpdateDeathCause(Player* player)
        {
            for (uint32 spellId : SELF_DAMAGING_CURSES)
                if (player->HasAura(spellId))
                    if (CoACurses::Curse const* curse = CoACurses::Find(spellId))
                    {
                        SetDeathCause(player, KillerKind::Mechanic, 0, std::string(CURSE_DEATH_PREFIX) + curse->name);
                        return;
                    }

            std::lock_guard<std::mutex> lock(LastKillerMutex);
            auto it = LastKiller.find(player->GetGUID().GetCounter());
            if (it != LastKiller.end() && it->second.kind == KillerKind::Mechanic &&
                it->second.name.rfind(CURSE_DEATH_PREFIX, 0) == 0)
                LastKiller.erase(it);
        }

        void LandCurse(Player* player, CoACurses::Curse const& curse)
        {
            if (curse.amount)
                player->CastCustomSpell(curse.spellId, SPELLVALUE_BASE_POINT0, curse.amount, player, true);
            else
                player->CastSpell(player, curse.spellId, true);
            UpdateDeathCause(player);

            if (CoACurses::IsMajor(curse))
                SendBossEmote(player, Acore::StringFormat("The curse takes hold: {}!", CurseLink(curse)));
            else
                NotifyPlayer(player, "The curse twists your fate: {}.", CurseLink(curse));
            LOG_DEBUG("module.coa_challenges", "Cursed: {} afflicted by {} ({})",
                player->GetName(), curse.name, curse.spellId);
        }

        void WarnCurse(Player* player, CoACurses::Curse const& curse)
        {
            SendBossEmote(player, Acore::StringFormat(
                "The curse stirs... {} will take hold in 5 seconds!", CurseLink(curse)));
        }

        void RemoveCurses(Player* player)
        {
            for (CoACurses::Curse const& curse : CoACurses::CURSES)
                player->RemoveAurasDueToSpell(curse.spellId);
            for (uint32 spellId : CoACurses::CURSE_SIDE_EFFECTS)
                player->RemoveAurasDueToSpell(spellId);
        }
    }

    // 943601 "Cursed!": the trial's own aura, the curse director.
    class aura_coa_cursed_trial : public AuraScript
    {
        PrepareAuraScript(aura_coa_cursed_trial);

        uint32 _minorIn = 0;
        uint32 _majorIn = 0;
        uint32 _pendingMajor = 0;
        int32 _doomRemaining = 0;
        bool _prestige = false;
        bool _started = false;

        void HandleTick(AuraEffect const* /*effect*/)
        {
            PreventDefaultAction();
            Player* player = GetUnitOwner() ? GetUnitOwner()->ToPlayer() : nullptr;
            if (!player || !player->GetSession() || player->GetSession()->PlayerLogout())
                return;

            UpdateDeathCause(player);
            AnnounceDoom(player);
            EndCursesByEatingAndDrinking(player);

            if (CoACurses::IsPaused(PauseFor(player)))
            {
                if (_pendingMajor)
                {
                    _pendingMajor = 0;
                    _majorIn = CoACurses::MAJOR_RETRY_MS;
                }
                return;
            }

            CoACurses::Context const context = ContextFor(player);
            if (!_started)
            {
                _started = true;
                _prestige = ActiveChallenges(player->GetGUID().GetCounter())
                    .contains(CoACurses::PRESTIGE_CURSED_CHALLENGE);
                _minorIn = CoACurses::Cooldown(CoACurses::Slot::Minor, context.level, _prestige, rand32());
                _majorIn = CoACurses::Cooldown(CoACurses::Slot::Major, context.level, _prestige, rand32());
                return;
            }

            UpdateMinor(player, context);
            UpdateMajor(player, context);
        }

        void UpdateMinor(Player* player, CoACurses::Context const& context)
        {
            if (HasCurseIn(player, CoACurses::Slot::Minor))
            {
                _minorIn = std::max(_minorIn, CoACurses::MINOR_GAP_MS);
                return;
            }
            if (_minorIn > CoACurses::TICK_MS)
            {
                _minorIn -= CoACurses::TICK_MS;
                return;
            }
            CoACurses::Curse const* curse = CoACurses::Pick(CoACurses::Slot::Minor, context, rand32());
            if (!curse)
            {
                _minorIn = CoACurses::MINOR_GAP_MS;
                return;
            }
            LandCurse(player, *curse);
            _minorIn = CoACurses::Cooldown(CoACurses::Slot::Minor, context.level, _prestige, rand32());
        }

        void UpdateMajor(Player* player, CoACurses::Context const& context)
        {
            if (!CoACurses::MajorsUnlocked(context.level))
                return;

            if (_pendingMajor)
            {
                CoACurses::Curse const* curse = CoACurses::Find(_pendingMajor);
                _pendingMajor = 0;
                if (curse && CoACurses::Allows(*curse, context) && !HasCurseIn(player, CoACurses::Slot::Major))
                {
                    LandCurse(player, *curse);
                    _majorIn = CoACurses::Cooldown(CoACurses::Slot::Major, context.level, _prestige, rand32());
                }
                else
                {
                    NotifyPlayer(player, "The curse loses its grip... for now.");
                    _majorIn = CoACurses::MAJOR_RETRY_MS;
                }
                return;
            }

            if (HasCurseIn(player, CoACurses::Slot::Major))
            {
                _majorIn = std::max(_majorIn, CoACurses::MAJOR_GAP_MS);
                return;
            }
            if (_majorIn > CoACurses::TICK_MS)
            {
                _majorIn -= CoACurses::TICK_MS;
                return;
            }
            CoACurses::Curse const* curse = CoACurses::Pick(CoACurses::Slot::Major, context, rand32());
            if (!curse)
            {
                _majorIn = CoACurses::MAJOR_RETRY_MS;
                return;
            }
            _pendingMajor = curse->spellId;
            WarnCurse(player, *curse);
        }

        void AnnounceDoom(Player* player)
        {
            Aura* doom = player->GetAura(CoACurses::DOOM);
            int32 const remaining = doom ? doom->GetDuration() : 0;
            if (uint32 mark = CoACurses::CountdownMark(_doomRemaining, remaining))
                SendBossEmote(player, Acore::StringFormat("Curse of Doom: {} seconds!", mark));
            _doomRemaining = remaining;
        }

        void EndCursesByEatingAndDrinking(Player* player)
        {
            if (player->HasAura(CoACurses::STARVATION) && IsConsuming(player, SPELL_AURA_MOD_REGEN))
            {
                player->RemoveAurasDueToSpell(CoACurses::STARVATION);
                NotifyPlayer(player, "Your hunger is sated. The Curse of Starvation fades.");
            }
            if (player->HasAura(CoACurses::DEHYDRATION) && IsConsuming(player, SPELL_AURA_MOD_POWER_REGEN))
            {
                player->RemoveAurasDueToSpell(CoACurses::DEHYDRATION);
                NotifyPlayer(player, "Your thirst is quenched. The Curse of Dehydration fades.");
            }
        }

        void HandleRemove(AuraEffect const* /*effect*/, AuraEffectHandleModes /*mode*/)
        {
            Player* player = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
            if (!player || !player->GetSession() || player->GetSession()->PlayerLogout())
                return;
            if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEATH)
                return;
            RemoveCurses(player);
            UpdateDeathCause(player);
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(aura_coa_cursed_trial::HandleTick, EFFECT_0,
                SPELL_AURA_PERIODIC_TRIGGER_SPELL);
            AfterEffectRemove += AuraEffectRemoveFn(aura_coa_cursed_trial::HandleRemove, EFFECT_0,
                SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        }
    };

    // 901809 Curse of Sickness, 901816 Curse of Clumsiness: every five seconds
    // a chance to throw up (stun) or trip (stun and dismount). Their own
    // trigger spells use Ascension's unimplemented "trigger random spell".
    class aura_coa_curse_affliction : public AuraScript
    {
        PrepareAuraScript(aura_coa_curse_affliction);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ CoACurses::SICKNESS_VOMIT, CoACurses::CLUMSINESS_TRIP });
        }

        void HandleTick(AuraEffect const* /*effect*/)
        {
            PreventDefaultAction();
            Unit* target = GetTarget();
            if (!target || !target->IsAlive() || !roll_chance_i(CoACurses::AFFLICTION_CHANCE_PCT))
                return;
            uint32 const outcome = GetId() == CoACurses::SICKNESS ? CoACurses::SICKNESS_VOMIT
                : CoACurses::CLUMSINESS_TRIP;
            target->CastSpell(target, outcome, true);
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(aura_coa_curse_affliction::HandleTick, EFFECT_0,
                SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        }
    };

    // 901830 Curse of Chaos: the archived data rolls between straight-up
    // launches and long sideways shoves every half second. Only the upward
    // launches are kept (no fall damage, no shoving off cliffs), at most two
    // per curse.
    class aura_coa_curse_chaos : public AuraScript
    {
        PrepareAuraScript(aura_coa_curse_chaos);

        uint32 _launches = 0;

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ CoACurses::CHAOS_HOP, CoACurses::CHAOS_LAUNCH });
        }

        void HandleTick(AuraEffect const* /*effect*/)
        {
            PreventDefaultAction();
            Unit* target = GetTarget();
            if (!target || !target->IsAlive() || _launches >= CoACurses::CHAOS_LAUNCH_LIMIT ||
                target->IsInWater() || target->IsFlying() || target->IsFalling() ||
                !roll_chance_i(CoACurses::CHAOS_CHANCE_PCT))
                return;
            ++_launches;
            target->CastSpell(target, urand(0, 1) ? CoACurses::CHAOS_LAUNCH : CoACurses::CHAOS_HOP, true);
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(aura_coa_curse_chaos::HandleTick, EFFECT_0,
                SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        }
    };

    // 901868 Curse of Whispers: every twenty seconds, a vision.
    class aura_coa_curse_whispers : public AuraScript
    {
        PrepareAuraScript(aura_coa_curse_whispers);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ CoACurses::WHISPERS_VISION });
        }

        void HandleTick(AuraEffect const* /*effect*/)
        {
            PreventDefaultAction();
            if (Unit* target = GetTarget())
                target->CastSpell(target, CoACurses::WHISPERS_VISION, true);
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(aura_coa_curse_whispers::HandleTick, EFFECT_0,
                SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        }
    };

    // 901864 Curse of Recall: after ten seconds, sent home to the hearthstone
    // location. Only in the open world; never mid-flight.
    class aura_coa_curse_recall : public AuraScript
    {
        PrepareAuraScript(aura_coa_curse_recall);

        void HandleTick(AuraEffect const* /*effect*/)
        {
            PreventDefaultAction();
            Player* player = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
            if (!player || !player->IsAlive() || player->IsInFlight() || player->IsBeingTeleported() ||
                !player->GetMap() || player->GetMap()->Instanceable())
                return;
            player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY,
                player->m_homebindZ, player->GetOrientation());
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(aura_coa_curse_recall::HandleTick, EFFECT_0,
                SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        }
    };

    // 901852 / 901853 / 901854 Curse of Major / Medium / Minor Reflection:
    // 75 / 50 / 25 percent of the damage the player deals is dealt to them.
    class aura_coa_curse_reflection : public AuraScript
    {
        PrepareAuraScript(aura_coa_curse_reflection);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ CoACurses::REFLECTION_DAMAGE });
        }

        bool CheckProc(AuraEffect const* effect, ProcEventInfo& eventInfo)
        {
            Unit* owner = GetTarget();
            DamageInfo* damage = eventInfo.GetDamageInfo();
            SpellInfo const* spell = eventInfo.GetSpellInfo();
            return owner && damage && damage->GetDamage() && effect->GetAmount() > 0 &&
                eventInfo.GetActor() == owner && eventInfo.GetActionTarget() &&
                eventInfo.GetActionTarget() != owner && (!spell || spell->Id != CoACurses::REFLECTION_DAMAGE);
        }

        void HandleProc(AuraEffect const* effect, ProcEventInfo& eventInfo)
        {
            PreventDefaultAction();
            Unit* owner = GetTarget();
            int32 const amount = int32(std::min<uint64>(std::numeric_limits<int32>::max(),
                uint64(eventInfo.GetDamageInfo()->GetDamage()) * uint32(effect->GetAmount()) / 100));
            if (amount > 0)
                owner->CastCustomSpell(CoACurses::REFLECTION_DAMAGE, SPELLVALUE_BASE_POINT0, amount, owner, true,
                    nullptr, effect);
        }

        void Register() override
        {
            DoCheckEffectProc += AuraCheckEffectProcFn(aura_coa_curse_reflection::CheckProc, EFFECT_0,
                SPELL_AURA_ASCENSION_PROC_PERCENT_EVENT);
            OnEffectProc += AuraEffectProcFn(aura_coa_curse_reflection::HandleProc, EFFECT_0,
                SPELL_AURA_ASCENSION_PROC_PERCENT_EVENT);
        }
    };

    void AddCurseScripts()
    {
        RegisterSpellScript(aura_coa_cursed_trial);
        RegisterSpellScript(aura_coa_curse_affliction);
        RegisterSpellScript(aura_coa_curse_chaos);
        RegisterSpellScript(aura_coa_curse_whispers);
        RegisterSpellScript(aura_coa_curse_recall);
        RegisterSpellScript(aura_coa_curse_reflection);
    }
}
