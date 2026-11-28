#ifndef MOD_COA_CHALLENGES_CURSES_H
#define MOD_COA_CHALLENGES_CURSES_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
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
}

#endif
