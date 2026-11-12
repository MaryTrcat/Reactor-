#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if [[ "$(id -u)" -eq 0 ]]; then
  SUDO=""
else
  SUDO="sudo"
fi

$SUDO service mysql status >/dev/null 2>&1 || $SUDO service mysql start
$SUDO mysql < config/user.sql
$SUDO mysql <<'SQL'
CREATE USER IF NOT EXISTS 'reactor_user'@'127.0.0.1' IDENTIFIED BY 'reactor_pass';
GRANT ALL PRIVILEGES ON reactor_user_service.* TO 'reactor_user'@'127.0.0.1';
FLUSH PRIVILEGES;
TRUNCATE TABLE reactor_user_service.users;
SQL

pkill -f reactor_user_server >/dev/null 2>&1 || true

./build-linux/reactor_user_server config/server.conf > /tmp/reactor_user_server.out 2>&1 &
server_pid=$!

cleanup() {
  kill "$server_pid" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 1
./build-linux/test_client 127.0.0.1 9000

echo "--- mysql users ---"
$SUDO mysql reactor_user_service -e "SELECT id, username, role, created_at FROM users;"

echo "--- server log ---"
cat /tmp/reactor_user_server.out
