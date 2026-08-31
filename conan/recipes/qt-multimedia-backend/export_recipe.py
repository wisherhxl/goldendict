#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
from pathlib import Path


QT_REFERENCE = "qt/6.11.1#b4b7bf63674c82b58b825ffca311f656"
FFMPEG_REFERENCE = "ffmpeg/7.1.5"


def replace_once(text: str, old: str, new: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one recipe match, found {count}: {old!r}")
    return text.replace(old, new, 1)


def main() -> None:
    subprocess.run(
        [
            "conan",
            "download",
            QT_REFERENCE,
            "--only-recipe",
            "--remote",
            "conancenter",
        ],
        check=True,
    )
    recipe_folder = Path(
        subprocess.check_output(
            ["conan", "cache", "path", QT_REFERENCE], text=True
        ).strip()
    )

    with tempfile.TemporaryDirectory(prefix="goldendict-qt-multimedia-") as temp:
        export_folder = Path(temp)
        for filename in ("conanfile.py", "conandata.yml", "qtmodules6.11.1.conf"):
            shutil.copy2(recipe_folder / filename, export_folder / filename)

        recipe_path = export_folder / "conanfile.py"
        recipe = recipe_path.read_text(encoding="utf-8")
        recipe = replace_once(
            recipe,
            '        "with_gstreamer": [True, False],\n'
            '        "with_pulseaudio": [True, False],',
            '        "with_gstreamer": [True, False],\n'
            '        "with_ffmpeg": [True, False],\n'
            '        "with_pulseaudio": [True, False],',
        )
        recipe = replace_once(
            recipe,
            '        "with_gstreamer": False,\n'
            '        "with_pulseaudio": False,',
            '        "with_gstreamer": False,\n'
            '        "with_ffmpeg": False,\n'
            '        "with_pulseaudio": False,',
        )
        recipe = replace_once(
            recipe,
            '            del self.options.with_gstreamer\n'
            '            del self.options.with_pulseaudio',
            '            del self.options.with_gstreamer\n'
            '            del self.options.with_ffmpeg\n'
            '            del self.options.with_pulseaudio',
        )
        recipe = replace_once(
            recipe,
            '        if self.options.get_safe("with_pulseaudio", False) or self.options.get_safe("with_libalsa", False):\n'
            '            raise ConanInvalidConfiguration("alsa and pulseaudio are not supported (QTBUG-95116), please disable them.")',
            '        if self.options.get_safe("with_libalsa", False):\n'
            '            raise ConanInvalidConfiguration("The Qt Conan recipe does not support its ALSA option; use PulseAudio for Linux multimedia output.")',
        )
        recipe = replace_once(
            recipe,
            '        if self.options.get_safe("with_gstreamer", False):\n'
            '            self.requires("gstreamer/1.19.2")\n'
            '            self.requires("gst-plugins-base/1.19.2")\n'
            '        if self.options.get_safe("with_pulseaudio", False):',
            '        if self.options.get_safe("with_gstreamer", False):\n'
            '            self.requires("gstreamer/1.19.2")\n'
            '            self.requires("gst-plugins-base/1.19.2")\n'
            '        if self.options.get_safe("with_ffmpeg", False):\n'
            f'            self.requires("{FFMPEG_REFERENCE}")\n'
            '        if self.options.get_safe("with_pulseaudio", False):',
        )
        recipe = replace_once(
            recipe,
            '            if self.options.get_safe("with_pulseaudio", False):\n'
            '                multimedia_reqs.append("pulseaudio::pulse")',
            '            if self.options.get_safe("with_ffmpeg", False):\n'
            '                # avformat carries Qt Multimedia\'s remaining FFmpeg\n'
            '                # component dependencies transitively. Recording it\n'
            '                # also satisfies Conan\'s direct-dependency check.\n'
            '                multimedia_reqs.append("ffmpeg::avformat")\n'
            '            if self.options.get_safe("with_pulseaudio", False):\n'
            '                multimedia_reqs.append("pulseaudio::pulse")',
        )
        recipe = replace_once(
            recipe,
            '    def generate(self):\n'
            '        ms = VirtualBuildEnv(self)',
            '    def generate(self):\n'
            '        # Qt\'s wrapper uses CMake\'s legacy PulseAudio variables, while\n'
            '        # Conan exposes the dependency as the pulseaudio::pulse target.\n'
            '        replace_in_file(\n'
            '            self,\n'
            '            os.path.join(\n'
            '                self.source_folder,\n'
            '                "qtmultimedia",\n'
            '                "cmake",\n'
            '                "FindWrapPulseAudio.cmake",\n'
            '            ),\n'
            '            "find_package(PulseAudio QUIET)\\n",\n'
            '            "find_package(PulseAudio QUIET)\\n"\n'
            '            "if(TARGET pulseaudio::pulse)\\n"\n'
            '            "    get_target_property(PULSEAUDIO_INCLUDE_DIR pulseaudio::pulse INTERFACE_INCLUDE_DIRECTORIES)\\n"\n'
            '            "    set(PULSEAUDIO_LIBRARY pulseaudio::pulse)\\n"\n'
            '            "    set(PulseAudio_FOUND ON)\\n"\n'
            '            "endif()\\n",\n'
            '        )\n'
            '        ms = VirtualBuildEnv(self)',
        )
        recipe = replace_once(
            recipe,
            '        tc.set_property("gstreamer", "cmake_find_mode", "module")\n\n'
            '        tc.generate()',
            '        tc.set_property("gstreamer", "cmake_find_mode", "module")\n'
            '        # Preserve Qt Multimedia\'s FindFFmpeg module and feed it Conan\'s\n'
            '        # generated pkg-config metadata.\n'
            '        tc.set_property("ffmpeg", "cmake_find_mode", "none")\n\n'
            '        # Qt\'s FindWrapPulseAudio calls find_package(PulseAudio) with\n'
            '        # case-sensitive config and variable names.\n'
            '        tc.set_property("pulseaudio", "cmake_file_name", "PulseAudio")\n\n'
            '        tc.set_property("pulseaudio", "cmake_find_mode", "module")\n\n'
            '        tc.generate()',
        )
        recipe = replace_once(
            recipe,
            '                              ("with_egl", "egl"),\n'
            '                              ("with_gstreamer", "gstreamer")]:',
            '                              ("with_egl", "egl"),\n'
            '                              ("with_gstreamer", "gstreamer"),\n'
            '                              ("with_ffmpeg", "ffmpeg"),\n'
            '                              ("with_pulseaudio", "pulseaudio")]:',
        )
        recipe_path.write_text(recipe, encoding="utf-8")

        subprocess.run(
            ["conan", "export", str(export_folder), "--version", "6.11.1"],
            check=True,
        )


if __name__ == "__main__":
    main()
