#!/usr/bin/env bash
# One-time setup of the self-hosted CI runner on the jumpbox (Phase 1b).
#
# Run on the jumpbox as chris (not with sudo; it asks for the password once):
#     bash setup-ci-runner.sh
#
# What it does, and why:
#   * Creates an unprivileged user, q2ci, to run CI jobs. Jobs execute PR code,
#     so they must not run as chris: chris's home holds GH_TOKEN and
#     HC_PING_URL (~/.config/q2red/env), Steam, and the q2cycle loop.
#   * Copies the retail files the game tests need (pak0.pak, Q2Game.kpf) into
#     /srv/q2ci/retail, read-only to q2ci. Retail data never leaves this box:
#     no workflow may upload it as an artifact.
#   * Makes /srv/q2ci/engines, where chris installs pinned Q2RED and ozbot
#     builds (scripts/jumpbox/install-engines.sh). q2ci can read, not write.
#   * Installs the GitHub Actions runner (pinned version, checksum verified)
#     under /srv/q2ci/runner as a systemd service running as q2ci, labelled
#     q2-game-tests. SELinux is enforcing on this box, so the runner directory
#     is labelled bin_t for systemd to execute it.
#
# Re-running is safe: every step checks whether it is already done.
set -euo pipefail

REPO=ozy24/axiom
RUNNER_USER=q2ci
RUNNER_LABEL=q2-game-tests
RUNNER_VERSION=2.338.0
RUNNER_SHA256=af4b794c1bc41d73d40535e3fe092a39f9679cd8d965954c2aca25a05ca41d32
RETAIL="$HOME/.local/share/Steam/steamapps/common/Quake 2/rerelease"
SRV=/srv/q2ci
RUNNER_DIR=$SRV/runner

[[ $EUID -ne 0 ]] || { echo "Run as chris, not root: the script uses sudo where needed."; exit 1; }
for f in "$RETAIL/baseq2/pak0.pak" "$RETAIL/Q2Game.kpf"; do
	[[ -r "$f" ]] || { echo "Missing retail file: $f"; exit 1; }
done
gh api "repos/$REPO" -q .full_name >/dev/null

sudo -v

echo "== user"
if ! id "$RUNNER_USER" &>/dev/null; then
	sudo useradd --create-home --shell /bin/bash --comment "Game CI runner" "$RUNNER_USER"
fi

echo "== directories and retail data"
# 0751: chris must traverse it to reach engines/; retail/ stays closed to others.
sudo install -d -o root -g "$RUNNER_USER" -m 0751 "$SRV"
sudo install -d -o root -g "$RUNNER_USER" -m 0750 "$SRV/retail" "$SRV/retail/baseq2"
sudo install -d -o chris -g "$RUNNER_USER" -m 2750 "$SRV/engines"
sudo install -d -o "$RUNNER_USER" -g "$RUNNER_USER" -m 0750 "$SRV/work" "$RUNNER_DIR"
sudo install -o root -g "$RUNNER_USER" -m 0440 "$RETAIL/baseq2/pak0.pak" "$SRV/retail/baseq2/pak0.pak"
sudo install -o root -g "$RUNNER_USER" -m 0440 "$RETAIL/Q2Game.kpf" "$SRV/retail/Q2Game.kpf"

echo "== runner $RUNNER_VERSION"
if [[ ! -x "$RUNNER_DIR/config.sh" ]]; then
	tmp=$(mktemp -d)
	trap 'rm -rf "$tmp"' EXIT
	curl -fsSL -o "$tmp/runner.tgz" \
		"https://github.com/actions/runner/releases/download/v$RUNNER_VERSION/actions-runner-linux-x64-$RUNNER_VERSION.tar.gz"
	echo "$RUNNER_SHA256  $tmp/runner.tgz" | sha256sum -c -
	chmod 0755 "$tmp" && chmod 0644 "$tmp/runner.tgz"
	sudo -u "$RUNNER_USER" tar -xzf "$tmp/runner.tgz" -C "$RUNNER_DIR"
	sudo "$RUNNER_DIR/bin/installdependencies.sh"
fi

echo "== SELinux labels"
if ! sudo semanage fcontext -l | grep -q "^$RUNNER_DIR(/.\*)?"; then
	sudo semanage fcontext -a -t bin_t "$RUNNER_DIR(/.*)?"
fi
sudo restorecon -R "$RUNNER_DIR"

echo "== register with $REPO"
if [[ ! -f "$RUNNER_DIR/.runner" ]]; then
	token=$(gh api -X POST "repos/$REPO/actions/runners/registration-token" -q .token)
	sudo -u "$RUNNER_USER" env RUNNER_TOKEN="$token" bash -c \
		"cd '$RUNNER_DIR' && ./config.sh --unattended --url 'https://github.com/$REPO' \
		 --token \"\$RUNNER_TOKEN\" --name jumpbox --labels '$RUNNER_LABEL' \
		 --work '$SRV/work' --replace"
fi

echo "== service"
if ! sudo test -f "$RUNNER_DIR/.service"; then
	sudo bash -c "cd '$RUNNER_DIR' && ./svc.sh install '$RUNNER_USER'"
fi
sudo bash -c "cd '$RUNNER_DIR' && ./svc.sh start" >/dev/null
sleep 5
sudo bash -c "cd '$RUNNER_DIR' && ./svc.sh status" | grep -E "Active:" || true

echo
echo "Done. Check the runner shows as Idle:"
echo "  gh api repos/$REPO/actions/runners -q '.runners[] | .name + \" \" + .status'"
