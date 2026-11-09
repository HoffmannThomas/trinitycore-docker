#!/usr/bin/env bash
set -euo pipefail

PREFIX=/opt/trinitycore
ETC=$PREFIX/etc
BIN=$PREFIX/bin
DATA=/data

# Ensure default config files exist
for name in bnetserver worldserver; do
    if [ ! -f "$ETC/$name.conf" ] && [ -f "$ETC/$name.conf.dist" ]; then
        cp "$ETC/$name.conf.dist" "$ETC/$name.conf"
    fi
done

# Point database connections at the stack's db service
if [ -f "$ETC/bnetserver.conf" ]; then
    sed -i 's#^[[:space:]]*LoginDatabaseInfo[[:space:]]*=.*#LoginDatabaseInfo = "db;3306;trinity;trinity;auth"#' "$ETC/bnetserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*CertificatesFile[[:space:]]*=.*#CertificatesFile = "/opt/trinitycore/bin/bnetserver.cert.pem"#' "$ETC/bnetserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*PrivateKeyFile[[:space:]]*=.*#PrivateKeyFile = "/opt/trinitycore/bin/bnetserver.key.pem"#' "$ETC/bnetserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*LoginREST\.ExternalAddress[[:space:]]*=.*#LoginREST.ExternalAddress = "YOUR_SERVER_IP"#' "$ETC/bnetserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*LoginREST\.LocalAddress[[:space:]]*=.*#LoginREST.LocalAddress = "YOUR_SERVER_IP"#' "$ETC/bnetserver.conf" 2>/dev/null || true
fi
if [ -f "$ETC/worldserver.conf" ]; then
    sed -i 's#^[[:space:]]*DataDir[[:space:]]*=.*#DataDir = "/data"#' "$ETC/worldserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*SourceDirectory[[:space:]]*=.*#SourceDirectory = "/opt/trinitycore"#' "$ETC/worldserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*MySQLExecutable[[:space:]]*=.*#MySQLExecutable = "/usr/bin/mysql"#' "$ETC/worldserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*LoginDatabaseInfo[[:space:]]*=.*#LoginDatabaseInfo = "db;3306;trinity;trinity;auth"#' "$ETC/worldserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*WorldDatabaseInfo[[:space:]]*=.*#WorldDatabaseInfo = "db;3306;trinity;trinity;world"#' "$ETC/worldserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*CharacterDatabaseInfo[[:space:]]*=.*#CharacterDatabaseInfo = "db;3306;trinity;trinity;characters"#' "$ETC/worldserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*HotfixDatabaseInfo[[:space:]]*=.*#HotfixDatabaseInfo = "db;3306;trinity;trinity;hotfixes"#' "$ETC/worldserver.conf" 2>/dev/null || true
    sed -i 's#^[[:space:]]*LogsDir[[:space:]]*=.*#LogsDir = "/data/logs"#' "$ETC/worldserver.conf" 2>/dev/null || true
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