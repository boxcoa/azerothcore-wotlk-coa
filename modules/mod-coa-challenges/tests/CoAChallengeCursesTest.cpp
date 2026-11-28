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

namespace
{
    bool Has(std::vector<Event> const& events, Event wanted)
    {
        for (Event event : events)
            if (event == wanted)
                return true;
        return false;
    }

    size_t CountOf(std::vector<Event> const& events, Event wanted)
    {
        size_t count = 0;
        for (Event event : events)
            if (event == wanted)
                ++count;
        return count;
    }
}

TEST(CoACurses, LaneClock_GapThenAnnounce)
{
    LaneClock clock;
    clock.StartGap(30000);
    EXPECT_TRUE(clock.Step(29999, true).empty());
    EXPECT_TRUE(Has(clock.Step(1, true), Event::Roll));

    std::vector<Event> begin = clock.Begin(FindCurse(901844));
    EXPECT_TRUE(Has(begin, Event::Announce));
    EXPECT_TRUE(Has(begin, Event::RequestProof));
    EXPECT_FALSE(Has(begin, Event::Remind));
}

TEST(CoACurses, LaneClock_LethalTimeline)
{
    LaneClock clock;
    std::vector<Event> begin = clock.Begin(FindCurse(901871));
    EXPECT_TRUE(Has(begin, Event::Announce));
    EXPECT_FALSE(Has(begin, Event::RequestProof));

    EXPECT_TRUE(clock.Step(14999, true).empty());
    std::vector<Event> reminder = clock.Step(1, true);
    EXPECT_TRUE(Has(reminder, Event::Remind));
    EXPECT_TRUE(Has(reminder, Event::RequestProof));
    clock.OnProofSent(7);

    EXPECT_FALSE(Has(clock.Step(3000, true), Event::Cast));
    clock.OnProofReply(7);
    EXPECT_FALSE(Has(clock.Step(1999, true), Event::Cast));
    EXPECT_TRUE(Has(clock.Step(1, true), Event::Cast));
    EXPECT_EQ(clock.phase, Phase::Active);
}

TEST(CoACurses, LaneClock_NoCastWithoutReply)
{
    LaneClock clock;
    clock.Begin(FindCurse(901871));
    clock.Step(15000, true);
    clock.OnProofSent(7);
    EXPECT_FALSE(Has(clock.Step(5000, true), Event::Cast));
    EXPECT_FALSE(Has(clock.Step(60000, true), Event::Cast));
    clock.OnProofReply(7);
    EXPECT_TRUE(Has(clock.Step(1, true), Event::Cast));
}

TEST(CoACurses, LaneClock_LongUpdateKeepsReminderWindow)
{
    LaneClock clock;
    clock.Begin(FindCurse(901871));
    std::vector<Event> burst = clock.Step(25000, true);
    EXPECT_EQ(CountOf(burst, Event::Remind), 1u);
    EXPECT_EQ(CountOf(burst, Event::RequestProof), 1u);
    EXPECT_FALSE(Has(burst, Event::Cast));

    clock.OnProofSent(9);
    clock.OnProofReply(9);
    EXPECT_FALSE(Has(clock.Step(4999, true), Event::Cast));
    std::vector<Event> cast = clock.Step(1, true);
    EXPECT_EQ(CountOf(cast, Event::Cast), 1u);
    EXPECT_FALSE(Has(cast, Event::Remind));
    EXPECT_TRUE(clock.Step(10000, true).empty());
}

TEST(CoACurses, LaneClock_OldReplyDoesNotProve)
{
    LaneClock clock;
    clock.Begin(FindCurse(901841));
    clock.OnProofReply(7);
    EXPECT_FALSE(clock.proven);
    clock.Step(10000, true);
    clock.OnProofSent(7);
    clock.OnProofReply(6);
    EXPECT_FALSE(clock.proven);
    clock.OnProofReply(7);
    EXPECT_TRUE(clock.proven);
}

TEST(CoACurses, LaneClock_ReturnDiscardsRequest)
{
    LaneClock clock;
    clock.Begin(FindCurse(901841));
    clock.Step(10000, true);
    clock.OnProofSent(7);
    clock.OnReturn();
    clock.OnProofReply(7);
    EXPECT_FALSE(clock.proven);
    EXPECT_FALSE(clock.proofCounter.has_value());
}

TEST(CoACurses, LaneClock_AbsencePauses)
{
    LaneClock clock;
    clock.StartGap(30000);
    EXPECT_TRUE(clock.Step(60000, false).empty());
    EXPECT_EQ(clock.gapLeftMs, 30000u);

    clock.Begin(FindCurse(901871));
    EXPECT_TRUE(clock.Step(60000, false).empty());
    EXPECT_EQ(clock.remainingMs, 20000u);
}

TEST(CoACurses, LaneClock_RelogWindows)
{
    LaneClock early;
    early.Begin(FindCurse(901871));
    early.Step(3000, true);
    early.OnReturn();
    EXPECT_EQ(early.remainingMs, 17000u);

    LaneClock late;
    late.Begin(FindCurse(901871));
    late.Step(18000, true);
    std::vector<Event> first = late.OnReturn();
    EXPECT_EQ(late.remainingMs, 10000u);
    EXPECT_TRUE(Has(first, Event::Announce));
    EXPECT_FALSE(late.reminderSent);

    late.Step(9000, true);
    std::vector<Event> second = late.OnReturn();
    EXPECT_EQ(late.remainingMs, 5000u);
    EXPECT_TRUE(Has(second, Event::Announce));
    EXPECT_TRUE(Has(second, Event::RequestProof));
    EXPECT_FALSE(late.proven);

    late.OnProofSent(11);
    late.OnProofReply(11);
    EXPECT_FALSE(Has(late.Step(4999, true), Event::Cast));
    EXPECT_TRUE(Has(late.Step(1, true), Event::Cast));
}

TEST(CoACurses, LaneClock_TierFixedAtRoll)
{
    LaneClock clock;
    Curse const* rolled = FindCurse(901841);
    clock.Begin(rolled);
    for (int i = 0; i < 20; ++i)
        clock.Step(500, true);
    EXPECT_EQ(clock.curse, rolled);
}

TEST(CoACurses, LaneClock_EndedStartsGap)
{
    LaneClock clock;
    clock.Begin(FindCurse(901844));
    clock.OnProofSent(3);
    clock.OnEnded(45000);
    EXPECT_EQ(clock.phase, Phase::Gap);
    EXPECT_EQ(clock.gapLeftMs, 45000u);
    EXPECT_EQ(clock.curse, nullptr);
    EXPECT_FALSE(clock.proofCounter.has_value());
}

TEST(CoACurses, Presence_StationaryClientStaysEligible)
{
    PresenceInput in;
    in.inWorld = true;
    in.loginReady = true;
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        in.msSinceLastPacket = 9999;
        EXPECT_TRUE(CountdownRuns(in));
        EXPECT_FALSE(DamageSuppressed(in));
        in.msSinceLastPacket = 0;
        EXPECT_TRUE(CountdownRuns(in));
    }
    in.msSinceLastPacket = 10000;
    EXPECT_FALSE(CountdownRuns(in));
    EXPECT_TRUE(DamageSuppressed(in));

    in.msSinceLastPacket = 0;
    in.loginReady = false;
    EXPECT_FALSE(CountdownRuns(in));
}

TEST(CoACurses, Presence_TaxiAndVehiclePauseButDoNotSuppress)
{
    PresenceInput in;
    in.inWorld = true;
    in.loginReady = true;

    in.taxi = true;
    EXPECT_FALSE(CountdownRuns(in));
    EXPECT_FALSE(DamageSuppressed(in));
    in.taxi = false;

    in.vehicle = true;
    EXPECT_FALSE(CountdownRuns(in));
    EXPECT_FALSE(DamageSuppressed(in));
    in.vehicle = false;

    in.farTeleport = true;
    EXPECT_FALSE(CountdownRuns(in));
    EXPECT_TRUE(DamageSuppressed(in));
}
