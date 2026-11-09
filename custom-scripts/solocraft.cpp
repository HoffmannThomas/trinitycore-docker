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
#include "CreatureAI.h"

// Configuration defaults
static bool SoloCraftEnable = true;
static float SoloCraftHealthMultiplier = 5.0f;
static float SoloCraftDamageMultiplier = 3.0f;
static bool SoloCraftAnnounce = true;
static bool SoloCraftLevelScaling = true;
static float SoloCraftMinLevelRatio = 0.5f;
static uint32 SoloCraftMaxLevelDiff = 10;

class SoloCraftAI : public CreatureAI
{
public:
    SoloCraftAI(Creature* creature) : CreatureAI(creature) { }

    void UpdateAI(uint32 /*diff*/) override { } // no-op

    void Reset() override
    {
        CreatureAI::Reset();
        ScaleCreature();
    }

private:
    void ScaleCreature()
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        if (!enable)
            return;

        if (!me->GetMap()->IsDungeon())
            return;

        InstanceMap* instanceMap = me->GetMap()->ToInstanceMap();
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

        // Check level spread (optional)
        uint32 maxDiff = sConfigMgr->GetIntDefault("SoloCraft.MaxLevelDiff", 10);
        if (maxDiff > 0 && (maxLevel - minLevel) > maxDiff)
        {
            // Level spread too wide - skip scaling to avoid trivializing content
            TC_LOG_INFO("scripts", "SoloCraft: Level spread too wide ({}-{}), skipping scaling", minLevel, maxLevel);
            return;
        }

        float meanLevel = (float)totalLevel / (float)playerCount;
        uint8 creatureLevel = me->GetLevel();

        // Party-size multipliers
        float hpMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.HealthMultiplier", 5.0f);
        float dmgMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.DamageMultiplier", 3.0f);
        float healthMult = hpMultCfg / (float)playerCount;
        float damageMult = dmgMultCfg / (float)playerCount;

        // Level-based scaling
        bool levelScaling = sConfigMgr->GetBoolDefault("SoloCraft.LevelScaling", true);
        float minLevelRatio = sConfigMgr->GetFloatDefault("SoloCraft.MinLevelRatio", 0.5f);

        if (levelScaling && meanLevel > 0 && creatureLevel > 0 && meanLevel < creatureLevel)
        {
            float levelRatio = meanLevel / (float)creatureLevel;
            if (levelRatio < minLevelRatio)
                levelRatio = minLevelRatio;

            healthMult *= levelRatio;
            damageMult *= levelRatio;

            // Scale creature level to match player
            me->SetLevel((uint8)meanLevel);
        }

        if (healthMult < 1.0f) healthMult = 1.0f;
        if (damageMult < 1.0f) damageMult = 1.0f;

        uint32 newHealth = (uint32)(me->GetMaxHealth() * healthMult);
        me->SetMaxHealth(newHealth);
        me->SetHealth(newHealth);

        me->SetStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, damageMult);
        me->UpdateAllStats();

        // Announce on dungeon entry (handled by InstanceScript)
    }
};

class SoloCraftInstanceScript : public InstanceMapScript
{
public:
    SoloCraftInstanceScript() : InstanceMapScript("SoloCraftInstanceScript", 0) { }

    void OnPlayerEnter(InstanceMap* /*map*/, Player* player) override
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        bool announce = sConfigMgr->GetBoolDefault("SoloCraft.Announce", true);
        if (!enable || !announce)
            return;

        InstanceMap* instanceMap = player->GetMap()->ToInstanceMap();
        if (!instanceMap)
            return;

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

        if (playerCount == 0)
            return;

        float meanLevel = (float)totalLevel / (float)playerCount;
        float hpMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.HealthMultiplier", 5.0f);
        float dmgMultCfg = sConfigMgr->GetFloatDefault("SoloCraft.DamageMultiplier", 3.0f);
        bool levelScaling = sConfigMgr->GetBoolDefault("SoloCraft.LevelScaling", true);

        float healthMult = hpMultCfg / (float)playerCount;
        float damageMult = dmgMultCfg / (float)playerCount;

        if (levelScaling)
        {
            healthMult *= (meanLevel / 50.0f); // approximate level ratio
            damageMult *= (meanLevel / 50.0f);
        }

        if (healthMult < 1.0f) healthMult = 1.0f;
        if (damageMult < 1.0f) damageMult = 1.0f;

        std::string msg = "SoloCraft: Dungeon scaling active. Group size: " + std::to_string(playerCount) +
            " | Levels: " + std::to_string(minLevel) + "-" + std::to_string(maxLevel) +
            " (avg " + std::to_string((int)meanLevel) + ")" +
            " | Config: HP x" + std::to_string(hpMultCfg) +
            ", DMG x" + std::to_string(dmgMultCfg);

        if (levelScaling)
            msg += " | Level scaling: ON";

        msg += " | Final: HP x" + std::to_string(healthMult).substr(0, 4) +
               ", DMG x" + std::to_string(damageMult).substr(0, 4);

        ChatHandler(player->GetSession()).SendSysMessage(msg);
    }

    void OnPlayerLeave(InstanceMap* /*map*/, Player* player) override
    {
        bool enable = sConfigMgr->GetBoolDefault("SoloCraft.Enable", true);
        bool announce = sConfigMgr->GetBoolDefault("SoloCraft.Announce", true);
        if (!enable || !announce)
            return;

        ChatHandler(player->GetSession()).SendSysMessage(
            "SoloCraft: Left dungeon. Scaling deactivated.");
    }
};

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
};

class SoloCraftCreatureScript : public CreatureScript
{
public:
    SoloCraftCreatureScript() : CreatureScript("SoloCraftCreatureScript") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new SoloCraftAI(creature);
    }
};

void AddSC_solocraft()
{
    new SoloCraftCreatureScript();
    new SoloCraftInstanceScript();
    new SoloCraftPlayerScript();
}