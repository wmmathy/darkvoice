"""Offline packaging integration fixtures; never claim these files are real products."""
import hashlib
import importlib.util
import json
import os
import plistlib
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SPEC = importlib.util.spec_from_file_location(
    'package_development', Path(__file__).resolve().parents[1] / 'tools/package-development.py')
PACKAGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGER)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name).resolve()
        self.source = self.root / 'source'
        self.juce = self.root / 'juce'
        for path in (self.source, self.juce):
            path.mkdir()
            self.git(path, 'init', '-q')
            self.git(path, 'config', 'user.name', 'Packaging fixture')
            self.git(path, 'config', 'user.email', 'fixture@example.invalid')
        self.write(self.source / '.gitignore', '/build/\n')
        self.write(self.source / 'LICENSE', 'Project license fixture\n')
        self.write(self.source / 'CMakeLists.txt', '# Project build fixture\n')
        self.write(self.source / 'docs/DEPENDENCIES.md', 'Dependency policy fixture\n')
        self.write(self.source / 'docs/DEVELOPER_BUILDS.md', 'Build instructions fixture\n')
        self.write(self.juce / 'LICENSE.md', 'JUCE license fixture\n')
        self.write(self.juce / 'CMakeLists.txt', '# Dependency build fixture\n')
        self.write(self.juce / 'modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt',
                   'SDK license fixture\n')
        self.write(self.juce / 'modules/third_party/notice.h', '// Complete upstream notice fixture\n')
        self.commit(self.source)
        self.commit(self.juce)
        self.pin = self.git(self.juce, 'rev-parse', 'HEAD')
        self.build = self.source / 'build'
        self.build.mkdir()
        self.output = self.build / 'artifacts'
        self.write(self.build / 'CMakeCache.txt',
                   f'CMAKE_HOME_DIRECTORY:INTERNAL={self.source}\n'
                   f'DARKVOICE_JUCE_SOURCE_DIR:PATH={self.juce}\n'
                   'DARKVOICE_BUILD_PRODUCTS:BOOL=ON\nCMAKE_BUILD_TYPE:STRING=Release\n')
        self.stamp()
        self.products('linux')
        self.pin_patch = mock.patch.object(PACKAGER, 'JUCE_COMMIT', self.pin)
        self.pin_patch.start()

    def tearDown(self):
        self.pin_patch.stop()
        self.temporary.cleanup()

    @staticmethod
    def write(path, value):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)

    @staticmethod
    def git(path, *args):
        return subprocess.check_output(['git', '-C', str(path), *args],
                                       text=True, stderr=subprocess.DEVNULL).strip()

    def commit(self, path):
        self.git(path, 'add', '.')
        self.git(path, 'commit', '-qm', 'Fixture source')

    def stamp(self):
        self.write(self.build / 'DARKVOICE_BUILD_COMMIT.txt',
                   self.git(self.source, 'rev-parse', 'HEAD') + '\n')

    def products(self, target_platform):
        for target, name in [('DarkVoiceVocal', 'DarkVoice Vocal'),
                             ('DarkVoiceMaster', 'DarkVoice Master')]:
            root = self.build / (target + '_artefacts/Release')
            self.write(root / 'VST3' / (name + '.vst3/Contents/module.so'), 'VST3 fixture\n')
            self.write(root / ('lib' + name + '_SharedCode.a'), 'Must not be packaged\n')
            if target_platform == 'macos':
                self.write(root / 'AU' / (name + '.component/Contents/module'), 'AU fixture\n')
        standalone = self.build / 'DarkVoiceStudio_artefacts/Release/Standalone'
        if target_platform == 'macos':
            self.write(standalone / 'DarkVoice Studio.app/Contents/MacOS/DarkVoice Studio', 'App fixture\n')
            self.write(standalone / 'DarkVoice Studio.app/Contents/Info.plist',
                       plistlib.dumps({'NSMicrophoneUsageDescription': 'Local monitoring fixture'}).decode())
        else:
            self.write(standalone / 'DarkVoice Studio', 'Standalone fixture\n')

    def package(self, target_platform='linux', **kwargs):
        return PACKAGER.package(self.source, self.build, self.juce, self.output,
                                target_platform, **kwargs)

    def fails(self, **kwargs):
        with self.assertRaises((ValueError, OSError, subprocess.CalledProcessError)):
            self.package(**kwargs)
        self.assertFalse(self.output.exists(), 'Failed validation left partial output')

    def test_linux_archive_integrity_and_complete_source(self):
        metadata = self.package(expected_commit=self.git(self.source, 'rev-parse', 'HEAD'))
        self.assertEqual(metadata['juceCommit'], self.pin)
        self.assertTrue(metadata['unsignedDevelopmentBuild'])
        self.assertFalse(metadata['hostAndListeningQualified'])
        for line in (self.output / 'SHA256SUMS.txt').read_text().splitlines():
            expected, name = line.split('  ', 1)
            self.assertEqual(hashlib.sha256((self.output / name).read_bytes()).hexdigest(), expected)
        with tarfile.open(self.output / 'darkvoice-source.tar.gz') as archive:
            names = archive.getnames()
            self.assertIn('darkvoice-source/LICENSE', names)
            self.assertIn('darkvoice-source/dependencies/JUCE/modules/third_party/notice.h', names)
            self.assertIn('darkvoice-source/dependencies/JUCE/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt', names)
            self.assertFalse(any('.git' in Path(name).parts for name in names))
        with tarfile.open(self.output / 'darkvoice-unsigned-products.tar.gz') as archive:
            names = archive.getnames()
            self.assertIn('products/DarkVoice Studio', names)
            self.assertFalse(any(name.endswith('.a') for name in names))
        self.assertEqual(json.loads((self.output / 'BUILD_MANIFEST.json').read_text()), metadata)

    def test_mac_runtime_layout_and_safe_relative_bundle_link(self):
        self.products('macos')
        link = self.build / 'DarkVoiceVocal_artefacts/Release/VST3/DarkVoice Vocal.vst3/Contents/Current'
        link.symlink_to('module.so')
        self.package('macos')
        with tarfile.open(self.output / 'darkvoice-unsigned-products.tar.gz') as archive:
            names = archive.getnames()
            self.assertIn('products/DarkVoice Vocal.component/Contents/module', names)
            self.assertIn('products/DarkVoice Studio.app/Contents/MacOS/DarkVoice Studio', names)
            self.assertTrue(archive.getmember('products/DarkVoice Vocal.vst3/Contents/Current').issym())

    def test_mac_microphone_declaration_required(self):
        self.products('macos')
        information = self.build / 'DarkVoiceStudio_artefacts/Release/Standalone/DarkVoice Studio.app/Contents/Info.plist'
        information.unlink()
        self.fails(target_platform='macos')
        self.write(information, plistlib.dumps({'NSMicrophoneUsageDescription': ''}).decode())
        self.fails(target_platform='macos')

    def test_wrong_pin_and_dirty_dependency(self):
        with mock.patch.object(PACKAGER, 'JUCE_COMMIT', '0' * 40):
            self.fails()
        self.write(self.juce / 'LICENSE.md', 'Changed dependency')
        self.fails()

    def test_untracked_and_ignored_dependency_additions(self):
        self.write(self.juce / 'untracked.txt', 'Do not ship')
        self.fails()
        (self.juce / 'untracked.txt').unlink()
        self.write(self.juce / '.gitignore', 'ignored.txt\n')
        self.commit(self.juce)
        self.pin_patch.stop()
        self.pin_patch = mock.patch.object(PACKAGER, 'JUCE_COMMIT', self.git(self.juce, 'rev-parse', 'HEAD'))
        self.pin_patch.start()
        self.write(self.juce / 'ignored.txt', 'Do not ship either')
        self.fails()

    def test_dirty_and_untracked_project(self):
        self.write(self.source / 'LICENSE', 'Changed source')
        self.fails()
        self.git(self.source, 'checkout', '--', 'LICENSE')
        self.write(self.source / 'private-untracked.txt', 'Do not ship')
        self.fails()

    def test_missing_runtime_or_provenance(self):
        (self.build / 'DARKVOICE_BUILD_COMMIT.txt').unlink()
        self.fails()
        self.stamp()
        (self.build / 'DarkVoiceStudio_artefacts/Release/Standalone/DarkVoice Studio').unlink()
        self.fails()

    def test_stale_stamp_selected_commit_and_wrong_build_cache(self):
        self.write(self.build / 'DARKVOICE_BUILD_COMMIT.txt', '0' * 40)
        self.fails()
        self.stamp()
        self.fails(expected_commit='0' * 40)
        self.write(self.build / 'CMakeCache.txt', 'DARKVOICE_BUILD_PRODUCTS:BOOL=OFF\n')
        self.fails()

    def test_external_and_absolute_product_links(self):
        link = self.build / 'DarkVoiceVocal_artefacts/Release/VST3/DarkVoice Vocal.vst3/Contents/leak'
        link.symlink_to(self.source / 'LICENSE')
        self.fails()
        link.unlink()
        link.symlink_to('../../../../../../LICENSE')
        self.fails()

    def test_unsafe_source_link_and_environment_file(self):
        (self.source / 'external').symlink_to('/etc/passwd')
        self.commit(self.source)
        self.stamp()
        self.fails()
        (self.source / 'external').unlink()
        self.write(self.source / '.env', 'NOT_A_REAL_SECRET=fixture\n')
        self.commit(self.source)
        self.stamp()
        self.fails()

    def test_preserve_existing_output_and_source_destination(self):
        self.output.mkdir()
        with self.assertRaises(ValueError):
            self.package()
        self.assertTrue(self.output.is_dir(), 'Existing empty user directory was deleted')
        self.output.rmdir()
        output = self.source / 'user-files'
        with self.assertRaises(ValueError):
            PACKAGER.package(self.source, self.build, self.juce, output, 'linux')
        self.assertFalse(output.exists())

    def test_output_symlink_parent_and_path_normalization(self):
        alias = self.root / 'alias'
        alias.symlink_to(self.build, target_is_directory=True)
        with self.assertRaises(ValueError):
            PACKAGER.package(self.source, self.build, self.juce, alias / 'artifacts', 'linux')
        with self.assertRaises(ValueError):
            PACKAGER.package(self.source, self.build, self.juce,
                             self.build / '..' / 'source-output', 'linux')
        self.assertFalse((self.source / 'source-output').exists())

    def test_failed_publication_removes_only_owned_partial_output(self):
        original = os.link
        count = 0
        def fail_second(source, target):
            nonlocal count
            count += 1
            if count == 2:
                raise OSError('Injected publication failure')
            return original(source, target)
        with mock.patch.object(PACKAGER.os, 'link', side_effect=fail_second):
            self.fails()


if __name__ == '__main__':
    unittest.main()
