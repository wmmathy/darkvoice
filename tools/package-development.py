#!/usr/bin/env python3
"""Prepare unsigned developer artifacts with complete pinned source, offline."""
import argparse
import hashlib
import json
import os
import platform
import plistlib
import posixpath
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path, PurePosixPath

JUCE_COMMIT = '51a8a6d7aeae7326956d747737ccf1575e61e209'
STATUS = ('Unsigned development builds; no release, signing, notarization, '
          'GarageBand/Logic compatibility or listening qualification. '
          'Mastering, pitch/correction/harmony and other specification work remain incomplete.\n')


def git(root, *arguments):
    environment = os.environ.copy()
    environment['GIT_NO_LAZY_FETCH'] = '1'
    return subprocess.check_output(['git', '-C', str(root), *arguments],
                                   text=True, env=environment).strip()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def inside(path, root):
    return path == root or root in path.parents


def no_symlink_path(path):
    require(not any(item.is_symlink() for item in (path, *path.parents)),
            'Input/output paths must not traverse symbolic links')


def clean_repository(root, dependency=False):
    require(Path(git(root, 'rev-parse', '--show-toplevel')).resolve() == root,
            'Source path must be a repository root')
    arguments = ['status', '--porcelain', '--untracked-files=all']
    if dependency:
        arguments.append('--ignored')
    require(not git(root, *arguments), 'Source repository contains modified or untracked files')


def safe_member(member, prefix):
    path = PurePosixPath(member.name)
    require(not path.is_absolute() and '..' not in path.parts and
            member.name.startswith(prefix + '/'), 'Unsafe source archive path')
    require(not any(part in {'.git', '.aws', '.codex', '.agents'} or
                    part == '.env' or part.startswith('.env.') for part in path.parts),
            'Source archive contains environment or credential configuration')
    require(member.isdir() or member.isfile() or member.issym(),
            'Unsupported source archive entry')
    if member.issym():
        target = posixpath.normpath(posixpath.join(str(path.parent), member.linkname))
        require(not posixpath.isabs(member.linkname) and
                (target == prefix or target.startswith(prefix + '/')),
                'Source archive contains an external symbolic link')


def append_git_archive(destination, root, prefix, temporary):
    archive = temporary / ('juce.tar' if prefix.endswith('/JUCE') else 'project.tar')
    environment = os.environ.copy()
    environment['GIT_NO_LAZY_FETCH'] = '1'
    with archive.open('wb') as stream:
        subprocess.run(['git', '-C', str(root), 'archive', '--format=tar',
                        '--prefix=' + prefix + '/', 'HEAD'], stdout=stream, check=True, env=environment)
    with tarfile.open(archive, 'r:') as source:
        for member in source:
            # git archive includes an entry for the prefix directory itself.
            if member.name.rstrip('/') == prefix:
                require(member.isdir(), 'Invalid archive prefix')
                continue
            safe_member(member, prefix)
            if prefix == 'darkvoice-source':
                require(not member.name.startswith('darkvoice-source/dependencies/JUCE'),
                        'Project already contains the bundled JUCE destination')
                require(not (member.name.rstrip('/') == 'darkvoice-source/dependencies' and
                             member.issym()), 'Dependency destination traverses a symbolic link')
            destination.addfile(member, source.extractfile(member) if member.isfile() else None)


def product_paths(build, configuration, target_platform):
    products = []
    for target, name in [('DarkVoiceVocal', 'DarkVoice Vocal'),
                         ('DarkVoiceMaster', 'DarkVoice Master')]:
        root = build / (target + '_artefacts') / configuration
        products.append(root / 'VST3' / (name + '.vst3'))
        if target_platform == 'macos':
            products.append(root / 'AU' / (name + '.component'))
    standalone = build / 'DarkVoiceStudio_artefacts' / configuration / 'Standalone'
    products.append(standalone / ('DarkVoice Studio.app' if target_platform == 'macos'
                                 else 'DarkVoice Studio'))
    for product in products:
        require(product.exists() and not product.is_symlink(), 'Missing runtime product: ' + product.name)
        no_symlink_path(product)
        file_product = target_platform == 'linux' and product.name == 'DarkVoice Studio'
        require(product.is_file() if file_product else product.is_dir(),
                'Invalid runtime product type')
        if file_product:
            require(product.stat().st_size > 0, 'Standalone runtime product is empty')
        entries = [product] if product.is_file() else [product, *product.rglob('*')]
        regular_files = 0
        for entry in entries:
            if entry.is_symlink():
                link = os.readlink(entry)
                require(not os.path.isabs(link), 'Runtime product contains an absolute symbolic link')
                require(inside(entry.resolve(strict=True), product),
                        'Runtime product contains an external symbolic link')
            else:
                require(entry.is_file() or entry.is_dir(), 'Unsupported runtime product entry')
                regular_files += entry.is_file()
        require(regular_files > 0, 'Runtime product is empty: ' + product.name)
    if target_platform == 'macos':
        with (standalone / 'DarkVoice Studio.app/Contents/Info.plist').open('rb') as stream:
            information = plistlib.load(stream)
        usage = information.get('NSMicrophoneUsageDescription')
        require(isinstance(usage, str) and bool(usage.strip()),
                'Studio application is missing its microphone usage declaration')
    return products


def cache_values(build):
    result = {}
    for line in (build / 'CMakeCache.txt').read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            result[key.split(':', 1)[0]] = value
    return result


def package(source, build, juce, output, target_platform, configuration='Release', expected_commit=None):
    require(target_platform in {'linux', 'macos'} and
            configuration in {'Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel'},
            'Unsupported platform or configuration')
    source, build, juce, output = map(lambda item: Path(item).absolute(), (source, build, juce, output))
    for path in (source, build, juce, output.parent):
        no_symlink_path(path)
        require(path.is_dir(), 'Required directory does not exist')
    no_symlink_path(output)
    source, build, juce = source.resolve(), build.resolve(), juce.resolve()
    output = output.resolve(strict=False)
    require(not output.exists() and not output.is_symlink(), 'Output already exists')
    require(not inside(output, juce) and
            (not inside(output, source) or inside(output, build)),
            'Output must be outside source trees or inside the ignored build directory')
    if inside(build, source):
        require(bool(git(source, 'check-ignore', '--no-index', str(build))),
                'Build directory must be ignored by the project')
    clean_repository(source)
    clean_repository(juce, dependency=True)
    commit = git(source, 'rev-parse', 'HEAD')
    require(expected_commit is None or expected_commit == commit, 'Selected source commit changed')
    require(git(juce, 'rev-parse', 'HEAD') == JUCE_COMMIT, 'JUCE revision is not the immutable project pin')
    require((juce / 'LICENSE.md').is_file() and (juce / 'CMakeLists.txt').is_file() and
            (juce / 'modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt').is_file(),
            'Pinned dependency license/source tree is incomplete')
    cache = cache_values(build)
    require(Path(cache.get('CMAKE_HOME_DIRECTORY', '')).resolve() == source and
            Path(cache.get('DARKVOICE_JUCE_SOURCE_DIR', '')).resolve() == juce and
            cache.get('DARKVOICE_BUILD_PRODUCTS') == 'ON' and
            cache.get('CMAKE_BUILD_TYPE') == configuration and
            cache.get('DARKVOICE_BUILD_SCULPTOR_CORE', 'OFF') == 'OFF',
            'Build configuration does not match selected source/products/dependency')
    require((build / 'DARKVOICE_BUILD_COMMIT.txt').read_text().strip() == commit,
            'Missing or stale post-build/test commit provenance')
    products = product_paths(build, configuration, target_platform)
    require(not any(inside(output, product) for product in products),
            'Output may not be placed inside a runtime product')
    for required in ['LICENSE', 'docs/DEPENDENCIES.md', 'docs/DEVELOPER_BUILDS.md', 'CMakeLists.txt']:
        git(source, 'cat-file', '-e', 'HEAD:' + required)
    with tempfile.TemporaryDirectory(prefix='.darkvoice-package-', dir=output.parent) as directory:
        temporary = Path(directory)
        with tarfile.open(temporary / 'darkvoice-source.tar.gz', 'w:gz') as archive:
            append_git_archive(archive, source, 'darkvoice-source', temporary)
            append_git_archive(archive, juce, 'darkvoice-source/dependencies/JUCE', temporary)
        with tarfile.open(temporary / 'darkvoice-unsigned-products.tar.gz', 'w:gz', dereference=False) as archive:
            for product in products:
                archive.add(product, arcname='products/' + product.name)
        # Recheck the finished runtime archive rather than relying only on the
        # earlier filesystem scan; no unsafe links may reach publication.
        with tarfile.open(temporary / 'darkvoice-unsigned-products.tar.gz') as archive:
            for member in archive:
                safe_member(member, 'products')
                if member.issym():
                    bundle = '/'.join(PurePosixPath(member.name).parts[:2])
                    safe_member(member, bundle)
        metadata = {
            'sourceCommit': commit, 'juceCommit': JUCE_COMMIT, 'juceVersion': '8.0.6',
            'platform': target_platform, 'builderArchitecture': platform.machine(),
            'configuration': configuration, 'unsignedDevelopmentBuild': True,
            'hostAndListeningQualified': False,
            'products': [product.name for product in products],
            'correspondingSource': 'darkvoice-source.tar.gz',
            'dependencyLicenseTree': 'darkvoice-source/dependencies/JUCE',
        }
        (temporary / 'BUILD_MANIFEST.json').write_text(json.dumps(metadata, indent=2) + '\n')
        (temporary / 'COMMIT.txt').write_text(commit + '\n')
        (temporary / 'BUILD_STATUS.txt').write_text(STATUS)
        (temporary / 'LICENSE').write_text(git(source, 'show', 'HEAD:LICENSE') + '\n')
        (temporary / 'DEPENDENCIES.md').write_text(git(source, 'show', 'HEAD:docs/DEPENDENCIES.md') + '\n')
        (temporary / 'DEVELOPER_BUILDS.md').write_text(git(source, 'show', 'HEAD:docs/DEVELOPER_BUILDS.md') + '\n')
        for name in ('project.tar', 'juce.tar'):
            (temporary / name).unlink()
        files = sorted(temporary.iterdir())
        checksums = []
        for file in files:
            with file.open('rb') as stream:
                digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            checksums.append(digest + '  ' + file.name + '\n')
        (temporary / 'SHA256SUMS.txt').write_text(''.join(checksums))
        files.append(temporary / 'SHA256SUMS.txt')
        published = []
        created = False
        try:
            output.mkdir(mode=0o700)  # Exclusive: never replace an existing user directory.
            created = True
            for file in files:
                target = output / file.name
                os.link(file, target)  # Sibling staging directory; exclusive file publication.
                published.append(target)
        except BaseException:
            for file in published:
                file.unlink()
            if created:
                try:
                    output.rmdir()
                except OSError:
                    pass
            raise
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path.cwd())
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--juce-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--platform', choices=['linux', 'macos'], required=True)
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--expected-commit')
    args = parser.parse_args()
    try:
        metadata = package(args.source, args.build, args.juce_source, args.output,
                           args.platform, args.configuration, args.expected_commit)
        print('Prepared unsigned developer artifacts for ' + metadata['sourceCommit'])
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, tarfile.TarError) as error:
        print('Packaging failed: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
