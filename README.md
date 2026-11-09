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
- **Environment-based config** — all server-specific values in `.env` (gitignored)

## Prerequisites

- Docker Engine 24+ with Docker Compose v2
- 8+ CPU cores, 16GB+ RAM, 50GB+ free disk space
- A WoW 12.1.5 client for map extraction
- A Linux server with a `trinitycore` user

## Server Setup

### 1. Create the trinitycore user

```bash
# As root on the server
useradd -m -s /bin/bash trinitycore
usermod -aG sudo,docker trinitycore
echo 'trinitycore ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/trinitycore
chmod 440 /etc/sudoers.d/trinitycore
```

### 2. Set up SSH access

```bash
# As trinitycore user
mkdir -p ~/.ssh
chmod 700 ~/.ssh
# Add your SSH public key to ~/.ssh/authorized_keys
```

### 3. Mount WoW client data (CIFS/SMB)

The WoW client is typically on a Windows machine. Mount it via CIFS:

```bash
# Install cifs-utils
sudo apt-get install -y cifs-utils

# Create mount point
sudo mkdir -p /mnt/wow-client

# Create credentials file
sudo bash -c 'echo "username=smb_user" > /root/.smbcredentials'
sudo bash -c 'echo "password=smb_pass" >> /root/.smbcredentials'
sudo chmod 600 /root/.smbcredentials

# Add to /etc/fstab
echo "//WINDOWS_IP/WoWShare /mnt/wow-client cifs credentials=/root/.smbcredentials,uid=1000,gid=1000,vers=3.0,nofail 0 0" | sudo tee -a /etc/fstab

# Mount
sudo mount -a
```

### 4. Clone and configure

```bash
# As trinitycore user
cd /home/trinitycore

# 1. Clone TrinityCore source
git clone https://github.com/TrinityCore/TrinityCore.git
cd TrinityCore

# 2. Clone this docker stack INTO the TrinityCore repo as 'docker/'
git clone https://github.com/HoffmannThomas/trinitycore-docker.git docker
# Or: cp -r /path/to/trinitycore-docker/docker ./

# 3. Configure
cp docker/.env.example docker/.env
# Edit docker/.env with your values (SERVER_IP, paths, passwords, etc.)
```

### 5. Build the image

```bash
cd docker
docker compose build
```

First build takes ~10-15 minutes. Subsequent builds use cached layers.

### 6. Extract map data (on host, not in container)

CascLib has issues with network filesystems. Run extractors natively:

```bash
cd /mnt/wow-client
/home/trinitycore/tc-host-build/bin/Release/bin/mapextractor
/home/trinitycore/tc-host-build/bin/Release/bin/vmap4extractor
mkdir vmaps
/home/trinitycore/tc-host-build/bin/Release/bin/vmap4assembler Buildings vmaps
mkdir mmaps
/home/trinitycore/tc-host-build/bin/Release/bin/mmaps_generator

# Copy extracted data to local directory
mkdir -p /home/trinitycore/tc-data
cp -r dbc maps gt cameras vmaps mmaps /home/trinitycore/tc-data/
```

### 7. Start the stack

```bash
cd /home/trinitycore/trinitycore-docker/docker
docker compose up -d
```

First start downloads TDB (master) and auto-populates databases (~5-10 minutes).

### 8. Create admin account

```bash
# Send command to worldserver console
docker exec trinitycore-worldserver-1 bash -c 'echo ".bnetaccount create admin@admin password" > /proc/$(pgrep -f worldserver | head -1)/fd/0'
# Set GM level
docker exec trinitycore-worldserver-1 bash -c 'echo ".account set gmlevel 1#1 3 -1" > /proc/$(pgrep -f worldserver | head -1)/fd/0'
```

## Configuration

### Environment Variables (.env)

| Variable | Default | Description |
|----------|---------|-------------|
| `SERVER_IP` | `127.0.0.1` | Server's public IP |
| `SERVER_USER` | `trinitycore` | Server user |
| `TRINITYCORE_SRC` | `/home/trinitycore/TrinityCore` | TrinityCore source path |
| `TC_ETC_PATH` | `./tc-etc` | Config files path (bind mount) |
| `TC_DATA_PATH` | `./tc-data` | Map data path (bind mount) |
| `WOW_CLIENT_PATH` | `/mnt/wow-client` | WoW client mount path |
| `MYSQL_ROOT_PASSWORD` | `trinity` | MySQL root password |
| `DB_USER` | `trinity` | Database user |
| `DB_PASS` | `trinity` | Database password |
| `CERT_PATH` | `./certs` | Certificate path |

### Volumes

| Volume | Purpose |
|--------|---------|
| `db_data` | MySQL data (persistent) |
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