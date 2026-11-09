#!/usr/bin/env bash
set -euo pipefail

PREFIX=/opt/trinitycore
ETC=$PREFIX/etc
BIN=$PREFIX/bin
DATA=/data

# Environment variables with defaults
SERVER_IP="${SERVER_IP:-127.0.0.1}"
DB_HOST="${DB_HOST:-db}"
DB_PORT="${DB_PORT:-3306}"
DB_USER="${DB_USER:-trinity}"
DB_PASS="${DB_PASS:-trinity}"
DB_NAME_AUTH="${DB_NAME_AUTH:-auth}"
DB_NAME_WORLD="${DB_NAME_WORLD:-world}"
DB_NAME_CHAR="${DB_NAME_CHAR:-characters}"
DB_NAME_HOTFIX="${DB_NAME_HOTFIX:-hotfixes}"

# Ensure default config files exist (copy once from .dist)
for name in bnetserver worldserver; do
    if [ ! -f "$ETC/$name.conf" ] && [ -f "$ETC/$name.conf.dist" ]; then
        cp "$ETC/$name.conf.dist" "$ETC/$name.conf"
    fi
done

# Apply initial Docker configuration only on first run.
# After this, .conf files are left untouched so manual edits persist.
MARKER="$ETC/.docker-configured"
if [ ! -f "$MARKER" ]; then
    if [ -f "$ETC/bnetserver.conf" ]; then
        sed -i "s#^[[:space:]]*LoginDatabaseInfo[[:space:]]*=.*#LoginDatabaseInfo = \"${DB_HOST};${DB_PORT};${DB_USER};${DB_PASS};${DB_NAME_AUTH}\"#" "$ETC/bnetserver.conf" 2>/dev/null || true
        sed -i 's#^[[:space:]]*CertificatesFile[[:space:]]*=.*#CertificatesFile = "/opt/trinitycore/bin/bnetserver.cert.pem"#' "$ETC/bnetserver.conf" 2>/dev/null || true
        sed -i 's#^[[:space:]]*PrivateKeyFile[[:space:]]*=.*#PrivateKeyFile = "/opt/trinitycore/bin/bnetserver.key.pem"#' "$ETC/bnetserver.conf" 2>/dev/null || true
        sed -i "s#^[[:space:]]*LoginREST\\.ExternalAddress[[:space:]]*=.*#LoginREST.ExternalAddress = \"${SERVER_IP}\"#" "$ETC/bnetserver.conf" 2>/dev/null || true
        sed -i "s#^[[:space:]]*LoginREST\\.LocalAddress[[:space:]]*=.*#LoginREST.LocalAddress = \"${SERVER_IP}\"#" "$ETC/bnetserver.conf" 2>/dev/null || true
    fi
    if [ -f "$ETC/worldserver.conf" ]; then
        sed -i 's#^[[:space:]]*DataDir[[:space:]]*=.*#DataDir = "/data"#' "$ETC/worldserver.conf" 2>/dev/null || true
        sed -i 's#^[[:space:]]*SourceDirectory[[:space:]]*=.*#SourceDirectory = "/opt/trinitycore"#' "$ETC/worldserver.conf" 2>/dev/null || true
        sed -i 's#^[[:space:]]*MySQLExecutable[[:space:]]*=.*#MySQLExecutable = "/usr/bin/mysql"#' "$ETC/worldserver.conf" 2>/dev/null || true
        sed -i "s#^[[:space:]]*LoginDatabaseInfo[[:space:]]*=.*#LoginDatabaseInfo = \"${DB_HOST};${DB_PORT};${DB_USER};${DB_PASS};${DB_NAME_AUTH}\"#" "$ETC/worldserver.conf" 2>/dev/null || true
        sed -i "s#^[[:space:]]*WorldDatabaseInfo[[:space:]]*=.*#WorldDatabaseInfo = \"${DB_HOST};${DB_PORT};${DB_USER};${DB_PASS};${DB_NAME_WORLD}\"#" "$ETC/worldserver.conf" 2>/dev/null || true
        sed -i "s#^[[:space:]]*CharacterDatabaseInfo[[:space:]]*=.*#CharacterDatabaseInfo = \"${DB_HOST};${DB_PORT};${DB_USER};${DB_PASS};${DB_NAME_CHAR}\"#" "$ETC/worldserver.conf" 2>/dev/null || true
        sed -i "s#^[[:space:]]*HotfixDatabaseInfo[[:space:]]*=.*#HotfixDatabaseInfo = \"${DB_HOST};${DB_PORT};${DB_USER};${DB_PASS};${DB_NAME_HOTFIX}\"#" "$ETC/worldserver.conf" 2>/dev/null || true
        sed -i 's#^[[:space:]]*LogsDir[[:space:]]*=.*#LogsDir = "/data/logs"#' "$ETC/worldserver.conf" 2>/dev/null || true
        # Solo scripts configuration (added by TrinityCore Docker stack)
        if ! grep -q "SoloCraft.Enable" "$ETC/worldserver.conf" 2>/dev/null; then
            cat >> "$ETC/worldserver.conf" << 'SOLEOF'

# Solo Scripts (TrinityCore Docker)
SoloCraft.Enable = 1
SoloCraft.HealthMultiplier = 5.0
SoloCraft.DamageMultiplier = 3.0
SoloCraft.Announce = 1
SoloLFG.Enable = 1
SoloLFG.AllowSoloQueue = 1
SOLEOF
        fi
    fi
    touch "$MARKER"
    echo "Initial Docker configuration applied. Edit .conf files freely; changes persist across restarts."
fi

mkdir -p "$DATA" /data/logs

# Run the requested server binary
case "$1" in
    bnetserver)
        LOGFILE="/data/logs/Bnet.log"
        ;;
    worldserver)
        LOGFILE="/data/logs/Server.log"
        ;;
    *)
        LOGFILE=""
        ;;
esac

# Start server in background with stdin from a named pipe (allows docker exec commands)
# Tail its log to stdout for Docker log capture
if [ -n "$LOGFILE" ]; then
    PIPE="/tmp/worldserver.cmd"
    rm -f "$PIPE"
    mkfifo "$PIPE"
    "$BIN/$1" < "$PIPE" &
    SERVER_PID=$!
    (while true; do sleep 1; done) > "$PIPE" &
    PIPE_WRITER=$!
    for i in $(seq 1 60); do
        [ -f "$LOGFILE" ] && break
        sleep 1
    done
    tail -f "$LOGFILE" &
    TAIL_PID=$!
    wait $SERVER_PID
    kill $TAIL_PID $PIPE_WRITER 2>/dev/null
else
    exec "$BIN/$1"
fi