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
#include "ObjectGuid.h"
#include <map>

// Configuration defaults
static bool SoloCraftEnable = true;
static float SoloCraftHealthMultiplier = 5.0f;
static float SoloCraftDamageMultiplier = 3.0f;
static bool SoloCraftAnnounce = true;
static bool SoloCraftLevelScaling = true;
static float SoloCraftMinLevelRatio = 0.5f;
static uint32 SoloCraftMaxLevelDiff = 10;

static std::map<uint32, uint32> originalHealthMap;
static InstanceMap* lastScaledInstance = nullptr;
static bool forceRescale = false;

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
            " | Group-equivalent: " + std::to_string((int)hpMult) + " players" +
            " | Solo HP scale: 1/" + std::to_string((int)hpMult) +
            " | Solo DMG scale: 1/" + std::to_string((int)dmgMult) +
            " | Level scaling: " + std::string(levelScaling ? "ON" : "OFF"));
    }

    void OnMapChanged(Player* player) override
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        bool announce = sConfigMgr->GetBoolDefault("SoloCraft.Announce", true);
        if (!enable)
            return;

        InstanceMap* instanceMap = player->GetMap()->ToInstanceMap();
        if (!instanceMap)
            return;

        // If this is the same instance we already scaled, skip (unless forced)
        if (instanceMap == lastScaledInstance && !forceRescale)
            return;

        // New instance - clear tracking and update pointer
        originalHealthMap.clear();
        lastScaledInstance = instanceMap;

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

        float healthMult = (float)playerCount / hpMultCfg;
        float damageMult = (float)playerCount / dmgMultCfg;

        std::string scalingInfo = "SoloCraft: Dungeon scaling active. Group size: " + std::to_string(playerCount) +
            " | Levels: " + std::to_string(minLevel) + "-" + std::to_string(maxLevel) +
            " (avg " + std::to_string((int)meanLevel) + ")" +
            " | Config: group-equiv HP=" + std::to_string((int)hpMultCfg) +
            ", DMG=" + std::to_string((int)dmgMultCfg);

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

    void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        if (!enable)
            return;

        InstanceMap* instanceMap = player->GetMap()->ToInstanceMap();
        if (!instanceMap)
            return;

        // Force re-scale on level change
        forceRescale = true;
        // Re-trigger scaling by calling OnMapChanged logic
        // (We can't call OnMapChanged directly, so we'll handle it here)

        // Count players
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
        float hpMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.HealthMultiplier", 5.0f);
        float dmgMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.DamageMultiplier", 3.0f);
        bool levelScaling = sConfigMgr->GetBoolDefault("SoloCraft.LevelScaling", true);
        float minLevelRatio = sConfigMgr->GetFloatDefault("SoloCraft.MinLevelRatio", 0.5f);

        float healthMult = (float)playerCount / hpMultCfg;
        float damageMult = (float)playerCount / dmgMultCfg;

        // Scale all creatures
        for (auto const& itr : instanceMap->GetCreatureBySpawnIdStore())
        {
            Creature* creature = itr.second;
            if (!creature || !creature->IsInWorld())
                continue;

            uint32 spawnId = itr.first;
            uint32 originalHealth;
            auto it = originalHealthMap.find(spawnId);
            if (it != originalHealthMap.end())
                originalHealth = it->second;
            else
            {
                originalHealth = creature->GetMaxHealth();
                originalHealthMap[spawnId] = originalHealth;
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

            creature->ApplyLevelScaling(0, 0);

            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, finalDamageMult);
            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, finalDamageMult);
            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT, finalDamageMult);
            creature->UpdateAllStats();

            uint32 newHealth = (uint32)(originalHealth * finalHealthMult);
            if (newHealth < 1) newHealth = 1;
            creature->SetMaxHealth(newHealth);
            creature->SetHealth(newHealth);
        }

        forceRescale = false;
    }

    void OnGiveXP(Player* player, uint32& amount, Unit* victim) override
    {
        // Prevent XP catapult from content-tuned creatures
        if (victim && victim->GetTypeId() == TYPEID_UNIT)
        {
            bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
            if (enable && player->GetMap()->ToInstanceMap())
            {
                // Set XP to 0 for scaled creatures
                amount = 0;
            }
        }
    }

    void OnPlayerLeave(InstanceMap* /*map*/, Player* player)
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        bool announce = sConfigMgr->GetBoolDefault("SoloCraft.Announce", true);
        if (!enable || !announce)
            return;

        // Clear tracking for this instance
        originalHealthMap.clear();
        lastScaledInstance = nullptr;
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

            uint32 spawnId = itr.first;

            // Check if we already have the original HP stored for this spawn
            uint32 originalHealth;
            auto it = originalHealthMap.find(spawnId);
            if (it != originalHealthMap.end())
            {
                // Already scaled - use stored original HP, but skip re-scaling
                continue;
            }
            else
            {
                // First time - store original HP
                originalHealth = creature->GetMaxHealth();
                originalHealthMap[spawnId] = originalHealth;
            }

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

            // Floor at 0.05 to prevent zero/negative HP
            if (finalHealthMult < 0.05f) finalHealthMult = 0.05f;
            if (finalDamageMult < 0.05f) finalDamageMult = 0.05f;

            // Disable content tuning (removes the massive level-based multiplier)
            creature->ApplyLevelScaling(0, 0);

            // Apply damage scaling via stat modifiers
            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, finalDamageMult);
            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, finalDamageMult);
            creature->SetStatPctModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT, finalDamageMult);
            creature->UpdateAllStats();

            // Apply HP scaling AFTER UpdateAllStats (which recalculates health from base)
            uint32 newHealth = (uint32)(originalHealth * finalHealthMult);
            if (newHealth < 1) newHealth = 1;
            creature->SetMaxHealth(newHealth);
            creature->SetHealth(newHealth);

            // Original HP already stored above
        }
    }

};

void AddSC_solocraft()
{
    new SoloCraftPlayerScript();
}
