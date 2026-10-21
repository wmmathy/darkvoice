#!/usr/bin/env bash
set -euo pipefail
# Debian cloud instance setup, retaining OS signature/TLS verification.
mkdir -p /workspace/.apt/lists/partial /workspace/.apt/archives/partial /workspace/sysroot
if [[ ! -x /workspace/.darkvoice-tools/cmake/data/bin/cmake ]]; then
  python3 -m pip install --target /workspace/.darkvoice-tools cmake==3.31.6 ninja==1.11.1.4
fi
if [[ ! -x /workspace/.quality-tools/clang_format/data/bin/clang-format ]]; then
  python3 -m pip install --target /workspace/.quality-tools clang-format==19.1.7
fi
if [[ ! -d /workspace/dependencies/JUCE/.git ]]; then
  git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git /workspace/dependencies/JUCE
fi
[[ $(git -C /workspace/dependencies/JUCE rev-parse HEAD) == 51a8a6d7aeae7326956d747737ccf1575e61e209 ]]
[[ -z $(git -C /workspace/dependencies/JUCE status --porcelain) ]]
cat > /workspace/.apt/sources.list <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie-updates main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://security.debian.org/debian-security trixie-security main
SOURCES
apt_options=(-o Dir::Etc::sourcelist=/workspace/.apt/sources.list -o Dir::Etc::sourceparts=-
 -o Dir::State::lists=/workspace/.apt/lists -o Dir::Cache::archives=/workspace/.apt/archives
 -o Debug::NoLocking=1 -o APT::Sandbox::User=agent)
apt-get "${apt_options[@]}" update
apt-get "${apt_options[@]}" --download-only --no-install-recommends -y install \
 libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
 libfreetype-dev libfontconfig-dev libgl1-mesa-dev xvfb xauth cppcheck
python3 - <<'PY'
from pathlib import Path
import subprocess
root=Path('/workspace/sysroot')
for package in Path('/workspace/.apt/archives').glob('*.deb'):
    subprocess.run(['dpkg-deb','-x',str(package),str(root)],check=True)
# Development .so links refer to already-installed OS runtime libraries.
for path in root.rglob('*'):
    if path.is_symlink() and not path.exists():
        target=path.readlink()
        if not target.is_absolute():
            installed=Path('/')/path.relative_to(root).parent/target
            destination=path.parent/target
            if installed.exists() and not destination.exists():
                destination.parent.mkdir(parents=True,exist_ok=True)
                destination.symlink_to(installed)
PY
