#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
app_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "$app_dir/bin/kamakiri-studio" "$@"
