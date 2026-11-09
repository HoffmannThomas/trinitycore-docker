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
#include "Group.h"
#include "LFGMgr.h"
#include "LFGScripts.h"
#include "Chat.h"
#include "Config.h"

static bool SoloLFGEnable = true;
static bool SoloLFGAllowSoloQueue = true;

class SoloLFGPlayerScript : public PlayerScript
{
public:
    SoloLFGPlayerScript() : PlayerScript("SoloLFGPlayerScript") { }

    void OnLogin(Player* player, bool /*loginFirst*/) override
    {
        if (!SoloLFGEnable || !SoloLFGAllowSoloQueue)
            return;

        // Inform player about solo LFG
        ChatHandler(player->GetSession()).SendSysMessage("Solo LFG is enabled. You can queue for dungeons alone.");
    }
};

class SoloLFGGroupScript : public GroupScript
{
public:
    SoloLFGGroupScript() : GroupScript("SoloLFGGroupScript") { }

    void OnAddMember(Group* group, ObjectGuid guid) override
    {
        // Allow solo groups to form
        if (!SoloLFGEnable)
            return;

        // If group has only 1 member and SoloLFG is enabled, mark as solo
        if (group->GetMembersCount() == 1)
        {
            // Log solo group creation
            TC_LOG_INFO("scripts", "SoloLFG: Player {} created solo group", guid.ToString());
        }
    }
};

void AddSC_solo_lfg()
{
    new SoloLFGPlayerScript();
    new SoloLFGGroupScript();
}