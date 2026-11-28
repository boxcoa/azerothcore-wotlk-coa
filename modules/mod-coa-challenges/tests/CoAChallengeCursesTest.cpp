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
    uint64_t now = 0;
    LaneClock clock;
    clock.StartGap(30000);
    EXPECT_TRUE(clock.Step(29999, true, now += 29999).empty());
    EXPECT_TRUE(Has(clock.Step(1, true, now += 1), Event::Roll));

    std::vector<Event> begin = clock.Begin(FindCurse(901844), now);
    EXPECT_TRUE(Has(begin, Event::Announce));
    EXPECT_TRUE(Has(begin, Event::RequestProof));
    EXPECT_FALSE(Has(begin, Event::Remind));
}

TEST(CoACurses, LaneClock_LethalTimeline)
{
    uint64_t now = 0;
    LaneClock clock;
    std::vector<Event> begin = clock.Begin(FindCurse(901871), now);
    EXPECT_TRUE(Has(begin, Event::Announce));
    EXPECT_FALSE(Has(begin, Event::RequestProof));

    EXPECT_TRUE(clock.Step(14999, true, now += 14999).empty());
    std::vector<Event> reminder = clock.Step(1, true, now += 1);
    EXPECT_TRUE(Has(reminder, Event::Remind));
    EXPECT_TRUE(Has(reminder, Event::RequestProof));
    clock.OnProofSent(7, now);

    EXPECT_FALSE(Has(clock.Step(3000, true, now += 3000), Event::Cast));
    clock.OnProofReply(7);
    EXPECT_FALSE(Has(clock.Step(1999, true, now += 1999), Event::Cast));
    EXPECT_TRUE(Has(clock.Step(1, true, now += 1), Event::Cast));
    EXPECT_EQ(clock.phase, Phase::Active);
}

TEST(CoACurses, LaneClock_NoCastWithoutReply)
{
    uint64_t now = 0;
    LaneClock clock;
    clock.Begin(FindCurse(901871), now);
    clock.Step(15000, true, now += 15000);
    clock.OnProofSent(7, now);
    EXPECT_FALSE(Has(clock.Step(5000, true, now += 5000), Event::Cast));
    EXPECT_FALSE(Has(clock.Step(60000, true, now += 60000), Event::Cast));
    clock.OnProofReply(7);
    EXPECT_TRUE(Has(clock.Step(1, true, now += 1), Event::Cast));
}

TEST(CoACurses, LaneClock_LongUpdateKeepsReminderWindow)
{
    uint64_t now = 0;
    LaneClock clock;
    clock.Begin(FindCurse(901871), now);
    std::vector<Event> burst = clock.Step(25000, true, now += 25000);
    EXPECT_EQ(CountOf(burst, Event::Remind), 1u);
    EXPECT_EQ(CountOf(burst, Event::RequestProof), 1u);
    EXPECT_FALSE(Has(burst, Event::Cast));

    clock.OnProofSent(9, now);
    clock.OnProofReply(9);
    EXPECT_FALSE(Has(clock.Step(4999, true, now += 4999), Event::Cast));
    std::vector<Event> cast = clock.Step(1, true, now += 1);
    EXPECT_EQ(CountOf(cast, Event::Cast), 1u);
    EXPECT_FALSE(Has(cast, Event::Remind));
    EXPECT_TRUE(clock.Step(10000, true, now += 10000).empty());
}

TEST(CoACurses, LaneClock_ReminderWindowStartsWhenTheRequestIsSent)
{
    uint64_t now = 0;
    LaneClock clock;
    clock.Begin(FindCurse(901841), now);
    std::vector<Event> reminder = clock.Step(10000, true, now += 10000);
    EXPECT_TRUE(Has(reminder, Event::RequestProof));
    clock.OnProofSent(5, now + 6000);
    clock.OnProofReply(5);

    EXPECT_FALSE(Has(clock.Step(6100, true, now += 6100), Event::Cast));
    EXPECT_FALSE(Has(clock.Step(4899, true, now += 4899), Event::Cast));
    EXPECT_TRUE(Has(clock.Step(1, true, now += 1), Event::Cast));
}

TEST(CoACurses, LaneClock_OldReplyDoesNotProve)
{
    uint64_t now = 0;
    LaneClock clock;
    clock.Begin(FindCurse(901841), now);
    clock.OnProofReply(7);
    EXPECT_FALSE(clock.proven);
    clock.Step(10000, true, now += 10000);
    clock.OnProofSent(7, now);
    clock.OnProofReply(6);
    EXPECT_FALSE(clock.proven);
    clock.OnProofReply(7);
    EXPECT_TRUE(clock.proven);
}

TEST(CoACurses, LaneClock_ReturnDiscardsRequest)
{
    uint64_t now = 0;
    LaneClock clock;
    clock.Begin(FindCurse(901841), now);
    clock.Step(10000, true, now += 10000);
    clock.OnProofSent(7, now);
    clock.OnReturn(now);
    clock.OnProofReply(7);
    EXPECT_FALSE(clock.proven);
    EXPECT_FALSE(clock.proofCounter.has_value());
}

TEST(CoACurses, LaneClock_AbsencePauses)
{
    uint64_t now = 0;
    LaneClock clock;
    clock.StartGap(30000);
    EXPECT_TRUE(clock.Step(60000, false, now += 60000).empty());
    EXPECT_EQ(clock.gapLeftMs, 30000u);

    clock.Begin(FindCurse(901871), now);
    EXPECT_TRUE(clock.Step(60000, false, now += 60000).empty());
    EXPECT_EQ(clock.remainingMs, 20000u);
}

TEST(CoACurses, LaneClock_RelogWindows)
{
    uint64_t now = 0;
    LaneClock early;
    early.Begin(FindCurse(901871), now);
    early.Step(3000, true, now += 3000);
    early.OnReturn(now);
    EXPECT_EQ(early.remainingMs, 17000u);

    LaneClock late;
    late.Begin(FindCurse(901871), now);
    late.Step(18000, true, now += 18000);
    now += 9000;
    std::vector<Event> first = late.OnReturn(now);
    EXPECT_EQ(late.remainingMs, 10000u);
    EXPECT_TRUE(Has(first, Event::Announce));
    EXPECT_FALSE(late.reminderSent);

    EXPECT_TRUE(late.Step(9000, true, now).empty());
    EXPECT_EQ(late.remainingMs, 10000u);
    late.Step(9000, true, now += 9000);
    now += 5000;
    std::vector<Event> second = late.OnReturn(now);
    EXPECT_EQ(late.remainingMs, 5000u);
    EXPECT_TRUE(Has(second, Event::Announce));
    EXPECT_TRUE(Has(second, Event::RequestProof));
    EXPECT_FALSE(late.proven);

    EXPECT_TRUE(late.Step(5000, true, now).empty());
    EXPECT_EQ(late.remainingMs, 5000u);
    late.OnProofSent(11, now);
    late.OnProofReply(11);
    EXPECT_FALSE(Has(late.Step(4999, true, now += 4999), Event::Cast));
    EXPECT_TRUE(Has(late.Step(1, true, now += 1), Event::Cast));
}

TEST(CoACurses, LaneClock_ReturnOnLargeUpdateKeepsFullWindow)
{
    uint64_t now = 0;
    LaneClock first;
    first.Begin(FindCurse(901871), now);
    first.Step(18000, true, now += 18000);
    now += 30000;
    first.OnReturn(now);
    EXPECT_TRUE(first.Step(30000, true, now).empty());
    EXPECT_EQ(first.remainingMs, 10000u);
    EXPECT_FALSE(first.reminderSent);

    LaneClock later;
    later.Begin(FindCurse(901871), now);
    later.Step(18000, true, now += 18000);
    later.OnReturn(now += 1);
    later.Step(1, true, now);
    later.Step(9500, true, now += 9500);
    now += 5000;
    std::vector<Event> back = later.OnReturn(now);
    EXPECT_TRUE(Has(back, Event::RequestProof));
    EXPECT_TRUE(later.Step(5000, true, now).empty());
    EXPECT_EQ(later.remainingMs, 5000u);
    later.OnProofSent(21, now);
    later.OnProofReply(21);
    EXPECT_FALSE(Has(later.Step(4999, true, now += 4999), Event::Cast));
    EXPECT_TRUE(Has(later.Step(1, true, now += 1), Event::Cast));
}

TEST(CoACurses, LaneClock_AnnouncedWindowStartsWhenAnnounced)
{
    LaneClock clock;
    clock.Begin(FindCurse(901841), 6000);
    EXPECT_TRUE(clock.Step(6100, true, 6100).empty());
    EXPECT_EQ(clock.remainingMs, 14900u);
    EXPECT_FALSE(Has(clock.Step(3900, true, 10000), Event::Remind));

    std::vector<Event> reminder = clock.Step(6000, true, 16000);
    EXPECT_TRUE(Has(reminder, Event::Remind));
    EXPECT_TRUE(Has(reminder, Event::RequestProof));
    clock.OnProofSent(4, 16000);
    clock.OnProofReply(4);
    EXPECT_FALSE(Has(clock.Step(4999, true, 20999), Event::Cast));
    EXPECT_TRUE(Has(clock.Step(1, true, 21000), Event::Cast));
}

TEST(CoACurses, LaneClock_ReturnWindowStartsWhenReannounced)
{
    LaneClock clock;
    clock.Begin(FindCurse(901841), 0);
    clock.Step(14000, true, 14000);
    clock.OnReturn(26000);
    EXPECT_EQ(clock.remainingMs, 10000u);
    EXPECT_TRUE(clock.Step(6000, true, 26000).empty());

    EXPECT_TRUE(clock.Step(6100, true, 26100).empty());
    EXPECT_EQ(clock.remainingMs, 9900u);
    std::vector<Event> reminder = clock.Step(4900, true, 31000);
    EXPECT_TRUE(Has(reminder, Event::Remind));
    clock.OnProofSent(8, 31000);
    clock.OnProofReply(8);
    EXPECT_FALSE(Has(clock.Step(4999, true, 35999), Event::Cast));
    EXPECT_TRUE(Has(clock.Step(1, true, 36000), Event::Cast));
}

TEST(CoACurses, LaneClock_TierFixedAtRoll)
{
    uint64_t now = 0;
    LaneClock clock;
    Curse const* rolled = FindCurse(901841);
    clock.Begin(rolled, now);
    for (int i = 0; i < 20; ++i)
        clock.Step(500, true, now += 500);
    EXPECT_EQ(clock.curse, rolled);
}

TEST(CoACurses, LaneClock_EndedStartsGap)
{
    uint64_t now = 0;
    LaneClock clock;
    clock.Begin(FindCurse(901844), now);
    clock.OnProofSent(3, now);
    clock.OnEnded(45000);
    EXPECT_EQ(clock.phase, Phase::Gap);
    EXPECT_EQ(clock.gapLeftMs, 45000u);
    EXPECT_EQ(clock.curse, nullptr);
    EXPECT_FALSE(clock.proofCounter.has_value());
}

TEST(CoACurses, Presence_PromptRepliesStayPresentDespiteJitter)
{
    ProbeTracker probes;
    PresenceInput in;
    in.inWorld = true;
    in.loginReady = true;

    uint64_t const sends[] = { 0, 10000, 20000, 30000, 40000 };
    uint64_t const replies[] = { 40, 10090, 20015, 30120, 40010 };
    for (size_t i = 0; i < 5; ++i)
    {
        probes.OnRequestSent(uint32_t(i), sends[i]);
        in.oldestUnansweredMs = probes.OldestUnansweredMs(sends[i] + 1);
        EXPECT_TRUE(CountdownRuns(in));
        EXPECT_TRUE(probes.OnReply(uint32_t(i)));
        in.oldestUnansweredMs = probes.OldestUnansweredMs(replies[i]);
        EXPECT_TRUE(CountdownRuns(in));
        EXPECT_FALSE(DamageSuppressed(in));
    }
    EXPECT_GT(replies[3] - replies[2], 10000u);
    in.oldestUnansweredMs = probes.OldestUnansweredMs(replies[3] + 10005);
    EXPECT_TRUE(CountdownRuns(in));
}

TEST(CoACurses, Presence_UnansweredRequestKeepsDeadline)
{
    ProbeTracker probes;
    PresenceInput in;
    in.inWorld = true;
    in.loginReady = true;

    probes.OnRequestSent(5, 0);
    in.oldestUnansweredMs = probes.OldestUnansweredMs(9999);
    EXPECT_TRUE(CountdownRuns(in));

    probes.OnRequestSent(6, 5000);
    in.oldestUnansweredMs = probes.OldestUnansweredMs(10000);
    EXPECT_EQ(in.oldestUnansweredMs, 10000u);
    EXPECT_FALSE(CountdownRuns(in));
    EXPECT_TRUE(DamageSuppressed(in));

    EXPECT_FALSE(probes.OnReply(99));
    in.oldestUnansweredMs = probes.OldestUnansweredMs(11000);
    EXPECT_FALSE(CountdownRuns(in));

    EXPECT_TRUE(probes.OnReply(6));
    EXPECT_TRUE(probes.outstanding.empty());
    in.oldestUnansweredMs = probes.OldestUnansweredMs(12000);
    EXPECT_TRUE(CountdownRuns(in));
    EXPECT_FALSE(DamageSuppressed(in));
}

TEST(CoACurses, Probe_SameTimestampRepliesRetireBySendOrder)
{
    ProbeTracker probes;
    probes.OnRequestSent(1, 100);
    probes.OnRequestSent(2, 100);
    EXPECT_TRUE(probes.OnReply(1));
    ASSERT_EQ(probes.outstanding.size(), 1u);
    EXPECT_EQ(probes.outstanding.front().first, 2u);
    EXPECT_EQ(probes.OldestUnansweredMs(10100), 10000u);
    EXPECT_TRUE(probes.OnReply(2));
    EXPECT_TRUE(probes.outstanding.empty());
}

TEST(CoACurses, Probe_CounterZeroStartsAFreshSequence)
{
    ProbeTracker probes;
    probes.OnRequestSent(7, 0);
    probes.OnRequestSent(8, 4000);
    probes.OnRequestSent(0, 6000);
    ASSERT_EQ(probes.outstanding.size(), 1u);
    EXPECT_EQ(probes.outstanding.front().first, 0u);
    EXPECT_EQ(probes.OldestUnansweredMs(15999), 9999u);
    EXPECT_FALSE(probes.DeadlinePassed(15999));
    EXPECT_TRUE(probes.DeadlinePassed(16000));
    EXPECT_TRUE(probes.OnReply(0));
    EXPECT_TRUE(probes.outstanding.empty());
}

TEST(CoACurses, Probe_LateReplyIsSeenAsAMissedDeadline)
{
    ProbeTracker probes;
    probes.OnRequestSent(3, 0);
    EXPECT_FALSE(probes.DeadlinePassed(9980));
    EXPECT_TRUE(probes.DeadlinePassed(10030));
    EXPECT_TRUE(probes.OnReply(3));
    EXPECT_FALSE(probes.DeadlinePassed(10030));
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

TEST(CoACurses, Reflection_NestedDamageKeepsTheOuterHit)
{
    int const player = 0;
    int const emperor = 0;
    int const twin = 0;
    HitNotes notes;
    notes.Push({ &player, &emperor, 75 });
    notes.Push({ &emperor, &twin, 0 });

    HitNote const inner = notes.Pop();
    EXPECT_EQ(inner.attacker, &emperor);
    EXPECT_EQ(inner.victim, &twin);
    EXPECT_EQ(inner.share, 0u);

    HitNote const outer = notes.Pop();
    EXPECT_EQ(outer.attacker, &player);
    EXPECT_EQ(outer.victim, &emperor);
    EXPECT_EQ(outer.share, 75u);

    HitNote const none = notes.Pop();
    EXPECT_EQ(none.attacker, nullptr);
    EXPECT_EQ(none.share, 0u);
}

TEST(CoACurses, Probe_ReplyIsJudgedWhenReceivedNotWhenDispatched)
{
    ProbeTracker probes;
    probes.OnRequestSent(4, 0);
    ReplyResult const onTime = probes.OnReplyReceived(4, 9900);
    EXPECT_TRUE(onTime.matched);
    EXPECT_FALSE(onTime.late);
    EXPECT_TRUE(probes.outstanding.empty());
}

TEST(CoACurses, Probe_LateReplyIsJudgedBeforeItRetiresTheRequest)
{
    ProbeTracker probes;
    probes.OnRequestSent(5, 20000);
    probes.OnRequestSent(6, 25000);
    ReplyResult const late = probes.OnReplyReceived(6, 30000);
    EXPECT_TRUE(late.matched);
    EXPECT_TRUE(late.late);
    EXPECT_TRUE(probes.outstanding.empty());

    probes.OnRequestSent(7, 40000);
    ReplyResult const unknown = probes.OnReplyReceived(9, 51000);
    EXPECT_FALSE(unknown.matched);
    EXPECT_TRUE(unknown.late);
    EXPECT_EQ(probes.outstanding.size(), 1u);
}

TEST(CoACurses, Probe_OnlyCounterZeroStartsANewSequence)
{
    ProbeTracker probes;
    probes.OnRequestSent(0, 0);
    probes.OnRequestSent(1, 5000);
    probes.OnRequestSent(2, 15000);
    EXPECT_EQ(probes.sequence, 1u);
    probes.OnRequestSent(0, 20000);
    probes.OnRequestSent(1, 25000);
    probes.OnRequestSent(2, 35000);
    EXPECT_EQ(probes.sequence, 2u);
}

TEST(CoACurses, LaneClock_WarningStartsWhenTheAnnouncementIsSent)
{
    LaneClock clock;
    clock.Begin(FindCurse(901841), 1000);
    clock.OnAnnouncementSent(clock.warningSerial, 7000);

    EXPECT_FALSE(Has(clock.Step(10000, true, 11000), Event::Remind));
    std::vector<Event> reminder = clock.Step(6000, true, 17000);
    EXPECT_TRUE(Has(reminder, Event::Remind));
    clock.OnProofSent(3, 17000);
    clock.OnProofReply(3);
    EXPECT_FALSE(Has(clock.Step(4999, true, 21999), Event::Cast));
    EXPECT_TRUE(Has(clock.Step(1, true, 22000), Event::Cast));
}

TEST(CoACurses, LaneClock_LateSendOfAnOlderAnnouncementIsIgnored)
{
    LaneClock clock;
    clock.Begin(FindCurse(901841), 0);
    uint32_t const first = clock.warningSerial;
    clock.OnReturn(5000);
    clock.OnAnnouncementSent(first, 9000);
    EXPECT_EQ(clock.windowStartMs, 5000u);
    clock.OnAnnouncementSent(clock.warningSerial, 4000);
    EXPECT_EQ(clock.windowStartMs, 5000u);
}
