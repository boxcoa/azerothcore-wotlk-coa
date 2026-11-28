#include "CoAChallengeCurses.h"

#include <gtest/gtest.h>

using namespace CoACurses;

namespace
{
    size_t CountTier(Tier tier)
    {
        size_t count = 0;
        for (Curse const& curse : Pool())
            if (curse.tier == tier)
                ++count;
        return count;
    }
}

TEST(CoACurses, PoolHasFortyFiveCursesByTier)
{
    EXPECT_EQ(Pool().size(), 45u);
    EXPECT_EQ(CountTier(Tier::Nuisance), 12u);
    EXPECT_EQ(CountTier(Tier::Hindrance), 13u);
    EXPECT_EQ(CountTier(Tier::Control), 13u);
    EXPECT_EQ(CountTier(Tier::Lethal), 7u);

    ASSERT_NE(FindCurse(901871), nullptr);
    EXPECT_EQ(FindCurse(901871)->damagePct, 90);
    ASSERT_NE(FindCurse(901827), nullptr);
    EXPECT_EQ(FindCurse(901827)->damagePct, 12);
    ASSERT_NE(FindCurse(901852), nullptr);
    EXPECT_EQ(FindCurse(901852)->damagePct, 0);
    EXPECT_EQ(FindCurse(901830), nullptr);
    EXPECT_EQ(FindCurse(901804), nullptr);
    EXPECT_EQ(FindCurse(901903), nullptr);
    EXPECT_EQ(FindCurse(901907), nullptr);
}

TEST(CoACurses, FamilyIncludesHelpers)
{
    EXPECT_TRUE(InFamily(*FindCurse(901866), 901867));
    EXPECT_TRUE(InFamily(*FindCurse(901874), 901875));
    EXPECT_TRUE(InFamily(*FindCurse(901868), 901860));
    EXPECT_TRUE(InFamily(*FindCurse(901809), 901813));
    EXPECT_FALSE(InFamily(*FindCurse(901866), 901875));
    EXPECT_FALSE(InFamily(*FindCurse(901841), 0));
}

TEST(CoACurses, BandsUnlockTiers)
{
    for (uint32_t roll = 0; roll < 100; ++roll)
    {
        EXPECT_FALSE(PickTier(9, Lane::Major, roll).has_value());
        EXPECT_EQ(PickTier(15, Lane::Major, roll), Tier::Control);
    }
    EXPECT_EQ(TierWeight(19, Tier::Lethal), 0u);
    EXPECT_EQ(TierWeight(20, Tier::Lethal), 10u);
    EXPECT_EQ(TierWeight(39, Tier::Nuisance), 35u);
    EXPECT_EQ(TierWeight(40, Tier::Nuisance), 25u);
    EXPECT_EQ(PickTier(80, Lane::Minor, 24), Tier::Nuisance);
    EXPECT_EQ(PickTier(80, Lane::Minor, 25), Tier::Hindrance);
    EXPECT_EQ(PickTier(80, Lane::Major, 24), Tier::Control);
    EXPECT_EQ(PickTier(80, Lane::Major, 25), Tier::Lethal);
    EXPECT_EQ(PickTier(1, Lane::Minor, 69), Tier::Nuisance);
    EXPECT_EQ(PickTier(1, Lane::Minor, 70), Tier::Hindrance);
}

TEST(CoACurses, CapBlocksOnlyStarvationWithHemophilia)
{
    Curse const& starvation = *FindCurse(901871);
    Curse const& hemophilia = *FindCurse(901827);
    Curse const& doom = *FindCurse(901866);

    EXPECT_FALSE(CapAllows(starvation, hemophilia.damagePct));
    EXPECT_FALSE(CapAllows(hemophilia, starvation.damagePct));
    EXPECT_TRUE(CapAllows(doom, hemophilia.damagePct));
    EXPECT_TRUE(CapAllows(hemophilia, doom.damagePct));

    for (Curse const& lethal : Pool())
        if (lethal.tier == Tier::Lethal && lethal.spell != 901871)
            EXPECT_TRUE(CapAllows(lethal, hemophilia.damagePct)) << lethal.spell;

    bool sawDoom = false;
    for (uint32_t roll = 0; roll < 100; ++roll)
    {
        Curse const* picked = PickCurse(Tier::Lethal, hemophilia.damagePct, roll);
        ASSERT_NE(picked, nullptr);
        EXPECT_NE(picked->spell, 901871u);
        sawDoom = sawDoom || picked->spell == 901866;
    }
    EXPECT_TRUE(sawDoom);

    for (uint32_t roll = 0; roll < 100; ++roll)
        EXPECT_NE(PickCurse(Tier::Hindrance, starvation.damagePct, roll)->spell, 901827u);
}

TEST(CoACurses, WarningsByTier)
{
    EXPECT_EQ(WarningMs(Tier::Nuisance), 5000u);
    EXPECT_EQ(WarningMs(Tier::Hindrance), 10000u);
    EXPECT_EQ(WarningMs(Tier::Control), 15000u);
    EXPECT_EQ(WarningMs(Tier::Lethal), 20000u);
}
