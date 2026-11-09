# TrinityCore Docker Stack

Docker Compose stack for running [TrinityCore](https://github.com/TrinityCore/TrinityCore) (master branch) built from source, with MySQL 8.4.

## Features

- **Multi-stage Docker build** — separate build-tools layer, minimal runtime image
- **BuildKit caching** — incremental rebuilds via ccache and build cache mounts
- **Auto-setup** — automatically downloads TDB, creates databases, applies updates
- **Map extraction** — includes extractor service for DBC/maps/vmaps/mmaps
- **Custom scripts** — SoloCraft (creature scaling) and SoloLFG (solo dungeon finder)
- **Clean logging** — log files tailed to stdout for Docker log capture
- **Console access** — named pipe for sending commands to the running server

## Prerequisites

- Docker Engine 24+ with Docker Compose v2
- 8+ CPU cores, 16GB+ RAM, 50GB+ free disk space
- A WoW 12.1.5 client for map extraction

## Quick Start

### 1. Clone the repository

```bash
git clone https://github.com/TrinityCore/TrinityCore.git
cd TrinityCore
# Copy docker/ directory into the repo root
cp -r /path/to/trinitycore-docker/docker ./
```

### 2. Build the image

```bash
cd docker
docker compose build
```

First build takes ~10-15 minutes. Subsequent builds use cached layers.

### 3. Extract map data (on host, not in container)

CascLib has issues with network filesystems. Run extractors natively on the host:

```bash
cd /path/to/wow/client
/path/to/mapextractor
/path/to/vmap4extractor
mkdir vmaps
/path/to/vmap4assembler Buildings vmaps
mkdir mmaps
/path/to/mmaps_generator

# Copy extracted data to local directory
mkdir -p ./tc-data
cp -r dbc maps gt cameras vmaps mmaps ./tc-data/
```

### 4. Configure

Edit `docker-compose.yml`:
- Set volume paths for your system (tc-etc, tc-data)
- Adjust database passwords if desired

Edit `entrypoint.sh`:
- Replace `YOUR_SERVER_IP` with your server's IP address

### 5. Start the stack

```bash
docker compose up -d
```

First start downloads TDB (master) and auto-populates databases (~5-10 minutes).

### 6. Create admin account

```bash
# Send command to worldserver console
docker exec trinitycore-worldserver-1 bash -c 'echo ".bnetaccount create admin@admin password" > /proc/$(pgrep -f worldserver | head -1)/fd/0'
# Set GM level
docker exec trinitycore-worldserver-1 bash -c 'echo ".account set gmlevel 1#1 3 -1" > /proc/$(pgrep -f worldserver | head -1)/fd/0'
```

## Configuration

### Volumes

| Volume | Purpose |
|--------|---------|
| `db_data` | MySQL data (persistent) |
| `tc_data` | Map/DBC data (read-write) |
| `tdb_data` | TDB extraction cache (persistent) |
| `./tc-etc` | Config files (bind mount, editable) |
| `./tc-data` | Map data (bind mount, read-write) |

### Ports

| Port | Service |
|------|---------|
| 1119 | Battle.net (bnetserver) |
| 8081 | Login REST API |
| 8085 | World server |
| 3306 | MySQL |

## Custom Scripts

Two custom scripts are included in `custom-scripts/`:

### SoloCraft
Scales creature health and damage for solo players in dungeons.
- Health multiplier: `5.0 / player_count`
- Damage multiplier: `3.0 / player_count`
- Only activates in dungeon/raid instances

### SoloLFG
Enables solo dungeon finder notifications.
- Notifies players on login that Solo LFG is available
- Logs solo group creation

### Installation
Copy the `.cpp` files to `TrinityCore/src/server/scripts/Custom/` and rebuild:
```bash
cp custom-scripts/*.cpp TrinityCore/src/server/scripts/Custom/
cd TrinityCore
mkdir build && cd build
cmake .. -DSCRIPTS=static -DSERVERS=1 -DTOOLS=1
make -j$(nproc)
```

## Troubleshooting

### SSL Handshake Failed
The bnetserver uses a self-signed certificate. Install the root CA on the client:
- Download `bnetserver.cert.pem` from the server
- Install in Windows Trusted Root Certification Authorities

### Realmlist Configuration
For remote clients, update the realmlist table:
```sql
UPDATE auth.realmlist SET address = 'YOUR_SERVER_IP', localAddress = 'YOUR_SERVER_IP' WHERE id = 1;
```

### No Map Data
The worldserver exits if DBC/map files are missing. Ensure `/data` contains `dbc/`, `maps/`, etc.

### Logs Not Showing
The entrypoint tails log files to stdout. Check `/data/logs/` inside the container.

## License

This project is provided as-is. TrinityCore is licensed under GPL v2.