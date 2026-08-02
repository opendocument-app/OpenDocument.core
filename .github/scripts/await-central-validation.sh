#!/usr/bin/env bash
# Waits for a central portal deployment to validate, and fails if it does not.
#
# `publishToMavenCentral` uploads and exits 0 whatever the portal makes of the
# bundle - the vanniktech plugin only validates when `automaticRelease` is on,
# which would also release it, and releasing is deliberately a human's job here.
# So the release path polls the portal itself. The maven jar gets this for free:
# the sonatype maven plugin blocks and reports.
#
#     await-central-validation.sh <gradle-log>
#
# Takes the gradle output to read the deployment id out of, and the portal token
# as MAVEN_CENTRAL_USERNAME / MAVEN_CENTRAL_PASSWORD.

set -euo pipefail

log="${1:?usage: await-central-validation.sh <gradle-log>}"
: "${MAVEN_CENTRAL_USERNAME:?}" "${MAVEN_CENTRAL_PASSWORD:?}"

deployment=$(grep -oE 'deployment id: [0-9a-fA-F-]{36}' "$log" | tail -1 | awk '{print $3}')
if [ -z "$deployment" ]; then
    echo "no deployment id in $log - did the upload actually run?" >&2
    exit 1
fi

token=$(printf '%s:%s' "$MAVEN_CENTRAL_USERNAME" "$MAVEN_CENTRAL_PASSWORD" | base64 | tr -d '\n')
echo "waiting on deployment ${deployment}"

# Validation is usually seconds; the ceiling is only here so a portal that never
# answers fails the release instead of hanging until github's own timeout.
for _ in $(seq 60); do
    response=$(curl -sS -X POST -H "Authorization: Bearer ${token}" \
        "https://central.sonatype.com/api/v1/publisher/status?id=${deployment}")
    state=$(printf '%s' "$response" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("deploymentState",""))')

    case "$state" in
        VALIDATED|PUBLISHING|PUBLISHED)
            echo "deployment ${deployment} is ${state}"
            exit 0
            ;;
        FAILED)
            echo "deployment ${deployment} failed validation:" >&2
            printf '%s\n' "$response" >&2
            exit 1
            ;;
        PENDING|VALIDATING|"")
            sleep 10
            ;;
        *)
            echo "unexpected deployment state '${state}':" >&2
            printf '%s\n' "$response" >&2
            exit 1
            ;;
    esac
done

echo "deployment ${deployment} still not validated after 10 minutes" >&2
exit 1
