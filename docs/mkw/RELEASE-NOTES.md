MKW performance and flickering fixes

- Fixed the item-icon flickering seen in the local test.
- Improved presentation timing and reduced texture-cache hitches.
- Reduced CPU work while processing GPU commands.
- Kept the recommended-settings choice and Switch 2 rooms.

Extract the whole ZIP and run eglantine.exe. Choose Yes for recommended settings.

Windows build and 93 tests passed. Earlier local tests reached about 55 FPS with better lows and fewer hitches. The final CPU changes still need a race comparison; locked 60 FPS is not confirmed.

Assisted by Codex.
