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
        if (!SoloLFGEnable)
            return;

        ChatHandler(player->GetSession()).SendSysMessage(
            "SoloLFG: " + std::string(SoloLFGEnable ? "Enabled" : "Disabled") +
            " | Solo queue: " + std::string(SoloLFGAllowSoloQueue ? "Allowed" : "Disabled"));
    }

    void OnMapChanged(Player* player) override
    {
        if (!SoloLFGEnable || !SoloLFGAllowSoloQueue)
            return;

        // Check if player is in a dungeon via LFG
        if (player->GetGroup() && player->GetGroup()->isLFGGroup())
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "SoloLFG: Entered LFG dungeon. Solo play active.");
        }
    }
};

class SoloLFGGroupScript : public GroupScript
{
public:
    SoloLFGGroupScript() : GroupScript("SoloLFGGroupScript") { }

    void OnAddMember(Group* group, ObjectGuid guid) override
    {
        if (!SoloLFGEnable)
            return;

        if (group->GetMembersCount() == 1)
        {
            TC_LOG_INFO("scripts", "SoloLFG: Player {} created solo group", guid.ToString());
        }
    }

    void OnDisband(Group* group) override
    {
        if (!SoloLFGEnable)
            return;

        // Check if this was a solo LFG group
        if (group->isLFGGroup())
        {
            for (GroupReference const& ref : group->GetMembers())
            {
                if (ref.GetSource() && ref.GetSource()->IsInWorld())
                {
                    ChatHandler(ref.GetSource()->GetSession()).SendSysMessage(
                        "SoloLFG: Left LFG dungeon. Scaling deactivated.");
                }
            }
        }
    }
};

void AddSC_solo_lfg()
{
    new SoloLFGPlayerScript();
    new SoloLFGGroupScript();
}