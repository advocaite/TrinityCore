#!/usr/bin/env bash
# Runs worldserver console commands over SOAP, for the spell test module (doc/Docker.md). compose.yaml enables SOAP and
# publishes its port on this machine only. The account (GM level 3) comes from soap.env next to this script, or from the
# environment: SOAP_USER, SOAP_PASS, and SOAP_URL (default http://127.0.0.1:7878/).
#
#   ./spelltest.sh run COMMAND...    runs each command and prints its output
#   ./spelltest.sh script FILE|-     one command per line; "sleep SECONDS" waits, "#" starts a comment
#
# Output: "> command" for a command that worked, "! command" for one that failed, then what the server answered.
set -euo pipefail
shopt -u patsub_replacement 2>/dev/null || true   # bash 5.2: keep "&" literal in ${var//pattern/replacement}

dir="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$dir/soap.env" ]; then
    # shellcheck source=/dev/null
    . "$dir/soap.env"
fi
SOAP_URL="${SOAP_URL:-http://127.0.0.1:7878/}"

usage() {
    echo "usage: $0 run COMMAND... | script FILE|-" >&2
    exit 2
}

unescape() {
    local text="$1"
    text="${text//&#xD;/}"
    text="${text//&#13;/}"
    text="${text//&#xA;/$'\n'}"
    text="${text//&#10;/$'\n'}"
    text="${text//&lt;/<}"
    text="${text//&gt;/>}"
    text="${text//&quot;/\"}"
    text="${text//&apos;/\'}"
    text="${text//&amp;/&}"
    printf '%s' "$text"
}

run_command() {
    local command="${1#.}" escaped body response mark output
    escaped="${command//&/&amp;}"
    escaped="${escaped//</&lt;}"
    escaped="${escaped//>/&gt;}"
    body='<?xml version="1.0" encoding="UTF-8"?><SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns1="urn:TC">'
    body+="<SOAP-ENV:Body><ns1:executeCommand><command>$escaped</command></ns1:executeCommand></SOAP-ENV:Body></SOAP-ENV:Envelope>"

    response="$(curl -sS --max-time 120 -u "$SOAP_USER:$SOAP_PASS" -H 'Content-Type: text/xml; charset=utf-8' \
        --data-binary "$body" "$SOAP_URL")"

    # A command's output comes in <result>. A failed command, or a wrong account or password, comes as a fault.
    if [[ "$response" == *"<result>"* ]]; then
        mark=">"
        output="${response#*<result>}"
        output="${output%%</result>*}"
    elif [[ "$response" == *"<faultstring>"* ]]; then
        mark="!"
        output="${response#*<faultstring>}"
        output="${output%%</faultstring>*}"
    else
        echo "! $command" >&2
        echo "Unexpected answer: ${response:0:300}" >&2
        return 1
    fi

    echo "$mark $command"
    output="$(unescape "$output")"
    output="${output%$'\n'}"
    if [ -n "$output" ]; then
        printf '%s\n' "$output"
    fi
}

[ $# -ge 1 ] || usage
: "${SOAP_USER:?set SOAP_USER and SOAP_PASS in $dir/soap.env or the environment}"
: "${SOAP_PASS:?set SOAP_PASS in $dir/soap.env or the environment}"

case "$1" in
    run)
        shift
        [ $# -ge 1 ] || usage
        for command in "$@"; do
            run_command "$command"
        done
        ;;
    script)
        [ $# -eq 2 ] || usage
        file="$2"
        [ "$file" = "-" ] && file=/dev/stdin
        while IFS= read -r line || [ -n "$line" ]; do
            line="${line#"${line%%[![:space:]]*}"}"
            line="${line%"${line##*[![:space:]]}"}"
            case "$line" in
                ""|"#"*) ;;
                "sleep "*) sleep "${line#sleep }" ;;
                *) run_command "$line" ;;
            esac
        done < "$file"
        ;;
    *)
        usage
        ;;
esac
