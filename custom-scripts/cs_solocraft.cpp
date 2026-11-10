/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Player.h"
#include "Map.h"
#include "Creature.h"
#include "Config.h"
#include "RBAC.h"
#include <map>

using namespace Trinity::ChatCommands;

static std::map<uint32, uint32> soloScaleOriginalHealth;

class solocraft_commandscript : public CommandScript
{
public:
    solocraft_commandscript() : CommandScript("solocraft_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable solocraftCommandTable =
        {
            { "scale", HandleSoloScaleCommand, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "info",  HandleSoloInfoCommand,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
        };
        static ChatCommandTable commandTable =
        {
            { "solocraft", solocraftCommandTable },
        };
        return commandTable;
    }

    static bool HandleSoloScaleCommand(ChatHandler* handler, Optional<uint32> levelArg)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player) return false;

        InstanceMap* instanceMap = player->GetMap()->ToInstanceMap();
        if (!instanceMap)
        {
            handler->PSendSysMessage("You must be in a dungeon instance.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 playerLevel = levelArg.value_or(player->GetLevel());

        uint32 playerCount = 0;
        uint32 totalLevel = 0;
        uint8 minLevel = 255, maxLevel = 0;
        for (auto const& itr : instanceMap->GetPlayers())
        {
            if (itr.GetSource()->IsInWorld())
            {
                ++playerCount;
                uint8 lvl = itr.GetSource()->GetLevel();
                totalLevel += lvl;
                if (lvl < minLevel) minLevel = lvl;
                if (lvl > maxLevel) maxLevel = lvl;
            }
        }
        if (playerCount == 0) playerCount = 1;

        float meanLevel = (float)totalLevel / (float)playerCount;
        if (meanLevel == 0) meanLevel = (float)playerLevel;

        float hpMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.HealthMultiplier", 5.0f);
        float dmgMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.DamageMultiplier", 3.0f);
        bool levelScaling = sConfigMgr->GetBoolDefault("SoloCraft.LevelScaling", true);
        float minLevelRatio = sConfigMgr->GetFloatDefault("SoloCraft.MinLevelRatio", 0.5f);

        float healthMult = (float)playerCount / hpMultCfg;
        float damageMult = (float)playerCount / dmgMultCfg;

        uint32 scaledCount = 0;
        for (auto const& itr : instanceMap->GetCreatureBySpawnIdStore())
        {
            Creature* creature = itr.second;
            if (!creature || !creature->IsInWorld()) continue;

            uint32 spawnId = itr.first;
            // Use stored original HP if available (prevents compounding)
            uint32 originalHealth;
            auto it = soloScaleOriginalHealth.find(spawnId);
            if (it != soloScaleOriginalHealth.end())
                originalHealth = it->second;
            else
            {
                originalHealth = creature->GetMaxHealth();
                soloScaleOriginalHealth[spawnId] = originalHealth;
            }

            uint8 creatureLevel = creature->GetLevel();
            float finalHealthMult = healthMult;
            float finalDamageMult = damageMult;

            if (levelScaling && meanLevel > 0 && creatureLevel > 0 && meanLevel < creatureLevel)
            {
                float levelRatio = meanLevel / (float)creatureLevel;
                if (levelRatio < minLevelRatio) levelRatio = minLevelRatio;
                finalHealthMult *= levelRatio;
                finalDamageMult *= levelRatio;
                creature->SetLevel((uint8)meanLevel);
            }

            if (finalHealthMult < 0.1f) finalHealthMult = 0.1f;
            if (finalDamageMult < 0.1f) finalDamageMult = 0.1f;

            creature->ClearContentTuning();

            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, finalDamageMult);
            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, finalDamageMult);
            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT, finalDamageMult);
            creature->UpdateAllStats();

            uint32 newHealth = (uint32)(originalHealth * finalHealthMult);
            if (newHealth < 1) newHealth = 1;
            creature->SetMaxHealth(newHealth);
            creature->SetHealth(newHealth);

            ++scaledCount;
        }

        handler->PSendSysMessage("SoloCraft: Scaled %u creatures. HP x%.4f, DMG x%.4f (level %u, %u players)",
            scaledCount, healthMult, damageMult, (uint32)meanLevel, playerCount);
        return true;
    }

    static bool HandleSoloInfoCommand(ChatHandler* handler, Optional<uint32> /*arg*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player) return false;

        InstanceMap* instanceMap = player->GetMap()->ToInstanceMap();
        if (!instanceMap)
        {
            handler->PSendSysMessage("Not in a dungeon instance.");
            return false;
        }

        uint32 creatureCount = instanceMap->GetCreatureBySpawnIdStore().size();
        handler->PSendSysMessage("SoloCraft: %u creatures. Config: HP x%.1f, DMG x%.1f",
            creatureCount,
            sConfigMgr->GetFloatDefault("SoloCraft.HealthMultiplier", 5.0f),
            sConfigMgr->GetFloatDefault("SoloCraft.DamageMultiplier", 3.0f));
        return true;
    }
};

void AddSC_solocraft_commandscript()
{
    new solocraft_commandscript();
}