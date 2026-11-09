# Plan: Solo/Small Group LFG without Core Modifications

## Problem
The user wants to avoid modifying TrinityCore core code (LFGMgr.cpp) because:
- Future git pulls would cause merge conflicts
- Hard to maintain in the auxiliary docker repo
- Core changes are invasive

## Reality Check
Solo LFG **requires** core modifications. The LFG system's JoinLfg, role checks,
and proposal system are hardcoded in C++. There are no script hooks to bypass them.

## Solution: Minimal Patch Approach

Instead of directly modifying core files, create **patch files** that:
1. Are small, focused diffs
2. Can be applied cleanly with `git apply` or `patch`
3. Are stored in the docker repo
4. Are applied automatically during Docker build
5. Can be re-applied after git pulls

### Files to Create

#### 1. `patches/solo_lfg.patch`
Minimal diff for LFGMgr.cpp:
```diff
--- a/src/server/game/DungeonFinding/LFGMgr.cpp
+++ b/src/server/game/DungeonFinding/LFGMgr.cpp
@@ -402,6 +402,12 @@ void LFGMgr::JoinLfg(Player* player, uint8 roles, LfgDungeonSet& dungeons)
     roles &= PLAYER_ROLE_ANY;
     roles = FilterClassRoles(player, roles);

+    // Solo/Small Group LFG: allow undersized groups
+    if (sConfigMgr->GetBoolDefault("SoloLFG.Enable", false))
+    {
+        if (!grp || grp->GetMembersCount() < 5)
+            roles = PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE;
+    }
+
     // At least 1 role must be selected
     if (!(roles & (PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE)))
         return;
```

#### 2. `patches/solo_lfg_rolecheck.patch`
Auto-finish role check for undersized groups.

#### 3. `patches/solo_lfg_proposal.patch`
Auto-accept proposals for undersized groups.

### Dockerfile Integration
Add patch application to the builder stage:
```dockerfile
# Apply custom patches
COPY patches /patches
RUN cd /src && for p in /patches/*.patch; do \
        git apply "$p" || echo "Patch $p failed"; \
    done
```

### Benefits
- Core changes are minimal (3 small patches)
- Patches are version-controlled in the docker repo
- Docker build applies them automatically
- After a git pull, just re-apply patches
- No direct modification of tracked files (cleaner)

### Alternative: Script-Only Approach (No Core Changes)
If you want to avoid patches entirely:

**Option A: NPC Party Members**
- Spawn NPC party members when player enters a dungeon solo
- NPCs follow player and participate in combat
- No LFG needed; works as a custom instance script
- More complex but fully self-contained

**Option B: Custom Dungeon Command**
- Create a GM/command that teleports player to a dungeon instance
- Scale creatures based on party size (SoloCraft already does this)
- No LFG needed

### Recommendation
Use the **patch approach** for Solo LFG. It's minimal, documented, and automatically applied during Docker build. The patches are small enough that re-applying after git pulls is trivial.

For SoloCraft, **no patches needed** - it's already a pure script.

### Files to Create
1. `patches/solo_lfg_join.patch` - JoinLfg role check bypass
2. `patches/solo_lfg_rolecheck.patch` - Auto-finish role check
3. `patches/solo_lfg_proposal.patch` - Auto-accept proposals
4. Update Dockerfile to apply patches
5. Update README with patch instructions

### Effort
- Patch creation: 2-3 hours
- Testing: 4-6 hours
- Total: 6-9 hours