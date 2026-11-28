#include "CoA.Challenges.Review.h"
#include "CoAChallengeCurses.h"
#include "GameTime.h"
#include "Random.h"

namespace CoAChallenges
{
    using Acore::ChatCommands::PlayerIdentifier;

    namespace
    {
        constexpr uint32 CURSED_TRIAL_AURA = 943601;
        constexpr uint32 CURSED_CHALLENGE = 617;
        constexpr uint32 CURSED_PRESTIGE_CHALLENGE = 618;
        constexpr uint32 REFLECTION_DAMAGE_SPELL = 901856;
        constexpr uint32 WHISPERS_PICK = 901869;
        constexpr uint32 WHISPERS_VISION = 901860;
        constexpr uint32 SICKNESS_PICK = 901812;
        constexpr std::array<uint32, 3> SICKNESS_HELPERS = { 901810, 901811, 901813 };
        constexpr uint32 CLUMSINESS_PICK = 901817;
        constexpr std::array<uint32, 3> CLUMSINESS_HELPERS = { 901818, 901819, 901820 };
        constexpr std::array<std::pair<uint32, uint32>, 3> REFLECTION_SHARES = {{
            { 901852, 75 },
            { 901853, 50 },
            { 901854, 25 },
        }};

        enum class PresenceOverride : uint8
        {
            Auto,
            Present,
            Absent
        };

        struct CursedState
        {
            std::array<CoACurses::LaneClock, 2> lanes;
            std::array<uint32, 2> forced = { 0, 0 };
            CoACurses::ProbeTracker probes;
            PresenceOverride presence = PresenceOverride::Auto;
            bool initialised = false;
            bool loginReady = true;
            bool wasPresent = true;
            bool absentForDamage = false;
            WorldSession const* session = nullptr;
            uint32 mapId = 0;
            uint32 instanceId = 0;
            std::vector<int32> reflections;
        };

        struct CurseAction
        {
            CoACurses::Lane lane;
            CoACurses::Event event;
            CoACurses::Curse const* curse;
            uint32 remainingMs;
        };

        struct ProofTarget
        {
            uint32 guid = 0;
            CoACurses::Lane lane = CoACurses::Lane::Minor;
            bool active = false;
        };

        std::mutex CursesMutex;
        std::unordered_map<uint32, CursedState> CursesStates;
        std::atomic<size_t> CursesStateCount = 0;
        thread_local ProofTarget t_proofTarget;

        thread_local CoACurses::HitNotes t_hitNotes;

        size_t LaneIndex(CoACurses::Lane lane)
        {
            return lane == CoACurses::Lane::Minor ? 0 : 1;
        }

        CoACurses::Lane LaneAt(size_t index)
        {
            return index == 0 ? CoACurses::Lane::Minor : CoACurses::Lane::Major;
        }

        uint64 NowMs()
        {
            return uint64(GameTime::GetGameTimeMS().count());
        }

        uint64 LiveMs()
        {
            return uint64(std::chrono::duration_cast<Milliseconds>(GameTime::SteadyNow().time_since_epoch()).count());
        }

        uint32 RandomGapMs()
        {
            return urand(CoACurses::GAP_MIN_MS, CoACurses::GAP_MAX_MS);
        }

        bool IsCursedRun(Player* player)
        {
            if (!player->HasAura(CURSED_TRIAL_AURA))
                return false;
            std::set<uint32> const active = ActiveChallenges(player->GetGUID().GetCounter());
            return active.count(CURSED_CHALLENGE) || active.count(CURSED_PRESTIGE_CHALLENGE);
        }

        bool HasOwnedFamilyAura(Player* player, CoACurses::Curse const& curse)
        {
            if (player->GetAura(curse.spell, player->GetGUID()))
                return true;
            for (uint32 helper : curse.helpers)
                if (helper && player->GetAura(helper, player->GetGUID()))
                    return true;
            return false;
        }

        uint8 LaneDamage(CoACurses::LaneClock const& clock)
        {
            if (clock.phase == CoACurses::Phase::Gap || !clock.curse)
                return 0;
            return clock.curse->damagePct;
        }

        void InitialiseLanes(Player* player, CursedState& state)
        {
            for (size_t index = 0; index < state.lanes.size(); ++index)
                state.lanes[index].StartGap(RandomGapMs());
            for (int pass = 0; pass < 3; ++pass)
            {
                for (CoACurses::Curse const& curse : CoACurses::Pool())
                {
                    CoACurses::LaneClock& clock = state.lanes[LaneIndex(CoACurses::LaneOf(curse.tier))];
                    if (clock.phase != CoACurses::Phase::Gap)
                        continue;
                    if (pass == 0 && !curse.helpers[0])
                        continue;
                    bool const owned = pass < 2 ? player->GetAura(curse.spell, player->GetGUID()) != nullptr
                        : HasOwnedFamilyAura(player, curse);
                    if (owned)
                    {
                        clock.phase = CoACurses::Phase::Active;
                        clock.curse = &curse;
                    }
                }
            }
            state.initialised = true;
        }

        CoACurses::Curse const* RollCurse(Player* player, CursedState& state, size_t index)
        {
            CoACurses::Lane const lane = LaneAt(index);
            uint8 const otherDamage = LaneDamage(state.lanes[1 - index]);
            uint8 const level = player->GetLevel();

            if (uint32 const forced = state.forced[index])
            {
                state.forced[index] = 0;
                CoACurses::Curse const* curse = CoACurses::FindCurse(forced);
                if (curse && CoACurses::LaneOf(curse->tier) == lane && CoACurses::TierWeight(level, curse->tier)
                    && CoACurses::CapAllows(*curse, otherDamage))
                    return curse;
                if (curse && CoACurses::LaneOf(curse->tier) == lane && CoACurses::TierWeight(level, curse->tier))
                    return CoACurses::PickCurse(curse->tier, otherDamage, urand(0, 0xFFFF));
            }

            std::optional<CoACurses::Tier> const tier = CoACurses::PickTier(level, lane, urand(0, 0xFFFF));
            if (!tier)
                return nullptr;
            return CoACurses::PickCurse(*tier, otherDamage, urand(0, 0xFFFF));
        }

        bool AbsentForDamageNow(Player const* player, CursedState const& state)
        {
            if (state.presence == PresenceOverride::Present)
                return false;
            if (state.presence == PresenceOverride::Absent)
                return true;
            CoACurses::PresenceInput in;
            in.farTeleport = player->IsBeingTeleportedFar();
            in.oldestUnansweredMs = state.probes.OldestUnansweredMs(NowMs());
            return CoACurses::DamageSuppressed(in);
        }

        CoACurses::PresenceInput ReadPresence(Player* player, CursedState const& state)
        {
            CoACurses::PresenceInput in;
            in.inWorld = player->IsInWorld() && player->FindMap();
            in.farTeleport = player->IsBeingTeleportedFar();
            in.taxi = player->IsInFlight();
            in.vehicle = player->GetVehicle() != nullptr;
            in.loginReady = state.loginReady;
            in.oldestUnansweredMs = state.probes.OldestUnansweredMs(NowMs());
            if (state.presence == PresenceOverride::Present)
            {
                in.loginReady = true;
                in.oldestUnansweredMs = 0;
            }
            else if (state.presence == PresenceOverride::Absent)
                in.oldestUnansweredMs = CoACurses::SILENCE_MS;
            return in;
        }

        void Append(std::vector<CurseAction>& actions, size_t index, std::vector<CoACurses::Event> const& events,
            CoACurses::LaneClock const& clock)
        {
            for (CoACurses::Event event : events)
                if (event != CoACurses::Event::Roll && event != CoACurses::Event::None)
                    actions.push_back({ LaneAt(index), event, clock.curse, clock.remainingMs });
        }

        void NoteSession(CursedState& state, WorldSession const* session)
        {
            if (state.session == session)
                return;
            bool const known = state.session != nullptr;
            state.session = session;
            if (!known)
                return;
            state.loginReady = false;
            state.wasPresent = false;
        }

        uint64 ReceivedMs(WorldPacket const& packet)
        {
            uint64 const now = NowMs();
            TimePoint const received = packet.GetReceivedTime();
            if (received == TimePoint() || received >= GameTime::Now())
                return now;
            uint64 const age = uint64(std::chrono::duration_cast<Milliseconds>(GameTime::Now() - received).count());
            return now > age ? now - age : 0;
        }

        void ApplyReply(CursedState& state, uint32 counter, uint64 receivedMs)
        {
            CoACurses::ReplyResult const reply = state.probes.OnReplyReceived(counter, receivedMs);
            if (state.presence == PresenceOverride::Auto && reply.late)
                state.wasPresent = false;
            for (CoACurses::LaneClock& clock : state.lanes)
                clock.OnProofReply(counter);
            state.loginReady = true;
        }

        char const* TierColour(CoACurses::Tier tier)
        {
            switch (tier)
            {
                case CoACurses::Tier::Nuisance:
                    return "1eff00";
                case CoACurses::Tier::Hindrance:
                    return "ffff00";
                case CoACurses::Tier::Control:
                    return "ff8000";
                case CoACurses::Tier::Lethal:
                    return "ff0000";
            }
            return "ff0000";
        }

        char const* TierLabel(CoACurses::Tier tier)
        {
            switch (tier)
            {
                case CoACurses::Tier::Nuisance:
                    return "Nuisance";
                case CoACurses::Tier::Hindrance:
                    return "Hindrance";
                case CoACurses::Tier::Control:
                    return "Control";
                case CoACurses::Tier::Lethal:
                    return "Lethal";
            }
            return "Lethal";
        }

        void SendCurseMessage(Player* player, CoACurses::Curse const& curse, uint32 remainingMs, bool reminder)
        {
            if (g_testQuiet || !player->GetSession())
                return;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(curse.spell);
            if (!info || !info->SpellName[0])
                return;

            static char const* const ANNOUNCE_LINES[] = {
                "The curse stirs... {} {} strikes in {} seconds!",
                "Fate twists against you: {} {} lands in {} seconds!",
                "A shadow gathers around you. {} {} will strike in {} seconds!",
            };
            static char const* const REMINDER_LINES[] = {
                "{} {} strikes in {} seconds!",
                "Brace yourself! {} {} lands in {} seconds!",
                "The curse is upon you: {} {} in {} seconds!",
            };

            std::string const colour = TierColour(curse.tier);
            std::string const label = "|cff" + colour + "[" + TierLabel(curse.tier) + "]|r";
            std::string const link = "|cff" + colour + "|Hspell:" + std::to_string(curse.spell) + "|h["
                + info->SpellName[0] + "]|h|r";
            uint32 const seconds = (remainingMs + 999) / 1000;
            char const* const line = reminder ? REMINDER_LINES[urand(0, 2)] : ANNOUNCE_LINES[urand(0, 2)];
            std::string const message = Acore::StringFormat(line, label, link, seconds);

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, nullptr, message);
            player->GetSession()->SendPacket(&data);
        }

        void RequestProof(Player* player, CoACurses::Lane lane)
        {
            WorldSession* session = player->GetSession();
            if (!session)
                return;
            t_proofTarget = { player->GetGUID().GetCounter(), lane, true };
            session->SendTimeSync();
            t_proofTarget.active = false;
        }

        void Execute(Player* player, std::vector<CurseAction> const& actions)
        {
            for (CurseAction const& action : actions)
            {
                if (!action.curse)
                    continue;
                switch (action.event)
                {
                    case CoACurses::Event::Announce:
                        SendCurseMessage(player, *action.curse, action.remainingMs, false);
                        break;
                    case CoACurses::Event::Remind:
                        SendCurseMessage(player, *action.curse, CoACurses::REMINDER_LEAD_MS, true);
                        break;
                    case CoACurses::Event::RequestProof:
                        RequestProof(player, action.lane);
                        break;
                    case CoACurses::Event::Cast:
                        LOG_INFO("module.coa_challenges", "Cursed: {} receives spell {}", player->GetName(),
                            action.curse->spell);
                        player->CastSpell(player, action.curse->spell, true);
                        break;
                    default:
                        break;
                }
            }
        }

        bool IsCurseFamilySpell(uint32 spell)
        {
            for (CoACurses::Curse const& curse : CoACurses::Pool())
                if (CoACurses::InFamily(curse, spell))
                    return true;
            return false;
        }

        bool SuppressCurseDamage(Unit* target, Unit* attacker, SpellInfo const* spellInfo)
        {
            if (!target || target != attacker || !spellInfo || !target->IsPlayer())
                return false;
            if (CursesStateCount.load(std::memory_order_relaxed) == 0 || !IsCurseFamilySpell(spellInfo->Id))
                return false;
            return CursesPlayerAbsent(target->ToPlayer());
        }

        void DealReflection(Player* player, int32 amount)
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(REFLECTION_DAMAGE_SPELL);
            if (!info || amount <= 0 || !player->IsAlive())
                return;
            if (player->IsImmunedToDamage(player, info))
                return;
            SpellNonMeleeDamage damage(player, player, info, info->GetSchoolMask());
            damage.damage = uint32(amount);
            DamageInfo mitigation(damage, SPELL_DIRECT_DAMAGE, BASE_ATTACK, 0);
            Unit::CalcAbsorbResist(mitigation);
            damage.absorb = mitigation.GetAbsorb();
            damage.resist = mitigation.GetResist();
            damage.damage = mitigation.GetDamage();
            Unit::DealDamageMods(player, damage.damage, &damage.absorb);
            player->SendSpellNonMeleeDamageLog(&damage);
            player->DealSpellDamage(&damage, true);
        }

        uint32 DamageAfterDuelFloor(Player const* attacker, Unit const* victim, uint32 damage)
        {
            uint32 const health = victim->GetHealth();
            if (!health || !damage)
                return 0;
            Player const* duelist = victim->ToPlayer();
            bool const vehicle = !duelist && victim->IsVehicle() && victim->GetCharmer();
            if (vehicle)
                duelist = victim->GetCharmer()->ToPlayer();
            if (duelist && duelist->duel && (!vehicle || duelist->duel->IsMounted)
                && duelist->duel->Opponent == attacker && damage >= health - 1)
                return health - 1;
            return std::min(damage, health);
        }

        CursedState* FindState(Player* player)
        {
            auto it = CursesStates.find(player->GetGUID().GetCounter());
            return it == CursesStates.end() ? nullptr : &it->second;
        }
    }

    void CursesUpdate(Player* player, uint32 diff)
    {
        uint32 const guid = player->GetGUID().GetCounter();
        if (!IsCursedRun(player))
        {
            if (CursesStateCount.load(std::memory_order_relaxed) == 0)
                return;
            std::lock_guard<std::mutex> lock(CursesMutex);
            CursesStates.erase(guid);
            CursesStateCount.store(CursesStates.size(), std::memory_order_relaxed);
            return;
        }

        std::vector<CurseAction> actions;
        std::vector<int32> reflections;
        {
            std::lock_guard<std::mutex> lock(CursesMutex);
            CursedState& state = CursesStates[guid];
            CursesStateCount.store(CursesStates.size(), std::memory_order_relaxed);
            if (!state.initialised)
            {
                InitialiseLanes(player, state);
                state.mapId = player->GetMapId();
                state.instanceId = player->GetInstanceId();
            }
            NoteSession(state, player->GetSession());
            if (state.mapId != player->GetMapId() || state.instanceId != player->GetInstanceId())
            {
                state.mapId = player->GetMapId();
                state.instanceId = player->GetInstanceId();
                state.wasPresent = false;
            }

            CoACurses::PresenceInput const presence = ReadPresence(player, state);
            bool const present = CoACurses::CountdownRuns(presence);
            state.absentForDamage = CoACurses::DamageSuppressed(presence);

            for (size_t index = 0; index < state.lanes.size(); ++index)
            {
                CoACurses::LaneClock& clock = state.lanes[index];
                if (present && !state.wasPresent)
                    Append(actions, index, clock.OnReturn(), clock);

                if (clock.phase == CoACurses::Phase::Active)
                {
                    if (!clock.curse || !HasOwnedFamilyAura(player, *clock.curse))
                        clock.OnEnded(RandomGapMs());
                    continue;
                }

                std::vector<CoACurses::Event> const events = clock.Step(diff, present, LiveMs());
                for (CoACurses::Event event : events)
                {
                    if (event != CoACurses::Event::Roll)
                        continue;
                    if (CoACurses::Curse const* curse = RollCurse(player, state, index))
                        Append(actions, index, clock.Begin(curse), clock);
                    else
                        clock.StartGap(RandomGapMs());
                }
                Append(actions, index, events, clock);
            }
            state.wasPresent = present;
            reflections.swap(state.reflections);
            if (state.absentForDamage)
                reflections.clear();
        }
        Execute(player, actions);
        for (int32 amount : reflections)
            DealReflection(player, amount);
    }

    void CursesOnLogin(Player* player)
    {
        std::lock_guard<std::mutex> lock(CursesMutex);
        CursedState& state = CursesStates[player->GetGUID().GetCounter()];
        CursesStateCount.store(CursesStates.size(), std::memory_order_relaxed);
        state.loginReady = false;
        state.wasPresent = false;
        state.session = player->GetSession();
        state.probes.OnRequestSent(0, NowMs());
    }

    void CursesOnPacketSent(WorldSession* session, WorldPacket const& packet)
    {
        if (packet.GetOpcode() != SMSG_TIME_SYNC_REQ || packet.size() < 4 || !session || !session->GetPlayer())
            return;
        uint32 const guid = session->GetPlayer()->GetGUID().GetCounter();
        uint32 const counter = packet.read<uint32>(0);
        std::lock_guard<std::mutex> lock(CursesMutex);
        auto it = CursesStates.find(guid);
        if (it == CursesStates.end())
            return;
        NoteSession(it->second, session);
        it->second.probes.OnRequestSent(counter, NowMs());
        if (t_proofTarget.active && t_proofTarget.guid == guid)
        {
            CoACurses::LaneClock& clock = it->second.lanes[LaneIndex(t_proofTarget.lane)];
            clock.OnProofSent(counter, LiveMs());
            clock.proofSequence = it->second.probes.sequence;
        }
    }

    void CursesOnPacketReceived(WorldSession* session, WorldPacket const& packet)
    {
        uint16 const opcode = packet.GetOpcode();
        if (opcode != CMSG_TIME_SYNC_RESP && opcode != CMSG_SET_ACTIVE_MOVER)
            return;
        if (!session || !session->GetPlayer())
            return;
        std::lock_guard<std::mutex> lock(CursesMutex);
        auto it = CursesStates.find(session->GetPlayer()->GetGUID().GetCounter());
        if (it == CursesStates.end())
            return;
        NoteSession(it->second, session);
        if (opcode == CMSG_SET_ACTIVE_MOVER)
            it->second.loginReady = true;
        else if (packet.size() >= 4)
            ApplyReply(it->second, packet.read<uint32>(0), ReceivedMs(packet));
    }

    bool CursesPlayerAbsent(Player const* player)
    {
        std::lock_guard<std::mutex> lock(CursesMutex);
        auto it = CursesStates.find(player->GetGUID().GetCounter());
        return it != CursesStates.end() && AbsentForDamageNow(player, it->second);
    }

    void CursesModifyDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo)
    {
        if (damage > 0 && SuppressCurseDamage(target, attacker, spellInfo))
            damage = 0;
    }

    void CursesModifyPeriodicTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
    {
        if (damage > 0 && SuppressCurseDamage(target, attacker, spellInfo))
            damage = 0;
    }

    void CursesOnDamageDealt(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType type)
    {
        uint32 share = 0;
        if (damage && attacker && victim && attacker != victim && attacker->IsPlayer()
            && (type == DIRECT_DAMAGE || type == SPELL_DIRECT_DAMAGE)
            && CursesStateCount.load(std::memory_order_relaxed) != 0)
        {
            Player* player = attacker->ToPlayer();
            for (auto const& [spell, pct] : REFLECTION_SHARES)
                if (player->GetAura(spell, player->GetGUID()))
                    share = std::max(share, pct);
        }
        t_hitNotes.Push({ attacker, victim, share });
    }

    void CursesOnDamageApplied(Unit* attacker, Unit* victim, uint32 damage)
    {
        CoACurses::HitNote const hit = t_hitNotes.Pop();
        if (!hit.share || hit.attacker != attacker || hit.victim != victim || !damage)
            return;
        Player* player = attacker->ToPlayer();
        if (Player const* target = victim->ToPlayer(); target && target->GetCommandStatus(CHEAT_GOD))
            return;
        uint32 const dealt = DamageAfterDuelFloor(player, victim, damage);
        int32 const amount = int32(uint64(dealt) * hit.share / 100);
        if (amount <= 0)
            return;
        std::lock_guard<std::mutex> lock(CursesMutex);
        if (CursedState* state = FindState(player))
            state->reflections.push_back(amount);
    }

    void CursesOnSpellCast(Unit* caster, SpellInfo const* spellInfo)
    {
        if (!caster || !spellInfo || !caster->IsPlayer())
            return;
        uint32 helper = 0;
        switch (spellInfo->Id)
        {
            case WHISPERS_PICK:
                helper = WHISPERS_VISION;
                break;
            case SICKNESS_PICK:
                helper = SICKNESS_HELPERS[urand(0, 2)];
                break;
            case CLUMSINESS_PICK:
                helper = CLUMSINESS_HELPERS[urand(0, 2)];
                break;
            default:
                return;
        }
        caster->CastSpell(caster, helper, true);
    }

    bool HandleCoACursedForceCommand(ChatHandler* handler, PlayerIdentifier target, uint32 spell)
    {
        Player* player = target.GetConnectedPlayer();
        CoACurses::Curse const* curse = CoACurses::FindCurse(spell);
        if (!player || !curse)
        {
            handler->SendErrorMessage("Cursed: player '{}' is not online or {} is not a pool curse.",
                target.GetName(), spell);
            return false;
        }
        size_t const index = LaneIndex(CoACurses::LaneOf(curse->tier));
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(CursesMutex);
            if (CursedState* state = FindState(player))
            {
                found = true;
                state->forced[index] = spell;
                CoACurses::LaneClock& clock = state->lanes[index];
                if (clock.phase == CoACurses::Phase::Gap)
                    clock.gapLeftMs = 0;
                else if (clock.phase == CoACurses::Phase::Warning)
                    clock.StartGap(0);
            }
        }
        if (!found)
        {
            handler->SendErrorMessage("Cursed: '{}' is not on a Cursed! run.", target.GetName());
            return false;
        }
        handler->PSendSysMessage("Cursed: next {} roll for {} is {}.", index == 0 ? "minor" : "major",
            target.GetName(), spell);
        return true;
    }

    bool HandleCoACursedPresenceCommand(ChatHandler* handler, PlayerIdentifier target, std::string mode)
    {
        Player* player = target.GetConnectedPlayer();
        PresenceOverride wanted;
        if (mode == "auto")
            wanted = PresenceOverride::Auto;
        else if (mode == "present")
            wanted = PresenceOverride::Present;
        else if (mode == "absent")
            wanted = PresenceOverride::Absent;
        else
        {
            handler->SendErrorMessage("Cursed: presence must be auto, present or absent.");
            return false;
        }
        if (!player)
        {
            handler->SendErrorMessage("Cursed: player '{}' is not online.", target.GetName());
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(CursesMutex);
            CursesStates[player->GetGUID().GetCounter()].presence = wanted;
            CursesStateCount.store(CursesStates.size(), std::memory_order_relaxed);
        }
        handler->PSendSysMessage("Cursed: presence for {} is {}.", target.GetName(), mode);
        return true;
    }

    bool HandleCoACursedReplyCommand(ChatHandler* handler, PlayerIdentifier target, std::string which)
    {
        Player* player = target.GetConnectedPlayer();
        if (!player)
        {
            handler->SendErrorMessage("Cursed: player '{}' is not online.", target.GetName());
            return false;
        }
        if (which != "minor" && which != "major" && which != "unrelated")
        {
            handler->SendErrorMessage("Cursed: reply must be minor, major or unrelated.");
            return false;
        }
        bool tracked = false;
        {
            std::lock_guard<std::mutex> lock(CursesMutex);
            tracked = FindState(player) != nullptr;
        }
        if (!tracked)
        {
            handler->SendErrorMessage("Cursed: '{}' is not on a Cursed! run.", target.GetName());
            return false;
        }
        if (which == "unrelated" && player->GetSession())
            player->GetSession()->SendTimeSync();

        bool found = false;
        bool requested = false;
        uint32 counter = 0;
        {
            std::lock_guard<std::mutex> lock(CursesMutex);
            if (CursedState* state = FindState(player))
            {
                found = true;
                if (which == "unrelated")
                {
                    if (!state->probes.outstanding.empty())
                    {
                        counter = state->probes.outstanding.back().first;
                        requested = true;
                        for (CoACurses::LaneClock const& clock : state->lanes)
                            if (clock.proofCounter && *clock.proofCounter == counter)
                                requested = false;
                    }
                }
                else if (CoACurses::LaneClock const& clock = state->lanes[which == "minor" ? 0 : 1];
                    clock.proofCounter && clock.proofSequence == state->probes.sequence)
                {
                    counter = *clock.proofCounter;
                    requested = true;
                }
                if (requested)
                    ApplyReply(*state, counter, NowMs());
            }
        }
        if (!found)
        {
            handler->SendErrorMessage("Cursed: '{}' is not on a Cursed! run.", target.GetName());
            return false;
        }
        if (!requested)
        {
            handler->SendErrorMessage("Cursed: no {} request was sent in the current time-sync sequence.", which);
            return false;
        }
        handler->PSendSysMessage("Cursed: reply {} sent for {}.", counter, target.GetName());
        return true;
    }

    bool HandleCoACursedExpectCommand(ChatHandler* handler, PlayerIdentifier target, std::string lane,
        std::string phase, Optional<std::string> detail)
    {
        Player* player = target.GetConnectedPlayer();
        if (!player || (lane != "minor" && lane != "major"))
        {
            handler->SendErrorMessage("Cursed: expect needs an online player and the minor or major lane.");
            return false;
        }
        CoACurses::Phase wanted;
        if (phase == "gap")
            wanted = CoACurses::Phase::Gap;
        else if (phase == "warning")
            wanted = CoACurses::Phase::Warning;
        else if (phase == "active")
            wanted = CoACurses::Phase::Active;
        else
        {
            handler->SendErrorMessage("Cursed: phase must be gap, warning or active.");
            return false;
        }

        bool found = false;
        CoACurses::Phase actual = CoACurses::Phase::Gap;
        CoACurses::Curse const* curse = nullptr;
        bool requested = false;
        {
            std::lock_guard<std::mutex> lock(CursesMutex);
            if (CursedState* state = FindState(player))
            {
                found = true;
                CoACurses::LaneClock const& clock = state->lanes[lane == "minor" ? 0 : 1];
                actual = clock.phase;
                curse = clock.curse;
                requested = clock.proofCounter && clock.proofSequence == state->probes.sequence;
            }
        }
        if (!found)
        {
            handler->SendErrorMessage("Cursed: '{}' is not on a Cursed! run.", target.GetName());
            return false;
        }

        bool matches = actual == wanted;
        if (matches && detail && (*detail == "requested" || *detail == "unrequested"))
            matches = requested == (*detail == "requested");
        else if (matches && detail)
        {
            static std::array<std::pair<char const*, CoACurses::Tier>, 4> const TIERS = {{
                { "nuisance", CoACurses::Tier::Nuisance },
                { "hindrance", CoACurses::Tier::Hindrance },
                { "control", CoACurses::Tier::Control },
                { "lethal", CoACurses::Tier::Lethal },
            }};
            bool tierDetail = false;
            for (auto const& [name, tier] : TIERS)
                if (*detail == name)
                {
                    tierDetail = true;
                    matches = curse && curse->tier == tier;
                }
            if (!tierDetail)
                matches = curse && std::to_string(curse->spell) == *detail;
        }

        uint32 const spell = curse ? curse->spell : 0;
        if (!matches)
        {
            handler->SendErrorMessage("Cursed: {} lane of {} is phase {} with curse {}, expected {} {}.", lane,
                target.GetName(), uint32(actual), spell, phase, detail ? *detail : "");
            return false;
        }
        handler->PSendSysMessage("Cursed: {} lane of {} is {} with curse {}.", lane, target.GetName(), phase, spell);
        return true;
    }
}
