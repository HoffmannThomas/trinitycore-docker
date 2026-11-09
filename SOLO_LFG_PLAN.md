# Plan: Solo/Small Group LFG for TrinityCore Master (12.1.5)

## Problem
The LFG system requires a full group of 5 players with all roles (tank, healer, dps).
Players in groups of 1-4 cannot queue for dungeons.

## Goal
Allow groups of 1-5 players to queue for dungeons. Undersized groups should:
- Bypass the full role check requirement
- Auto-accept proposals
- Scale dungeon difficulty (via SoloCraft) based on actual group size

## Approach
Patch LFGMgr to handle undersized groups. This requires modifying the core.

## Files to Modify

### 1. LFGMgr.cpp / LFGMgr.h
- Add `SoloLFG.Enable` and `SoloLFG.AllowUndersized` config options
- Modify `JoinLfg()` to allow groups with < 5 players
- Modify `UpdateRoleCheck()` to auto-finish for undersized groups
- Modify proposal system to handle smaller groups
- Modify `FilterClassRoles()` to not require all roles for undersized groups

### 2. LFGScripts.cpp
- Enhance notifications for solo/undersized queue events
- Add hooks for queue join/leave

### 3. Config
- Add `SoloLFG.Enable`, `SoloLFG.AllowSoloQueue`, `SoloLFG.AllowUndersized` to worldserver.conf

## Implementation Steps

### Step 1: Add config options
In worldserver.conf.dist, add:
```
# Solo/Small Group LFG
SoloLFG.Enable = 0
SoloLFG.AllowSoloQueue = 0
SoloLFG.AllowUndersized = 0
```

### Step 2: Modify LFGMgr::JoinLfg()
After the role check (line ~402), add:
```cpp
bool soloLFG = sConfigMgr->GetBoolDefault("SoloLFG.Enable", false);
bool allowUndersized = sConfigMgr->GetBoolDefault("SoloLFG.AllowUndersized", false);
bool allowSolo = sConfigMgr->GetBoolDefault("SoloLFG.AllowSoloQueue", false);

// For undersized groups (1-4 players), relax role requirements
if (soloLFG && allowUndersized && grp && grp->GetMembersCount() < 5) {
    // Auto-assign missing roles
    if (!(roles & PLAYER_ROLE_TANK)) roles |= PLAYER_ROLE_TANK;
    if (!(roles & PLAYER_ROLE_HEALER)) roles |= PLAYER_ROLE_HEALER;
    if (!(roles & PLAYER_ROLE_DAMAGE)) roles |= PLAYER_ROLE_DAMAGE;
}
```

### Step 3: Modify role check
In UpdateRoleCheck, auto-finish for undersized groups:
```cpp
if (soloLFG && allowUndersized) {
    uint8 memberCount = group->GetMembersCount();
    if (memberCount < 5) {
        // Auto-finish role check if all present members confirmed
        roleCheck.state = LFG_ROLECHECK_FINISHED;
    }
}
```

### Step 4: Modify proposal system
Handle smaller groups in proposal formation:
```cpp
if (soloLFG && allowUndersized) {
    // Lower the required player count for proposals
    // Auto-accept for undersized groups
}
```

### Step 5: Handle group formation
- Allow solo players to form a "group of one"
- Allow 2-4 player groups to queue without filling to 5

### Step 6: Integration with SoloCraft
- SoloCraft scaling already handles 1-5 players
- Undersized groups get appropriate scaling

## Effort Estimate
- Core LFGMgr changes: 3-4 hours
- Testing: 4-6 hours
- Total: 7-10 hours

## Risks
- LFG system is complex; changes may break matchmaking
- Need to handle edge cases (disconnects, dungeon resets, group changes mid-queue)
- May conflict with existing LFG logic
- Requires rebuilding the entire server

## Alternative: Script-Only Approach
Use a PlayerScript to:
1. On LFG queue attempt, create a fake group with NPC "ghost" members
2. Auto-accept all proposals
This is hacky and may not work reliably.

## Recommendation
Patch LFGMgr directly. The changes are localized to JoinLfg and the proposal/role-check flow.
Combine with SoloCraft for difficulty scaling based on actual group size.