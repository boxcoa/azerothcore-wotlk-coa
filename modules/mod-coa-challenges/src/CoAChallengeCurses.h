/*
 * mod-coa-challenges: pure curse table and scheduling rules for the
 * "Hardcore - Cursed!" trial (no core dependencies, host-testable).
 *
 * The trial aura (943601 "Cursed!") ticks every 5 seconds. The archived client
 * data carries the curse spells themselves (901800-901878) but not the server
 * logic that picked them, so the selection rules below are an original design:
 * one minor curse (Nuisance/Hindrance) and one major curse (Dangerous/Deadly)
 * at a time, each on its own cooldown, gated by level and by situation.
 */
#ifndef MOD_COA_CHALLENGES_CURSES_H
#define MOD_COA_CHALLENGES_CURSES_H

#include <cstdint>
#include <vector>

namespace CoACurses
{
    constexpr uint32_t CURSED_AURA = 943601;
    constexpr uint32_t PRESTIGE_CURSED_CHALLENGE = 618;

    // Outcome spells rolled by the curses whose own trigger effect (Ascension
    // effect 184, "trigger random spell") has no server implementation.
    constexpr uint32_t SICKNESS = 901809;
    constexpr uint32_t SICKNESS_VOMIT = 901810;
    constexpr uint32_t CLUMSINESS = 901816;
    constexpr uint32_t CLUMSINESS_TRIP = 901818;
    constexpr uint32_t CHAOS = 901830;
    constexpr uint32_t CHAOS_HOP = 901832;
    constexpr uint32_t CHAOS_LAUNCH = 901837;
    constexpr uint32_t WHISPERS = 901868;
    constexpr uint32_t WHISPERS_VISION = 901860;
    constexpr uint32_t RECALL = 901864;
    constexpr uint32_t DOOM = 901866;
    constexpr uint32_t STARVATION = 901871;
    constexpr uint32_t DEHYDRATION = 901872;
    constexpr uint32_t REFLECTION_DAMAGE = 901856;

    constexpr uint32_t TICK_MS = 5000;
    constexpr uint32_t MINOR_MIN_MS = 60000;
    constexpr uint32_t MINOR_MAX_MS = 120000;
    constexpr uint32_t MAJOR_MIN_MS = 180000;
    constexpr uint32_t MAJOR_MAX_MS = 300000;
    constexpr uint32_t MINOR_GAP_MS = 30000;   // breathing room after a minor curse ends
    constexpr uint32_t MAJOR_GAP_MS = 60000;   // breathing room after a major curse ends
    constexpr uint32_t MAJOR_RETRY_MS = 30000; // a warned curse that can no longer land
    constexpr uint32_t PRESTIGE_PCT = 70;
    constexpr uint32_t LOW_LEVEL_MAJOR_PCT = 200;

    constexpr uint8_t HINDRANCE_LEVEL = 10;
    constexpr uint8_t DANGEROUS_LEVEL = 10;
    constexpr uint8_t FULL_MAJOR_LEVEL = 30;
    constexpr uint8_t DEADLY_LEVEL = 40;

    constexpr uint32_t AFFLICTION_CHANCE_PCT = 33; // Sickness / Clumsiness, per 5 s tick
    constexpr uint32_t CHAOS_CHANCE_PCT = 10;      // per 0.5 s tick
    constexpr uint32_t CHAOS_LAUNCH_LIMIT = 2;     // per application

    enum class Tier : uint8_t
    {
        Nuisance,
        Hindrance,
        Dangerous,
        Deadly
    };

    enum class Slot : uint8_t
    {
        Minor,
        Major
    };

    enum Flag : uint8_t
    {
        FLAG_NONE           = 0x00,
        FLAG_OUT_OF_COMBAT  = 0x01, // sleep / polymorph: only while not fighting
        FLAG_OPEN_WORLD     = 0x02, // never inside dungeons, raids or battlegrounds
        FLAG_GROUNDED       = 0x04, // never while swimming, flying or falling
    };

    struct Curse
    {
        uint32_t spellId;
        char const* name;
        Tier tier;
        uint8_t minLevel;
        uint8_t flags;
        uint8_t weight;
        int32_t amount; // 0 keeps the spell data's own value
    };

    // Situation of the player at the moment a curse would land.
    struct Context
    {
        uint8_t level = 1;
        bool inCombat = false;
        bool inInstance = false;
        bool swimming = false;
        bool airborne = false;
    };

    // While any of these hold, no curse lands and the timers stand still.
    struct Pause
    {
        bool dead = false;
        bool onTaxi = false;
        bool resting = false;
        bool teleporting = false;
        bool battleground = false;
    };

    // Weights: 4 = a curse that changes how you play, 2 = a plain stat penalty
    // (kept rarer so the interesting curses dominate), 1 = one of five Visions.
    inline constexpr Curse CURSES[] =
    {
        { 901840, "Curse of Drunkeness",          Tier::Nuisance,  1,  FLAG_NONE,                       4, 0 },
        { 901843, "Curse of Overgrowth",          Tier::Nuisance,  1,  FLAG_NONE,                       4, 0 },
        { 901848, "Curse of Displacement",        Tier::Nuisance,  1,  FLAG_OPEN_WORLD | FLAG_GROUNDED, 4, 0 },
        { 901857, "Curse of Visions",             Tier::Nuisance,  1,  FLAG_NONE,                       1, 0 },
        { 901858, "Curse of Visions",             Tier::Nuisance,  1,  FLAG_NONE,                       1, 0 },
        { 901859, "Curse of Visions",             Tier::Nuisance,  1,  FLAG_NONE,                       1, 0 },
        { 901860, "Curse of Visions",             Tier::Nuisance,  1,  FLAG_NONE,                       1, 0 },
        { 901861, "Curse of Visions",             Tier::Nuisance,  1,  FLAG_NONE,                       1, 0 },
        { 901868, "Curse of Whispers",            Tier::Nuisance,  1,  FLAG_NONE,                       4, 0 },

        { 901801, "Curse of Sloth",               Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },
        { 901802, "Curse of Weakness",            Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901804, "Curse of Mending",             Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },
        { 901806, "Curse of Major Feebleness",    Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901807, "Curse of Medium Feebleness",   Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901808, "Curse of Minor Feebleness",    Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901826, "Curse of Poor Vision",         Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901827, "Curse of Hemophillia",         Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },
        { 901829, "Curse of Frailness",           Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901844, "Curse of Shortening",          Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901850, "Curse of Enfeeblement",        Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },
        { 901851, "Curse of Magic Nullification", Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },
        { 901855, "Curse of Failing Grasp",       Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },
        { 901862, "Curse of Mana Addiction",      Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },
        { 901864, "Curse of Recall",              Tier::Hindrance, 10, FLAG_OPEN_WORLD,                 4, 0 },
        { 901870, "Curse of the Leprous",         Tier::Hindrance, 10, FLAG_NONE,                       2, 0 },
        { 901872, "Curse of Dehydration",         Tier::Hindrance, 10, FLAG_NONE,                       4, 0 },

        { 901800, "Curse of Fear",                Tier::Dangerous, 10, FLAG_GROUNDED,                   4, 0 },
        { 901803, "Curse of Aggression",          Tier::Dangerous, 10, FLAG_NONE,                       4, 0 },
        { 901809, "Curse of Sickness",            Tier::Dangerous, 10, FLAG_NONE,                       4, 0 },
        { 901816, "Curse of Clumsiness",          Tier::Dangerous, 10, FLAG_NONE,                       4, 0 },
        { 901825, "Curse of Minor Pain",          Tier::Dangerous, 10, FLAG_NONE,                       2, 0 },
        { 901824, "Curse of Medium Pain",         Tier::Dangerous, 30, FLAG_NONE,                       2, 0 },
        { 901823, "Curse of Major Pain",          Tier::Dangerous, 30, FLAG_NONE,                       2, 0 },
        { 901830, "Curse of Chaos",               Tier::Dangerous, 10, FLAG_GROUNDED,                   4, 0 },
        { 901841, "Curse of Narcolepsy",          Tier::Dangerous, 10, FLAG_OUT_OF_COMBAT,              4, 0 },
        { 901842, "Curse of Polymorph",           Tier::Dangerous, 10, FLAG_OUT_OF_COMBAT,              4, 0 },
        { 901845, "Curse of Storms",              Tier::Dangerous, 10, FLAG_NONE,                       4, 0 },
        { 901847, "Curse of Grooviness",          Tier::Dangerous, 10, FLAG_NONE,                       4, 0 },
        { 901854, "Curse of Minor Reflection",    Tier::Dangerous, 10, FLAG_NONE,                       2, 0 },
        { 901853, "Curse of Medium Reflection",   Tier::Dangerous, 30, FLAG_NONE,                       2, 0 },
        { 901852, "Curse of Major Reflection",    Tier::Dangerous, 30, FLAG_NONE,                       2, 0 },
        { 901874, "Curse of Paranoia",            Tier::Dangerous, 10, FLAG_NONE,                       4, 0 },
        { 901877, "Curse of Taunting",            Tier::Dangerous, 10, FLAG_NONE,                       4, 0 },

        { 901866, "Curse of Doom",                Tier::Deadly,    40, FLAG_NONE,                       4, 0 },
        { 901871, "Curse of Starvation",          Tier::Deadly,    40, FLAG_NONE,                       4, 5 },
        { 901876, "Curse of Combustion",          Tier::Deadly,    40, FLAG_NONE,                       4, 5 },
    };

    // Auras a curse leaves behind that must go when the trial ends.
    inline constexpr uint32_t CURSE_SIDE_EFFECTS[] =
    {
        SICKNESS_VOMIT, CLUMSINESS_TRIP, 901867, 901875
    };

    inline Slot SlotOf(Tier tier)
    {
        return (tier == Tier::Nuisance || tier == Tier::Hindrance) ? Slot::Minor : Slot::Major;
    }

    inline bool IsMajor(Curse const& curse)
    {
        return SlotOf(curse.tier) == Slot::Major;
    }

    inline Curse const* Find(uint32_t spellId)
    {
        for (Curse const& curse : CURSES)
            if (curse.spellId == spellId)
                return &curse;
        return nullptr;
    }

    inline bool IsPaused(Pause const& pause)
    {
        return pause.dead || pause.onTaxi || pause.resting || pause.teleporting || pause.battleground;
    }

    inline bool Allows(Curse const& curse, Context const& context)
    {
        if (context.level < curse.minLevel)
            return false;
        if ((curse.flags & FLAG_OUT_OF_COMBAT) && context.inCombat)
            return false;
        if ((curse.flags & FLAG_OPEN_WORLD) && context.inInstance)
            return false;
        if ((curse.flags & FLAG_GROUNDED) && (context.swimming || context.airborne))
            return false;
        return true;
    }

    inline std::vector<Curse const*> Eligible(Slot slot, Context const& context)
    {
        std::vector<Curse const*> out;
        for (Curse const& curse : CURSES)
            if (SlotOf(curse.tier) == slot && Allows(curse, context))
                out.push_back(&curse);
        return out;
    }

    // Weighted pick; `roll` is any uniformly random 32-bit value.
    inline Curse const* Pick(Slot slot, Context const& context, uint32_t roll)
    {
        std::vector<Curse const*> const eligible = Eligible(slot, context);
        uint32_t total = 0;
        for (Curse const* curse : eligible)
            total += curse->weight;
        if (!total)
            return nullptr;
        uint32_t point = roll % total;
        for (Curse const* curse : eligible)
        {
            if (point < curse->weight)
                return curse;
            point -= curse->weight;
        }
        return eligible.back();
    }

    // Time until the next curse of a slot may be rolled; `roll` as above.
    inline uint32_t Cooldown(Slot slot, uint8_t level, bool prestige, uint32_t roll)
    {
        uint32_t const low = slot == Slot::Minor ? MINOR_MIN_MS : MAJOR_MIN_MS;
        uint32_t const high = slot == Slot::Minor ? MINOR_MAX_MS : MAJOR_MAX_MS;
        uint64_t delay = low + roll % (high - low + 1);
        if (slot == Slot::Major && level < FULL_MAJOR_LEVEL)
            delay = delay * LOW_LEVEL_MAJOR_PCT / 100;
        if (prestige)
            delay = delay * PRESTIGE_PCT / 100;
        return uint32_t(delay);
    }

    // Seconds mark (30, 10 or 5) whose announcement falls between two
    // successive readings of a countdown, in milliseconds remaining; 0 if none.
    inline uint32_t CountdownMark(int32_t remainingBeforeMs, int32_t remainingAfterMs)
    {
        static constexpr int32_t marks[] = { 30000, 10000, 5000 };
        for (int32_t mark : marks)
            if (remainingBeforeMs > mark && remainingAfterMs <= mark && remainingAfterMs > 0)
                return uint32_t(mark / 1000);
        return 0;
    }

    // Major curses only become a real threat from FULL_MAJOR_LEVEL onwards;
    // below it they still exist, but far less often (see Cooldown).
    inline bool MajorsUnlocked(uint8_t level)
    {
        return level >= DANGEROUS_LEVEL;
    }
}

#endif
