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
        if (!SoloCraftEnable)
            return;

        if (!me->GetMap()->IsDungeon())
            return;

        InstanceMap* instanceMap = me->GetMap()->ToInstanceMap();
        if (!instanceMap)
            return;

        uint32 playerCount = 0;
        Map::PlayerList const& players = instanceMap->GetPlayers();
        for (auto const& itr : players)
            if (itr.GetSource()->IsInWorld())
                ++playerCount;

        if (playerCount == 0 || playerCount > 5)
            return;

        float healthMult = SoloCraftHealthMultiplier / (float)playerCount;
        float damageMult = SoloCraftDamageMultiplier / (float)playerCount;

        if (healthMult < 1.0f) healthMult = 1.0f;
        if (damageMult < 1.0f) damageMult = 1.0f;

        uint32 newHealth = (uint32)(me->GetMaxHealth() * healthMult);
        me->SetMaxHealth(newHealth);
        me->SetHealth(newHealth);

        me->SetStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, damageMult);
        me->UpdateAllStats();
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
}