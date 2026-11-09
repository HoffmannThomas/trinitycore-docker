# Plan: Unsanitized Repo with Environment-Based Configuration

## Problem
Sanitizing every push is cumbersome and prevents direct checkout on the server.

## Solution
Move all environment-specific values (IP, paths, credentials) into `.env` and config files that are **gitignored**, while keeping templates committed. The repo assumes a `trinitycore` user exists.

## Changes Required

### 1. Repository Structure
```
trinitycore-docker/
├── .env.example              # Template with placeholders (committed)
├── .env                      # Real values (gitignored)
├── .gitignore                # Ignore .env, tc-etc/, tc-data/, certs/
├── README.md                 # Setup instructions (create trinitycore user first)
├── custom-scripts/
│   ├── custom_script_loader.cpp
│   ├── solocraft.cpp
│   └── solo_lfg.cpp
└── docker/
    ├── .dockerignore
    ├── Dockerfile
    ├── docker-compose.yml     # Uses ${VAR} from .env
    ├── entrypoint.sh          # Uses env vars, no hardcoded IPs
    ├── init.sql
    └── README.md
```

### 2. Environment Variables (.env)
Move all real data into `.env`:
```
# Server configuration
SERVER_IP=192.168.1.100
SERVER_USER=trinitycore
SERVER_HOME=/home/trinitycore

# Paths (on host)
TRINITYCORE_SRC=/home/trinitycore/TrinityCore
TC_ETC_PATH=/home/trinitycore/tc-etc
TC_DATA_PATH=/home/trinitycore/tc-data
WOW_CLIENT_PATH=/mnt/wow-client

# Database
MYSQL_ROOT_PASSWORD=trinity
DB_USER=trinity
DB_PASS=trinity

# Certificate paths
CERT_PATH=/home/trinitycore/certs
```

### 3. docker-compose.yml Changes
- Use `${SERVER_IP}`, `${TC_ETC_PATH}`, `${TC_DATA_PATH}`, etc.
- No hardcoded paths or IPs
- Mount paths use `${WOW_CLIENT_PATH}` for extractor

### 4. entrypoint.sh Changes
- Read `SERVER_IP` from environment (passed via compose)
- Use env vars for all paths
- No hardcoded IPs

### 5. .gitignore Updates
```
.env
tc-etc/
tc-data/
certs/
*.local
```

### 6. CIFS/SMB Mount (for WoW client data)
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

Then set in .env:
```
WOW_CLIENT_PATH=/mnt/wow-client
```

### 7. README.md Updates
- Add "Prerequisites: Create trinitycore user" section
- Add "CIFS/SMB mount instructions for WoW client data"
- Add "Configuration: Copy .env.example to .env and fill in values"
- Add "Direct checkout on server" instructions

### Files to Modify
1. Create `.env.example`
2. Update `docker-compose.yml` to use env vars
3. Update `entrypoint.sh` to use env vars
4. Update `.gitignore`
5. Update `README.md` with user creation + CIFS mount + config instructions
6. Remove sanitized placeholders, use real structure

### Benefits
- Repo can be cloned directly on the server
- No manual sanitization needed
- Config changes are local (gitignored)
- Contributions can be pulled without conflicts
- Clear separation of code (committed) vs config (local)
- WoW client data mount documented for extraction
