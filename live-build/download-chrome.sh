#!/usr/bin/env bash
set -euo pipefail

destination="$(realpath "${1:?Expected the destination directory}")"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
chmod 0755 "$work"

# Limit trust to Google's published Linux package signing key.
install -d -m 0700 "$work/gnupg"
curl --fail --silent --show-error --location --retry 2 \
    https://dl.google.com/linux/linux_signing_key.pub -o "$work/google.asc"
gpg --batch --homedir "$work/gnupg" --import "$work/google.asc"
gpg --batch --homedir "$work/gnupg" --export EB4C1BFD4F042F6DDDCCEC917721F63BD38B4796 > "$work/google.gpg"
test -s "$work/google.gpg"

mkdir "$work/lists"
printf 'deb [arch=amd64 signed-by=%s] https://dl.google.com/linux/chrome-stable/deb/ stable main\n' \
    "$work/google.gpg" > "$work/google.list"
apt_options=(
    -o "Dir::Etc::sourcelist=$work/google.list"
    -o "Dir::Etc::sourceparts=-"
    -o "Dir::State::lists=$work/lists"
    -o "APT::Architecture=amd64"
    -o "APT::Update::Error-Mode=any"
)
apt-get "${apt_options[@]}" update
install -d -m 1777 "$work/download"
cd "$work/download"
apt-get "${apt_options[@]}" download google-chrome-stable:amd64
install -m 0644 google-chrome-stable_*_amd64.deb "$destination/"
