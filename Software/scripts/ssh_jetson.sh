#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
SECRETS_FILE="${REPO_ROOT}/secrets.env"

if [[ -f "${SECRETS_FILE}" ]]; then
	set -a
	# shellcheck source=/dev/null
	source "${SECRETS_FILE}"
	set +a
fi

: "${JETSON_SSH_HOST:?Set JETSON_SSH_HOST in ${SECRETS_FILE} or the environment}"
: "${JETSON_SSH_USER:?Set JETSON_SSH_USER in ${SECRETS_FILE} or the environment}"
: "${JETSON_SSH_PASSWORD:?Set JETSON_SSH_PASSWORD in ${SECRETS_FILE} or the environment}"
export JETSON_SSH_PASSWORD

if ! command -v expect >/dev/null 2>&1; then
	echo "error: expect is required to auto-enter the Jetson SSH password" >&2
	exit 1
fi

exec expect -f /dev/stdin "${JETSON_SSH_USER}@${JETSON_SSH_HOST}" "$@" <<'EXPECT_SCRIPT'
	set timeout -1
	set password $env(JETSON_SSH_PASSWORD)

	spawn ssh {*}$argv

	expect {
		-re "(?i)are you sure you want to continue connecting" {
			send -- "yes\r"
			exp_continue
		}
		-re "(?i)password:" {
			send -- "$password\r"
		}
		eof {
			catch wait result
			exit [lindex $result 3]
		}
	}

	interact
	catch wait result
	exit [lindex $result 3]
EXPECT_SCRIPT
