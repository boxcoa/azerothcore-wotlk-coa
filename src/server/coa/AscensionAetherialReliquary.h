/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#ifndef ASCENSION_AETHERIAL_RELIQUARY_H
#define ASCENSION_AETHERIAL_RELIQUARY_H

#include "AscensionAetherialReliquaryPolicy.h"

class Player;

namespace AetherialReliquary
{
enum class CaptureReason
{
    RealmCap,
    AccountCatchUp
};

[[nodiscard]] bool IsAtCaptureLevel(Player const* player, CaptureReason reason);
[[nodiscard]] bool CatchUpAllowsBoosts(Player const* player);
bool CaptureBuffs(Player* player, CaptureReason reason);
bool CaptureAtCaptureLevel(Player* player);
[[nodiscard]] StoredBuffs GetStored(Player const* player);
bool RestoreBuffs(Player* player);
}

#endif
