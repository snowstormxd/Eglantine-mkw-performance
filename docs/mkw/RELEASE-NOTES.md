MKW timing and memory fixes

- Fixed the item-icon flickering seen in the local test.
- Reduced memory use and CPU work when processing GPU commands.
- Reduced Windows timer CPU use while keeping precise short waits.
- Kept Linux and other platforms on their existing timer path.

Extract the whole ZIP and run eglantine.exe. Choose Yes for recommended settings.

Windows build and 97 tests passed. The new timer still needs a race comparison; locked 60 FPS is not confirmed.

Assisted by Codex.
