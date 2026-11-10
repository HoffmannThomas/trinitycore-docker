# AI Agent Learnings (Internal)

> **This is internal engineering context for AI agents.** It is not a user-facing setup guide.
> Users should refer to the main [README.md](../../README.md) and [docs/setup.md](../setup.md) instead.

Internal notes for AI agents working on this TrinityCore Docker stack.

## Key Learnings

### Build System
- BuildKit cache mounts (`/ccache`, `/src/build`) persist across builds
- ccache 4.11.2 is installed on the host for native builds
- `COPY . /src` invalidates on any source change
- Custom scripts in `docker/custom-scripts/` are copied to `/src/server/scripts/Custom/` during build
- Patches in `docker/patches/` are applied with `git apply` during build

### Database
- MySQL 8.4 uses `innodb_redo_log_capacity` instead of `innodb_log_file_size` (deprecated)
- TDB is auto-imported by the tdb-init service
- Config files are in `tc-etc/` (bind-mounted to `/opt/trinitycore/etc/`)
- Solo script configs go in `worldserver.conf.d/` (auto-loaded)

### Content Tuning (WoW 12.x)
- Creatures have `ContentTuningID` in `creature_template_difficulty`
- Content tuning applies massive HP/damage multipliers based on dungeon level range
- `HasScalableLevels()` returns `ContentTuningID != 0`
- `GetLevelForTarget()` uses content tuning to calculate creature level
- `ApplyLevelScaling(0, 0)` sets ContentTuningID to 0 (disables content tuning)
- But `ScalingLevelMin/Max/Delta` fields may still have values from original tuning

### Scaling Script (SoloCraft)
- `OnMapChanged` fires when player enters an instance
- `ScaleInstanceCreatures` iterates `GetCreatureBySpawnIdStore()`
- Must store original HP before `SetLevel()` (which may recalculate HP)
- Must call `SetMaxHealth()` AFTER `UpdateAllStats()` (which recalculates from base)
- Track scaled creatures by spawn ID to prevent compounding on re-entry
- `OnGiveXP` hook can prevent XP catapult from scaled creatures

### Level Display Issue
- `SetLevel()` changes `m_unitData->Level` (server-side)
- Client displays level from content tuning fields, not from Level
- `SetUpdateFieldValue` is protected (can't call from scripts)
- `ApplyLevelScaling(0, 0)` sets ContentTuningID to 0 but doesn't clear ScalingLevelMin/Max
- UI still shows content-tuned level (35) even though server uses 10

### Healthchecks
- `pgrep` is NOT available in the container
- Use `grep -q` on `/proc/*/cmdline` instead
- Logs are cleared on startup (`rm -f /data/logs/*.log`)
- Healthcheck must check both process AND log content

### RA Console
- Port 3443, requires username/password authentication
- Account username is `2#1` (not email format)
- Custom commands work in-game but may not appear in RA's command list
- RA uses `CliHandler::ParseCommands()` which uses the same command map

### GM Commands
- Custom commands must be in the `Commands` module (not `Custom`)
- Must be registered in `cs_script_loader.cpp`
- `Console::Yes` flag required for RA access
- `rbac::RBAC_PERM_COMMAND_GM` for GM-level access

### Docker
- Named pipe at `/tmp/worldserver.cmd` for stdin commands
- Entrypoint clears logs on startup
- `depends_on` with `condition: service_healthy` for startup order
- bnetserver must restart after worldserver restart (realm re-registration)

### Common Pitfalls
1. `SetMaxHealth()` before `UpdateAllStats()` gets overridden
2. `ApplyLevelScaling(0, 0)` doesn't clear ScalingLevelMin/Max
3. `pgrep` not available in container
4. RA command list may not include custom commands
5. Healthcheck only checks log, not process (fixed with port check)
6. Compounding scaling on re-entry (fixed with spawn ID tracking)