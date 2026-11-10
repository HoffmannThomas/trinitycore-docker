# TrinityCore Docker Stack

Docker Compose stack for running [TrinityCore](https://github.com/TrinityCore/TrinityCore) (master branch, WoW 12.x) built from source, with MySQL 8.4.

## Features

- **Multi-stage Docker build** — separate build-tools layer, minimal runtime image (~1.5GB)
- **BuildKit caching** — incremental rebuilds via ccache + build cache mounts (5.3s no-change, 45s single-file)
- **Auto-setup** — downloads TDB, creates databases, applies updates automatically
- **Custom scripts** — SoloCraft (creature scaling) and SoloLFG (solo dungeon finder)
- **GM commands** — `.solocraft scale/info` for automated testing
- **Healthchecks** — process + log verification for all services
- **Clean logging** — timestamps, no spam, tailed to stdout
- **Environment-based config** — all server-specific values in `.env` (gitignored)
- **MySQL optimization** — 2GB buffer pool, 8 I/O threads, connection pooling
- **Console access** — named pipe + RA console (port 3443) for remote commands

## Quick Start

```bash
# 1. Clone TrinityCore source
git clone https://github.com/TrinityCore/TrinityCore.git
cd TrinityCore

# 2. Clone this docker stack
git clone https://github.com/HoffmannThomas/trinitycore-docker.git docker

# 3. Configure
cp docker/.env.example docker/.env
# Edit docker/.env with your values

# 4. Build
cd docker && docker compose build

# 5. Start
docker compose up -d
```

First start downloads TDB and auto-populates databases (~5-10 minutes).

## Prerequisites

- **Server**: Debian 12+ / Ubuntu 22.04+, 8+ CPU cores, 16GB+ RAM, 50GB+ disk
- **Docker**: Engine 24+ with Compose v2
- **Build tools**: clang, cmake, ccache (for native map extraction)
- **WoW client**: 12.1.5.70783 (for map extraction)
- **SSH access** to the server

See the [Quick Start](#quick-start) section above for detailed setup including CIFS mount, user creation, and map extraction.

## Configuration

### Environment Variables (.env)

| Variable | Default | Description |
|----------|---------|-------------|
| `SERVER_IP` | `127.0.0.1` | Server's public IP |
| `TC_ETC_PATH` | `./tc-etc` | Config files path (bind mount) |
| `TC_DATA_PATH` | `./tc-data` | Map data path (bind mount) |
| `MYSQL_ROOT_PASSWORD` | `trinity` | MySQL root password |
| `DB_USER` / `DB_PASS` | `trinity` | Database credentials |

### Ports

| Port | Service |
|------|---------|
| 1119 | Battle.net (bnetserver) |
| 8081 | Login REST API |
| 8085 | World server |
| 3306 | MySQL |
| 3443 | Remote Access (RA) console |

## Custom Scripts

### SoloCraft
Scales creature health and damage for solo/small-group play in dungeons.

**How it works:**
- **Party size scaling**: `healthMult = playerCount / HealthMultiplier` (solo = 1/5.0 = 0.2x)
- **Level scaling**: adjusts creature level to match player level (reduces level-gap penalties)
- **Content tuning disabled**: removes the massive level-based HP/damage multipliers
- **HP and damage scaled independently**: HealthMultiplier and DamageMultiplier are separate
- **No compounding**: Re-entering the same instance doesn't re-scale (tracked by spawn ID)

**Configuration** (`solocraft.conf`):
```ini
SoloCraft.Enable = 1
SoloCraft.HealthMultiplier = 5.0    # Solo: 1/5 = 0.2x HP (5-player equivalent)
SoloCraft.DamageMultiplier = 3.0   # Solo: 1/3 = 0.33x damage (3-player equivalent)
SoloCraft.Announce = 1
SoloCraft.LevelScaling = 1
SoloCraft.MinLevelRatio = 0.5       # Minimum level ratio (prevents 0 HP)
SoloCraft.MaxLevelDiff = 10         # Skip scaling if level spread > 10
```

**GM Command:**
```
.solocraft scale [level]   # Scale all creatures in current instance
.solocraft info            # Show creature count and config
```

**Notes:**
- Only activates in dungeon/raid instances
- Scaling is per-instance (tracked by spawn ID)
- Re-entering the same instance doesn't compound scaling
- Level changes trigger re-scaling

### SoloLFG
Enables solo dungeon finder notifications and core patches for solo LFG.

**Configuration** (`solo_lfg.conf`):
```ini
SoloLFG.Enable = 1
SoloLFG.AllowSoloQueue = 1
```

### Core Patches
Minimal patches in `patches/` are applied during Docker build:
- `lfg_solo_queue.patch` — Allow solo LFG queueing
- `lfg_solo_group.patch` — Solo group creation support

## Healthchecks

All services have healthchecks that verify actual readiness:

| Service | Check |
|---------|-------|
| **MySQL** | `mysqladmin ping` |
| **bnetserver** | Process running + "Added realm" in log |
| **worldserver** | Process running + "ready..." in log |

Logs are cleared on startup to prevent stale healthcheck passes.

## MySQL Optimization

MySQL is tuned for TrinityCore workloads. See [docs/mysql-optimization.md](docs/mysql-optimization.md) for details.

**Key settings:**
- InnoDB buffer pool: 2GB
- I/O threads: 8 read, 8 write
- Max connections: 200
- CPU limit: 4 cores, 4GB RAM

## Startup Optimization

Server startup takes ~60-90 seconds. See [docs/startup-optimization.md](docs/startup-optimization.md) for details.

**Quick wins applied:**
- BuildKit cache (5.3s no-change rebuild)
- ccache (incremental builds)
- `GridUnload = 1` (unload unused grids)
- `UseProcessors = 0` (all cores)

## GM Commands

| Command | Description |
|---------|-------------|
| `.solocraft scale [level]` | Scale creatures in current instance |
| `.solocraft info` | Show creature count and config |
| `.tele <name> <location>` | Teleport to location |
| `.npc info` | Check creature stats (target NPC first) |
| `.character level <name> <level>` | Set character level |
| `.gmannounce <msg>` | Server-wide announcement |

### Remote Access (RA)
Port 3443 provides a telnet-like console:
```bash
# Connect
python3 -c "
import socket
s = socket.socket(); s.connect(('SERVER_IP', 3443))
# Send username, password, then commands
"
```

## Client Setup

### 1. Install SSL Certificate
```powershell
# Download cert from server
scp trinitycore@SERVER_IP:/home/trinitycore/certs/bnetserver.cert.pem C:\temp\
# Install as Trusted Root (Admin PowerShell)
Import-Certificate -FilePath "C:\temp\bnetserver.cert.pem" -CertStoreLocation "Cert:\LocalMachine\Root"
```

### 2. Configure Client
Edit `WTF/Config.wtf`:
```
SET portal "SERVER_IP"
```

### 3. Launch
Use [Burralis](https://burralis.io/) launcher from the WoW client folder.

## Troubleshooting

### Login Stuck at Realmlist
- Restart bnetserver: `docker compose restart bnetserver`
- Check realm registration: `docker logs trinitycore-bnetserver-1 | grep "Added realm"`

### No Map Data
Worldserver exits if DBC/map files are missing. Ensure `/data` contains `dbc/`, `maps/`, etc.

### Logs Not Showing
Entrypoint tails log files to stdout. Check `/data/logs/` inside the container.

### SSL Handshake Failed
- Install certificate in "Trusted Root Certification Authorities"
- Restart WoW client after installing

### Healthcheck Failing
- Check if process is running: `docker exec <container> sh -c 'ls /proc/*/cmdline | xargs grep worldserver'`
- Logs may be stale — restart container to clear

## Automated Testing

### Manual Workflow
1. Log in with GM character
2. `.tele deadmines`
3. `.solocraft scale 10`
4. `.npc info` on creatures
5. Test combat
6. Leave and re-enter → verify no compounding

### SQL Verification
```sql
-- Check creature HP after scaling
SELECT guid, id, health, maxHealth FROM creature WHERE map = 36;
```

## License

This project is provided as-is. TrinityCore is licensed under GPL v2.