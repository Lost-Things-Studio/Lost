#!/bin/bash
# Runs the in-game checks of Tests/GameChecks with a built OvGame. Each scene of the project is a check, run through the
# game's command line: it passes when the game ends with exit code 0 and logs "CHECK PASSED <name>".
# Usage: Tests/run_game_checks.sh [--packed] <OvGame binary> [check name...]
# With --packed, the project's assets and the engine's resources are packed first, by the LostPack built next to the
# game, and the checks read them from the packages alone: no loose asset is left to fall back on.
# The game needs a display: a desktop session, or xvfb-run on a headless Linux machine. On Linux, the saves of the
# checks go to a temporary folder, never to the user's own; elsewhere, they go to the save folder of the project's own
# ID (GameChecks.loproject's project_id), apart from any game's. The system's language is set aside.
set -u

PACKED=0
if [ "${1:-}" = "--packed" ]; then
	PACKED=1
	shift
fi

if [ $# -lt 1 ]; then
	echo "Usage: $0 [--packed] <OvGame binary> [check name...]" >&2
	exit 2
fi

if [ ! -x "$1" ]; then
	echo "No game to run at '$1': build OvGame first" >&2
	exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
shift

if [ $# -gt 0 ]; then
	CHECKS=("$@")
else
	CHECKS=()
	# A check is a scene, under Lost's name or Overload's (LegacyFormats keeps Overload's)
	for SCENE in "$ROOT/Tests/GameChecks/Assets/Scenes/"*.loscene "$ROOT/Tests/GameChecks/Assets/Scenes/"*.ovscene; do
		[ -e "$SCENE" ] || continue
		SCENE="$(basename "$SCENE")"
		CHECKS+=("${SCENE%.*}")
	done
fi

DATA="$(mktemp -d)"
trap 'rm -rf "$DATA"' EXIT
export XDG_DATA_HOME="$DATA"

# Where the checks read their assets: the source tree, or packages in a folder of their own
PROJECT="$ROOT/Tests/GameChecks"
ENGINE="$ROOT/Resources/Engine"
if [ $PACKED -eq 1 ]; then
	LOSTPACK="$(dirname "$GAME")/../LostPack/LostPack"
	if [ ! -x "$LOSTPACK" ]; then
		echo "No LostPack next to the game, at '$LOSTPACK': build it first" >&2
		exit 2
	fi

	PROJECT="$DATA/Packed/GameChecks"
	ENGINE="$DATA/Packed/Engine"
	mkdir -p "$PROJECT"
	cp "$ROOT/Tests/GameChecks/"*.loproject "$PROJECT/"
	"$LOSTPACK" pack "$ROOT/Tests/GameChecks/Assets" "$PROJECT/Assets.lopak" --project-id GameChecks > /dev/null &&
		"$LOSTPACK" pack "$ROOT/Resources/Engine" "$ENGINE.lopak" > /dev/null || exit 2
	echo "Reading the assets from $PROJECT/Assets.lopak and $ENGINE.lopak"
fi

# The checks start in the project's default language, whatever the machine's
export LANG=C.UTF-8
unset LC_ALL LC_MESSAGES

FAILED=0
for NAME in "${CHECKS[@]}"; do
	LOG="$DATA/$NAME.log"
	# A check ends the game itself: the frame cap only stops one that never does. The physics steps in real time,
	# so a check that waits for it needs as many frames as the game draws in that time
	SCENE="Scenes/$NAME.loscene"
	[ -e "$ROOT/Tests/GameChecks/Assets/$SCENE" ] || SCENE="Scenes/$NAME.ovscene"
	timeout 300 "$GAME" --project "$PROJECT" --engine "$ENGINE" \
		--scene "$SCENE" --frames 20000 --hidden-window --fail-on-error > "$LOG" 2>&1
	CODE=$?
	VERDICT="$(sed 's/\x1b\[[0-9;]*m//g' "$LOG" | grep -o "CHECK PASSED $NAME .*" | head -1)"

	if [ $CODE -eq 0 ] && [ -n "$VERDICT" ]; then
		echo "PASS $NAME: ${VERDICT#CHECK PASSED $NAME }"
	else
		REASON="exit code $CODE"
		[ $CODE -eq 124 ] && REASON="stopped after 300 s (timeout)"
		echo "FAIL $NAME: $REASON${VERDICT:+, though it logged its pass}"

		# The errors and the checks' lines, or else the end of the log
		LINES="$(sed 's/\x1b\[[0-9;]*m//g' "$LOG" | grep -E "\[ERROR\]|CHECK" | head -20)"
		[ -n "$LINES" ] || LINES="$(sed 's/\x1b\[[0-9;]*m//g' "$LOG" | tail -20)"
		printf '%s\n' "$LINES" | sed 's/^/    /'
		FAILED=1
	fi
done

exit $FAILED
