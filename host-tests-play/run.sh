#!/bin/sh
# Run the isolated CrossInk Play native suites. The link suite needs loopback UDP.
set -eu
cd "$(dirname "$0")/.."
for suite in host-tests-play/*/run.sh; do
  echo "Running $suite"
  sh "$suite"
done
