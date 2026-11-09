# TrinityCore (master) – Docker Compose stack (built from source)

This directory contains a Docker Compose stack that builds TrinityCore `master`
from the checked-out source tree and runs it with MySQL 8.4.

## Layout

- `Dockerfile` – multi-stage build (builder with all build tools, minimal runtime)
- `docker-compose.yml` – db, bnetserver, worldserver, tdb-init, extractor (tools profile)
- `entrypoint.sh` – prepares config files and points them at the stack's services
- `init.sql` – creates the `auth`, `characters`, `world`, `hotfixes` databases

## Build (once)

Build uses Docker layer cache for incremental rebuilds:

```bash
cd /home/trinitycore/TrinityCore/docker
docker compose build
```

## Extract Map/DBC Data (native, on host)

CascLib has issues with CIFS/SMB mounts. Run extractors natively on the host:

```bash
cd /mnt/WoWRetail
/home/trinitycore/tc-host-build/bin/Release/bin/mapextractor
/home/trinitycore/tc-host-build/bin/Release/bin/vmap4extractor
mkdir vmaps
/home/trinitycore/tc-host-build/bin/Release/bin/vmap4assembler Buildings vmaps
mkdir mmaps
/home/trinitycore/tc-host-build/bin/Release/bin/mmaps_generator

# Copy extracted data to local filesystem (avoiding SMB for worldserver)
sudo mkdir -p /home/trinitycore/tc-data
sudo cp -r /mnt/WoWRetail/dbc /mnt/WoWRetail/maps /mnt/WoWRetail/gt \
           /mnt/WoWRetail/cameras /mnt/WoWRetail/vmaps /mnt/WoWRetail/mmaps \
           /home/trinitycore/tc-data/
sudo chown -R trinitycore:trinitycore /home/trinitycore/tc-data
```

## Run

```bash
cd /home/trinitycore/TrinityCore/docker
docker compose up -d
```

First start downloads TDB (master) and auto-populates the databases (may take a while).

## Ports

- `1119` – Battle.net (bnetserver)
- `8081` – Login REST
- `8085` – World server
- `3306` – MySQL

## Notes

- Default DB credentials: `trinity` / `trinity` (change in `init.sql` and conf).
- Map data is mounted from `/home/trinitycore/tc-data` (local, not SMB).
- Building master requires MySQL >= 8.0.34, Boost >= 1.74, OpenSSL >= 3.x,
  CMake >= 3.24, Clang >= 11 (used here) or GCC >= 11.1.
