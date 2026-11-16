#!/usr/bin/env python3
"""Initialize standard window-manager atoms for JUCE on a bare Xvfb display.

No window manager is simulated. Keep a display connection open while the actual
GUI executable runs and return its exit status. A normal desktop supplies these
atoms itself; JUCE 8.0.6 assumes some exist when creating top-level windows.
"""
import ctypes
import ctypes.util
import subprocess
import sys


def main():
    if len(sys.argv) < 2:
        print('Usage: run-x11-smoke.py COMMAND [ARG ...]', file=sys.stderr)
        return 2
    library = ctypes.util.find_library('X11')
    if not library:
        raise RuntimeError('X11 library unavailable')
    x11 = ctypes.CDLL(library)
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError('Cannot open DISPLAY')
    try:
        atoms = (
            'WM_PROTOCOLS WM_TAKE_FOCUS WM_DELETE_WINDOW WM_CHANGE_STATE WM_STATE '
            '_NET_WM_PING _NET_WM_WINDOW_TYPE _NET_WM_WINDOW_TYPE_NORMAL '
            '_NET_WM_WINDOW_TYPE_COMBO _NET_WM_STATE _NET_WM_STATE_HIDDEN '
            '_NET_WM_STATE_FULLSCREEN _NET_WM_ALLOWED_ACTIONS _NET_WM_MOVERESIZE '
            '_NET_FRAME_EXTENTS _NET_WORKAREA _MOTIF_WM_HINTS '
            '_WIN_HINTS KWM_WIN_DECORATION _KDE_NET_WM_WINDOW_TYPE_OVERRIDE'
        )
        for atom in atoms.split():
            if not x11.XInternAtom(display, atom.encode('ascii'), 0):
                raise RuntimeError(f'Cannot initialize X11 atom {atom}')
        x11.XSync(display, 0)
        result = subprocess.call(sys.argv[1:])
        return result if result >= 0 else 128 - result
    finally:
        x11.XCloseDisplay(display)


if __name__ == '__main__':
    sys.exit(main())
