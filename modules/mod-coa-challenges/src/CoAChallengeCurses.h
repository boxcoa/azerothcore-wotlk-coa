#ifndef MOD_COA_CHALLENGES_CURSES_H
#define MOD_COA_CHALLENGES_CURSES_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace CoACurses
{
    enum class Lane : uint8_t
    {
        Minor,
        Major
    };

    enum class Tier : uint8_t
    {
        Nuisance = 1,
        Hindrance,
        Control,
        Lethal
    };

    struct Curse
    {
        uint32_t spell;
        Tier tier;
        uint8_t damagePct;
        std::array<uint32_t, 4> helpers;
    };

    inline constexpr uint8_t DAMAGE_CAP_PCT = 100;

    inline constexpr std::array<Curse, 45> POOL = {{
        { 901857, Tier::Nuisance, 0, {} },
        { 901858, Tier::Nuisance, 0, {} },
        { 901859, Tier::Nuisance, 0, {} },
        { 901860, Tier::Nuisance, 0, {} },
        { 901861, Tier::Nuisance, 0, {} },
        { 901844, Tier::Nuisance, 0, {} },
        { 901843, Tier::Nuisance, 0, {} },
        { 901840, Tier::Nuisance, 0, {} },
        { 901850, Tier::Nuisance, 0, {} },
        { 901808, Tier::Nuisance, 0, {} },
        { 901872, Tier::Nuisance, 0, { 901873 } },
        { 901868, Tier::Nuisance, 0, { 901869, 901860 } },
        { 901802, Tier::Hindrance, 0, {} },
        { 901801, Tier::Hindrance, 0, {} },
        { 901807, Tier::Hindrance, 0, {} },
        { 901806, Tier::Hindrance, 0, {} },
        { 901829, Tier::Hindrance, 0, {} },
        { 901870, Tier::Hindrance, 0, {} },
        { 901826, Tier::Hindrance, 0, {} },
        { 901827, Tier::Hindrance, 12, { 901828 } },
        { 901877, Tier::Hindrance, 0, { 901878 } },
        { 901862, Tier::Hindrance, 0, {} },
        { 901851, Tier::Hindrance, 0, { 901863 } },
        { 901853, Tier::Hindrance, 0, { 901856 } },
        { 901854, Tier::Hindrance, 0, { 901856 } },
        { 901800, Tier::Control, 0, {} },
        { 901841, Tier::Control, 0, {} },
        { 901842, Tier::Control, 0, {} },
        { 901847, Tier::Control, 0, {} },
        { 901855, Tier::Control, 0, {} },
        { 901848, Tier::Control, 0, { 901849 } },
        { 901864, Tier::Control, 0, { 901865 } },
        { 901905, Tier::Control, 0, {} },
        { 901803, Tier::Control, 0, {} },
        { 901874, Tier::Control, 0, { 901875 } },
        { 901852, Tier::Control, 0, { 901856 } },
        { 901809, Tier::Control, 0, { 901812, 901810, 901811, 901813 } },
        { 901816, Tier::Control, 0, { 901817, 901818, 901819, 901820 } },
        { 901825, Tier::Lethal, 30, {} },
        { 901824, Tier::Lethal, 18, {} },
        { 901823, Tier::Lethal, 15, {} },
        { 901845, Tier::Lethal, 40, { 901846 } },
        { 901876, Tier::Lethal, 70, {} },
        { 901871, Tier::Lethal, 90, {} },
        { 901866, Tier::Lethal, 75, { 901867 } },
    }};

    inline constexpr std::array<std::array<uint32_t, 4>, 4> BAND_WEIGHTS = {{
        { 70, 30, 0, 0 },
        { 50, 35, 15, 0 },
        { 35, 35, 20, 10 },
        { 25, 30, 25, 20 },
    }};

    inline std::span<Curse const> Pool()
    {
        return POOL;
    }

    inline Curse const* FindCurse(uint32_t spell)
    {
        for (Curse const& curse : POOL)
            if (curse.spell == spell)
                return &curse;
        return nullptr;
    }

    inline bool InFamily(Curse const& curse, uint32_t spell)
    {
        if (spell == 0)
            return false;
        if (curse.spell == spell)
            return true;
        for (uint32_t helper : curse.helpers)
            if (helper == spell)
                return true;
        return false;
    }

    inline Lane LaneOf(Tier tier)
    {
        return tier == Tier::Nuisance || tier == Tier::Hindrance ? Lane::Minor : Lane::Major;
    }

    inline uint32_t TierWeight(uint8_t level, Tier tier)
    {
        size_t const band = level < 10 ? 0 : level < 20 ? 1 : level < 40 ? 2 : 3;
        return BAND_WEIGHTS[band][static_cast<size_t>(tier) - 1];
    }

    inline std::array<Tier, 2> LaneTiers(Lane lane)
    {
        if (lane == Lane::Minor)
            return { Tier::Nuisance, Tier::Hindrance };
        return { Tier::Control, Tier::Lethal };
    }

    inline std::optional<Tier> PickTier(uint8_t level, Lane lane, uint32_t roll)
    {
        std::array<Tier, 2> const tiers = LaneTiers(lane);
        uint32_t const total = TierWeight(level, tiers[0]) + TierWeight(level, tiers[1]);
        if (total == 0)
            return std::nullopt;
        uint32_t const point = roll % total;
        return point < TierWeight(level, tiers[0]) ? tiers[0] : tiers[1];
    }

    inline bool CapAllows(Curse const& candidate, uint8_t otherLaneDamagePct)
    {
        return uint32_t(candidate.damagePct) + otherLaneDamagePct < DAMAGE_CAP_PCT;
    }

    inline Curse const* PickCurse(Tier tier, uint8_t otherLaneDamagePct, uint32_t roll)
    {
        std::vector<Curse const*> allowed;
        for (Curse const& curse : POOL)
            if (curse.tier == tier && CapAllows(curse, otherLaneDamagePct))
                allowed.push_back(&curse);
        if (allowed.empty())
            return nullptr;
        return allowed[roll % allowed.size()];
    }

    inline uint32_t WarningMs(Tier tier)
    {
        switch (tier)
        {
            case Tier::Nuisance:
                return 5000;
            case Tier::Hindrance:
                return 10000;
            case Tier::Control:
                return 15000;
            case Tier::Lethal:
                return 20000;
        }
        return 20000;
    }

    inline constexpr uint32_t REMINDER_LEAD_MS = 5000;
    inline constexpr uint32_t FIRST_RETURN_MIN_MS = 10000;
    inline constexpr uint32_t LATER_RETURN_MIN_MS = 5000;
    inline constexpr uint32_t SILENCE_MS = 10000;
    inline constexpr uint32_t GAP_MIN_MS = 30000;
    inline constexpr uint32_t GAP_MAX_MS = 60000;

    enum class Phase : uint8_t
    {
        Gap,
        Warning,
        Active
    };

    enum class Event : uint8_t
    {
        None,
        Roll,
        Announce,
        Remind,
        RequestProof,
        Cast
    };

    struct LaneClock
    {
        Phase phase = Phase::Gap;
        uint32_t gapLeftMs = 0;
        Curse const* curse = nullptr;
        uint32_t remainingMs = 0;
        uint32_t servedMs = 0;
        uint64_t windowStartMs = 0;
        uint32_t windowMs = 0;
        std::optional<uint64_t> proofSentMs;
        bool reminderSent = false;
        bool proven = false;
        bool holdNextStep = false;
        uint32_t returns = 0;
        std::optional<uint32_t> proofCounter;
        uint32_t proofSequence = 0;

        void StartGap(uint32_t gapMs)
        {
            phase = Phase::Gap;
            gapLeftMs = gapMs;
            curse = nullptr;
            ResetWarning();
            returns = 0;
        }

        std::vector<Event> Begin(Curse const* rolled, uint64_t nowMs)
        {
            phase = Phase::Warning;
            curse = rolled;
            remainingMs = WarningMs(rolled->tier);
            windowStartMs = nowMs;
            windowMs = remainingMs;
            servedMs = 0;
            returns = 0;
            ResetWarning();
            return Announcement();
        }

        std::vector<Event> Step(uint32_t diffMs, bool present, uint64_t nowMs)
        {
            std::vector<Event> events;
            if (!present)
                return events;

            if (phase == Phase::Gap)
            {
                if (gapLeftMs > diffMs)
                {
                    gapLeftMs -= diffMs;
                    return events;
                }
                gapLeftMs = 0;
                events.push_back(Event::Roll);
                return events;
            }

            if (phase != Phase::Warning)
                return events;

            if (holdNextStep)
            {
                holdNextStep = false;
                return events;
            }

            bool const reminderWasSent = reminderSent;
            servedMs += diffMs;
            uint64_t const elapsed = nowMs > windowStartMs ? nowMs - windowStartMs : 0;
            remainingMs = elapsed < windowMs ? uint32_t(windowMs - elapsed) : 0;
            if (!reminderWasSent && remainingMs <= REMINDER_LEAD_MS)
            {
                reminderSent = true;
                events.push_back(Event::Remind);
                events.push_back(Event::RequestProof);
            }

            if (reminderWasSent && proven && proofSentMs && nowMs >= *proofSentMs + REMINDER_LEAD_MS)
            {
                phase = Phase::Active;
                events.push_back(Event::Cast);
            }
            return events;
        }

        void OnProofSent(uint32_t counter, uint64_t sentMs)
        {
            proofCounter = counter;
            proofSentMs = sentMs;
        }

        void OnProofReply(uint32_t counter)
        {
            if (proofCounter && *proofCounter == counter)
                proven = true;
        }

        std::vector<Event> OnReturn(uint64_t nowMs)
        {
            if (phase != Phase::Warning)
                return {};
            ++returns;
            uint32_t const minimum = returns == 1 ? FIRST_RETURN_MIN_MS : LATER_RETURN_MIN_MS;
            if (remainingMs < minimum)
                remainingMs = minimum;
            windowStartMs = nowMs;
            windowMs = remainingMs;
            ResetWarning();
            holdNextStep = true;
            return Announcement();
        }

        void OnEnded(uint32_t gapMs)
        {
            StartGap(gapMs);
        }

    private:
        void ResetWarning()
        {
            reminderSent = false;
            proven = false;
            holdNextStep = false;
            proofCounter.reset();
            proofSentMs.reset();
        }

        std::vector<Event> Announcement()
        {
            std::vector<Event> events = { Event::Announce };
            if (remainingMs <= REMINDER_LEAD_MS)
            {
                reminderSent = true;
                events.push_back(Event::RequestProof);
            }
            return events;
        }
    };

    struct ReplyResult
    {
        bool matched = false;
        bool late = false;
    };

    struct ProbeTracker
    {
        std::vector<std::pair<uint32_t, uint64_t>> outstanding;
        uint32_t sequence = 0;

        void OnRequestSent(uint32_t counter, uint64_t nowMs)
        {
            if (counter == 0)
            {
                outstanding.clear();
                ++sequence;
            }
            outstanding.emplace_back(counter, nowMs);
        }

        bool DeadlinePassed(uint64_t nowMs) const
        {
            return OldestUnansweredMs(nowMs) >= SILENCE_MS;
        }

        bool OnReply(uint32_t counter)
        {
            for (size_t i = 0; i < outstanding.size(); ++i)
            {
                if (outstanding[i].first != counter)
                    continue;
                outstanding.erase(outstanding.begin(), outstanding.begin() + std::ptrdiff_t(i + 1));
                return true;
            }
            return false;
        }

        ReplyResult OnReplyReceived(uint32_t counter, uint64_t receivedMs)
        {
            ReplyResult result;
            result.late = DeadlinePassed(receivedMs);
            result.matched = OnReply(counter);
            return result;
        }

        uint32_t OldestUnansweredMs(uint64_t nowMs) const
        {
            if (outstanding.empty())
                return 0;
            uint64_t oldest = outstanding.front().second;
            for (auto const& request : outstanding)
                if (request.second < oldest)
                    oldest = request.second;
            return nowMs > oldest ? uint32_t(nowMs - oldest) : 0;
        }
    };

    struct HitNote
    {
        void const* attacker = nullptr;
        void const* victim = nullptr;
        uint32_t share = 0;
    };

    struct HitNotes
    {
        std::vector<HitNote> open;

        void Push(HitNote note)
        {
            open.push_back(note);
        }

        HitNote Pop()
        {
            if (open.empty())
                return {};
            HitNote const note = open.back();
            open.pop_back();
            return note;
        }
    };

    struct PresenceInput
    {
        bool inWorld = false;
        bool farTeleport = false;
        bool taxi = false;
        bool vehicle = false;
        bool loginReady = false;
        uint32_t oldestUnansweredMs = 0;
    };

    inline bool CountdownRuns(PresenceInput const& in)
    {
        return in.inWorld && !in.farTeleport && !in.taxi && !in.vehicle && in.loginReady
            && in.oldestUnansweredMs < SILENCE_MS;
    }

    inline bool DamageSuppressed(PresenceInput const& in)
    {
        return in.farTeleport || in.oldestUnansweredMs >= SILENCE_MS;
    }
}

#endif
