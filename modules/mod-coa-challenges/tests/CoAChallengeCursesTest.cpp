/*
 * mod-coa-challenges: host-side unit tests for the "Hardcore - Cursed!" curse
 * table and scheduling rules in CoAChallengeCurses.h.
 */
#include "CoAChallengeCurses.h"

#include <gtest/gtest.h>

#include <map>
#include <set>

using namespace CoACurses;

namespace
{
    Context AtLevel(uint8_t level)
    {
        Context context;
        context.level = level;
        return context;
    }
}

TEST(CoACurses, TableHasUniqueSpellsWithPositiveWeights)
{
    std::set<uint32_t> seen;
    for (Curse const& curse : CURSES)
    {
        EXPECT_TRUE(seen.insert(curse.spellId).second) << curse.spellId;
        EXPECT_GT(curse.weight, 0u) << curse.name;
        EXPECT_GE(curse.spellId, 901800u) << curse.name;
        EXPECT_LE(curse.spellId, 901878u) << curse.name;
    }
}

TEST(CoACurses, TierLevelGates)
{
    for (Curse const& curse : CURSES)
    {
        switch (curse.tier)
        {
            case Tier::Nuisance:
                EXPECT_EQ(curse.minLevel, 1u) << curse.name;
                break;
            case Tier::Hindrance:
                EXPECT_GE(curse.minLevel, HINDRANCE_LEVEL) << curse.name;
                break;
            case Tier::Dangerous:
                EXPECT_GE(curse.minLevel, DANGEROUS_LEVEL) << curse.name;
                break;
            case Tier::Deadly:
                EXPECT_GE(curse.minLevel, DEADLY_LEVEL) << curse.name;
                break;
        }
    }
}

TEST(CoACurses, UnfairOrBrokenSpellsAreExcluded)
{
    EXPECT_EQ(Find(901903), nullptr); // Curse of Partying: 2,000 damage per second
    EXPECT_EQ(Find(901905), nullptr); // Curse of Recklesness: charges at enemies
    EXPECT_EQ(Find(901907), nullptr); // dummy duplicate of Magic Nullification
    EXPECT_EQ(Find(901835), nullptr); // Chaos 300: shove strong enough to kill
    EXPECT_EQ(Find(901838), nullptr);
}

TEST(CoACurses, DeadlyDrainsAreTonedDown)
{
    ASSERT_NE(Find(STARVATION), nullptr);
    EXPECT_EQ(Find(STARVATION)->amount, 5);
    ASSERT_NE(Find(901876), nullptr);
    EXPECT_EQ(Find(901876)->amount, 5);
    ASSERT_NE(Find(DOOM), nullptr);
    EXPECT_EQ(Find(DOOM)->tier, Tier::Deadly);
}

TEST(CoACurses, SlotsSplitByTier)
{
    EXPECT_EQ(SlotOf(Tier::Nuisance), Slot::Minor);
    EXPECT_EQ(SlotOf(Tier::Hindrance), Slot::Minor);
    EXPECT_EQ(SlotOf(Tier::Dangerous), Slot::Major);
    EXPECT_EQ(SlotOf(Tier::Deadly), Slot::Major);
}

TEST(CoACurses, LowLevelsOnlyGetNuisances)
{
    for (Curse const* curse : Eligible(Slot::Minor, AtLevel(9)))
        EXPECT_EQ(curse->tier, Tier::Nuisance) << curse->name;
    EXPECT_TRUE(Eligible(Slot::Major, AtLevel(9)).empty());
    EXPECT_FALSE(MajorsUnlocked(9));
    EXPECT_TRUE(MajorsUnlocked(10));
}

TEST(CoACurses, DeadlyCursesWaitForLevelForty)
{
    auto hasDeadly = [](uint8_t level)
    {
        for (Curse const* curse : Eligible(Slot::Major, AtLevel(level)))
            if (curse->tier == Tier::Deadly)
                return true;
        return false;
    };
    EXPECT_FALSE(hasDeadly(39));
    EXPECT_TRUE(hasDeadly(40));
}

TEST(CoACurses, SituationalFlags)
{
    Curse const* narcolepsy = Find(901841);
    Curse const* recall = Find(RECALL);
    Curse const* chaos = Find(CHAOS);
    ASSERT_TRUE(narcolepsy && recall && chaos);

    Context context = AtLevel(60);
    EXPECT_TRUE(Allows(*narcolepsy, context));
    context.inCombat = true;
    EXPECT_FALSE(Allows(*narcolepsy, context));

    context = AtLevel(60);
    context.inInstance = true;
    EXPECT_FALSE(Allows(*recall, context));
    EXPECT_TRUE(Allows(*chaos, context));

    context = AtLevel(60);
    context.swimming = true;
    EXPECT_FALSE(Allows(*chaos, context));
    context.swimming = false;
    context.airborne = true;
    EXPECT_FALSE(Allows(*chaos, context));
}

TEST(CoACurses, PickHonoursWeightsAndEligibility)
{
    Context const context = AtLevel(60);
    std::vector<Curse const*> const eligible = Eligible(Slot::Major, context);
    uint32_t total = 0;
    for (Curse const* curse : eligible)
        total += curse->weight;
    ASSERT_GT(total, 0u);

    std::map<uint32_t, uint32_t> hits;
    for (uint32_t roll = 0; roll < total; ++roll)
    {
        Curse const* curse = Pick(Slot::Major, context, roll);
        ASSERT_NE(curse, nullptr);
        EXPECT_TRUE(IsMajor(*curse));
        ++hits[curse->spellId];
    }
    for (Curse const* curse : eligible)
        EXPECT_EQ(hits[curse->spellId], curse->weight) << curse->name;

    EXPECT_EQ(Pick(Slot::Major, AtLevel(1), 12345), nullptr);
}

TEST(CoACurses, PickNeverReturnsAForbiddenCurse)
{
    Context context = AtLevel(60);
    context.inCombat = true;
    context.inInstance = true;
    context.swimming = true;
    for (uint32_t roll = 0; roll < 500; ++roll)
    {
        Curse const* major = Pick(Slot::Major, context, roll);
        Curse const* minor = Pick(Slot::Minor, context, roll);
        ASSERT_TRUE(major && minor);
        EXPECT_TRUE(Allows(*major, context)) << major->name;
        EXPECT_TRUE(Allows(*minor, context)) << minor->name;
    }
}

TEST(CoACurses, CooldownRanges)
{
    for (uint32_t roll : { 0u, 1u, 30000u, 4294967295u })
    {
        uint32_t const minor = Cooldown(Slot::Minor, 60, false, roll);
        EXPECT_GE(minor, MINOR_MIN_MS);
        EXPECT_LE(minor, MINOR_MAX_MS);

        uint32_t const major = Cooldown(Slot::Major, 60, false, roll);
        EXPECT_GE(major, MAJOR_MIN_MS);
        EXPECT_LE(major, MAJOR_MAX_MS);

        uint32_t const lowLevelMajor = Cooldown(Slot::Major, 20, false, roll);
        EXPECT_EQ(lowLevelMajor, major * 2);
        EXPECT_EQ(Cooldown(Slot::Minor, 20, false, roll), minor);

        EXPECT_EQ(Cooldown(Slot::Major, 60, true, roll), major * 70 / 100);
        EXPECT_EQ(Cooldown(Slot::Minor, 60, true, roll), minor * 70 / 100);
    }
}

TEST(CoACurses, DoomCountdownMarks)
{
    EXPECT_EQ(CountdownMark(35000, 30000), 30u);
    EXPECT_EQ(CountdownMark(31000, 26000), 30u);
    EXPECT_EQ(CountdownMark(30000, 25000), 0u);
    EXPECT_EQ(CountdownMark(12000, 7000), 10u);
    EXPECT_EQ(CountdownMark(6000, 1000), 5u);
    EXPECT_EQ(CountdownMark(40000, 35000), 0u);
    EXPECT_EQ(CountdownMark(4000, 0), 0u);
    EXPECT_EQ(CountdownMark(0, 90000), 0u);
}

TEST(CoACurses, PausedSituations)
{
    EXPECT_FALSE(IsPaused(Pause{}));
    Pause pause;
    pause.resting = true;
    EXPECT_TRUE(IsPaused(pause));
    pause = Pause{};
    pause.onTaxi = true;
    EXPECT_TRUE(IsPaused(pause));
    pause = Pause{};
    pause.dead = true;
    EXPECT_TRUE(IsPaused(pause));
    pause = Pause{};
    pause.battleground = true;
    EXPECT_TRUE(IsPaused(pause));
}
