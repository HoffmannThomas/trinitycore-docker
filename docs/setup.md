# Server Setup Guide

Detailed setup guide for the TrinityCore Docker stack.

## 1. Create the trinitycore user

```bash
# As root on the server
useradd -m -s /bin/bash trinitycore
usermod -aG sudo,docker trinitycore
echo 'trinitycore ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/trinitycore
chmod 440 /etc/sudoers.d/trinitycore
```

## 2. Set up SSH access

```bash
# As trinitycore user
mkdir -p ~/.ssh
chmod 700 ~/.ssh
# Add your SSH public key to ~/.ssh/authorized_keys
```

## 3. Mount WoW client data (CIFS/SMB)

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

## 4. Clone and configure

```bash
# As trinitycore user
cd /home/trinitycore

# 1. Clone TrinityCore source
git clone https://github.com/TrinityCore/TrinityCore.git
cd TrinityCore

# 2. Clone this docker stack INTO the TrinityCore repo as 'docker/'
git clone https://github.com/HoffmannThomas/trinitycore-docker.git docker

# 3. Configure
cp docker/.env.example docker/.env
# Edit docker/.env with your values (SERVER_IP, paths, passwords, etc.)
```

## 5. Build the image

```bash
cd docker
docker compose build
```

First build takes ~10-15 minutes. Subsequent builds use cached layers.

## 6. Build extraction tools (on host)

The Docker image includes the tools, but CascLib has issues with network filesystems (CIFS/SMB).
For WoW clients on a network share, build and run the tools natively on the host:

```bash
cd /home/trinitycore/TrinityCore
mkdir -p tc-host-build && cd tc-host-build
cmake .. -DTOOLS=1 -DSERVERS=0 -DSCRIPTS=0 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
make -j$(nproc)
# Tools are in: tc-host-build/bin/Release/bin/
```

## 7. Extract map data (on host, not in container)

Run extractors from the WoW client directory:

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

## 8. Start the stack

```bash
cd /home/trinitycore/TrinityCore/docker
docker compose up -d
```

First start downloads TDB (master) and auto-populates databases (~5-10 minutes).

## 9. Create admin account

```bash
# Send command to worldserver console
docker exec trinitycore-worldserver-1 bash -c 'echo ".bnetaccount create admin@admin password" > /proc/$(pgrep -f worldserver | head -1)/fd/0'
# Set GM level
docker exec trinitycore-worldserver-1 bash -c 'echo ".account set gmlevel 1#1 3 -1" > /proc/$(pgrep -f worldserver | head -1)/fd/0'
```

## Troubleshooting

### SSL Handshake Failed
- Ensure the certificate is installed in "Trusted Root Certification Authorities" (Local Machine)
- Restart the WoW client after installing
- Check Burralis settings for certificate verification options

### Realmlist Configuration
For remote clients, update the realmlist table:
```sql
UPDATE auth.realmlist SET address = 'YOUR_SERVER_IP', localAddress = 'YOUR_SERVER_IP' WHERE id = 1;
```

### No Map Data
The worldserver exits if DBC/map files are missing. Ensure `/data` contains `dbc/`, `maps/`, etc.

### Logs Not Showing
The entrypoint tails log files to stdout. Check `/data/logs/` inside the container.

### Burralis Connection Issues
- Ensure Burralis is configured with the correct server IP
- Check that port 1119 is not blocked by Windows Firewall
- Verify the WoW client version matches the server (12.1.5.70783)