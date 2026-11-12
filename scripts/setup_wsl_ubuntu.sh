#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  protobuf-compiler \
  libprotobuf-dev \
  default-libmysqlclient-dev \
  mysql-server

echo "Dependencies installed."
echo "Next:"
echo "  mysql -u root -p < config/user.sql"
echo "  cmake -S . -B build -G Ninja"
echo "  cmake --build build"
echo "  ctest --test-dir build --output-on-failure"
