#ifndef ASCENSION_ACCOUNT_PROGRESS_H
#define ASCENSION_ACCOUNT_PROGRESS_H

#include "DatabaseEnv.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "WorldSession.h"

#include <algorithm>
#include <optional>
#include <string>

namespace AscensionAccountProgress
{
    constexpr uint8 RealmProgressLevel = 60;

    constexpr char PrestigeSource[] = "core.coa_prestige";

    struct Summary
    {
        bool reachedLevelOrPrestige = false;
        uint8 highestOtherLevel = 0;
    };

    inline bool IsPrestiged(PlayerSettingVector const& settings)
    {
        return !settings.empty() && settings.front().value > 0;
    }

    inline bool HasOwnLevelOrPrestige(Player const* player, uint8 requiredLevel)
    {
        if (player->GetLevel() >= requiredLevel)
            return true;

        PlayerSettingVector const* settings = player->FindPlayerSettings(PrestigeSource);
        return settings && IsPrestiged(*settings);
    }

    inline std::optional<Summary> Summarize(Player const* player, uint8 requiredLevel)
    {
        if (!player)
            return std::nullopt;

        Summary summary;
        summary.reachedLevelOrPrestige = HasOwnLevelOrPrestige(player, requiredLevel);

        WorldSession const* session = player->GetSession();
        if (!session)
            return std::nullopt;

        CharacterDatabasePreparedStatement* stmt =
            CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_LEVEL_OR_PRESTIGE);
        stmt->SetData(0, std::string(PrestigeSource));
        stmt->SetData(1, session->GetAccountId());
        stmt->SetData(2, player->GetGUID().GetCounter());
        PreparedQueryResult result = CharacterDatabase.Query(stmt);
        if (!result)
            return summary;

        do
        {
            Field* fields = result->Fetch();
            uint8 const level = fields[0].Get<uint8>();
            summary.highestOtherLevel = std::max(summary.highestOtherLevel, level);
            PlayerSettingVector const settings = PlayerSettingsStore::ParseSettingsData(fields[1].Get<std::string>());
            if (level >= requiredLevel || IsPrestiged(settings))
                summary.reachedLevelOrPrestige = true;
        } while (result->NextRow());

        return summary;
    }

    inline bool HasLevelOrPrestige(Player const* player, uint8 requiredLevel)
    {
        if (!player)
            return false;

        if (HasOwnLevelOrPrestige(player, requiredLevel))
            return true;

        std::optional<Summary> const summary = Summarize(player, requiredLevel);
        return summary && summary->reachedLevelOrPrestige;
    }
}

#endif
