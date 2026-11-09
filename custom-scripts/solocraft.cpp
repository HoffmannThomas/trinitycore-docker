/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Config.h"
#include "World.h"
#include "Chat.h"
#include "Map.h"

// Configuration defaults
static bool SoloCraftEnable = true;
static float SoloCraftHealthMultiplier = 5.0f;
static float SoloCraftDamageMultiplier = 3.0f;
static bool SoloCraftAnnounce = true;
static bool SoloCraftLevelScaling = true;
static float SoloCraftMinLevelRatio = 0.5f;
static uint32 SoloCraftMaxLevelDiff = 10;

class SoloCraftPlayerScript : public PlayerScript
{
public:
    SoloCraftPlayerScript() : PlayerScript("SoloCraftPlayerScript") { }

    void OnLogin(Player* player, bool /*loginFirst*/) override
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        if (!enable)
            return;

        float hpMult = sConfigMgr->GetFloatDefault("SoloCraft.HealthMultiplier", 5.0f);
        float dmgMult = sConfigMgr->GetFloatDefault("SoloCraft.DamageMultiplier", 3.0f);
        bool levelScaling = sConfigMgr->GetBoolDefault("SoloCraft.LevelScaling", true);

        ChatHandler(player->GetSession()).SendSysMessage(
            "SoloCraft: " + std::string(enable ? "Enabled" : "Disabled") +
            " | HP multiplier: " + std::to_string(hpMult) +
            " | DMG multiplier: " + std::to_string(dmgMult) +
            " | Level scaling: " + std::string(levelScaling ? "ON" : "OFF"));
    }

    void OnMapChanged(Player* player) override
    {
        TC_LOG_INFO("scripts", "SoloCraft: OnMapChanged called for player {}", player->GetName());
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        bool announce = sConfigMgr->GetBoolDefault("SoloCraft.Announce", true);
        if (!enable)
            return;

        InstanceMap* instanceMap = player->GetMap()->ToInstanceMap();
        if (!instanceMap)
            return;

        // Count players and calculate mean level
        uint32 playerCount = 0;
        uint32 totalLevel = 0;
        uint8 minLevel = 255;
        uint8 maxLevel = 0;
        Map::PlayerList const& players = instanceMap->GetPlayers();
        for (auto const& itr : players)
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

        if (playerCount == 0 || playerCount > 5)
            return;

        // Check level spread
        uint32 maxDiff = sConfigMgr->GetIntDefault("SoloCraft.MaxLevelDiff", 10);
        if (maxDiff > 0 && (maxLevel - minLevel) > maxDiff)
        {
            TC_LOG_INFO("scripts", "SoloCraft: Level spread too wide ({}-{}), skipping scaling", minLevel, maxLevel);
            if (announce)
                ChatHandler(player->GetSession()).SendSysMessage(
                    "SoloCraft: Level spread too wide (" + std::to_string(minLevel) + "-" + std::to_string(maxLevel) + "). Scaling skipped.");
            return;
        }

        float meanLevel = (float)totalLevel / (float)playerCount;
        float hpMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.HealthMultiplier", 5.0f);
        float dmgMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.DamageMultiplier", 3.0f);
        bool levelScaling = sConfigMgr->GetBoolDefault("SoloCraft.LevelScaling", true);
        float minLevelRatio = sConfigMgr->GetFloatDefault("SoloCraft.MinLevelRatio", 0.5f);

        float healthMult = hpMultCfg / (float)playerCount;
        float damageMult = dmgMultCfg / (float)playerCount;

        std::string scalingInfo = "SoloCraft: Dungeon scaling active. Group size: " + std::to_string(playerCount) +
            " | Levels: " + std::to_string(minLevel) + "-" + std::to_string(maxLevel) +
            " (avg " + std::to_string((int)meanLevel) + ")" +
            " | Config: HP x" + std::to_string(hpMultCfg) +
            ", DMG x" + std::to_string(dmgMultCfg);

        if (levelScaling)
        {
            // Scale based on creature level vs mean player level
            // Applied per-creature in CreatureAI, but we announce here
            scalingInfo += " | Level scaling: ON (min ratio " + std::to_string(minLevelRatio) + ")";
        }

        scalingInfo += " | Final: HP x" + std::to_string(healthMult).substr(0, 4) +
                       ", DMG x" + std::to_string(damageMult).substr(0, 4);

        if (announce)
            ChatHandler(player->GetSession()).SendSysMessage(scalingInfo);

        // Scale all creatures in the instance
        ScaleInstanceCreatures(instanceMap, playerCount, meanLevel, healthMult, damageMult, levelScaling, minLevelRatio);
    }

    void OnPlayerLeave(InstanceMap* /*map*/, Player* player)
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        bool announce = sConfigMgr->GetBoolDefault("SoloCraft.Announce", true);
        if (!enable || !announce)
            return;

        ChatHandler(player->GetSession()).SendSysMessage(
            "SoloCraft: Left dungeon. Scaling deactivated.");
    }

private:
    void ScaleInstanceCreatures(InstanceMap* instanceMap, uint32 playerCount, float meanLevel,
                                 float healthMult, float damageMult, bool levelScaling, float minLevelRatio)
    {
        for (auto const& itr : instanceMap->GetCreatureBySpawnIdStore())
        {
            Creature* creature = itr.second;
            if (!creature || !creature->IsInWorld())
                continue;

            uint8 creatureLevel = creature->GetLevel();
            float finalHealthMult = healthMult;
            float finalDamageMult = damageMult;

            if (levelScaling && meanLevel > 0 && creatureLevel > 0 && meanLevel < creatureLevel)
            {
                float levelRatio = meanLevel / (float)creatureLevel;
                if (levelRatio < minLevelRatio)
                    levelRatio = minLevelRatio;

                finalHealthMult *= levelRatio;
                finalDamageMult *= levelRatio;

                creature->SetLevel((uint8)meanLevel);
            }

            if (finalHealthMult < 1.0f) finalHealthMult = 1.0f;
            if (finalDamageMult < 1.0f) finalDamageMult = 1.0f;

            uint32 newHealth = (uint32)(creature->GetMaxHealth() * finalHealthMult);
            creature->SetMaxHealth(newHealth);
            creature->SetHealth(newHealth);

            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, finalDamageMult);
            creature->UpdateAllStats();
        }
    }
};

void AddSC_solocraft()
{
    new SoloCraftPlayerScript();
}