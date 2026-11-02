# Local MKW race test, 2 November 2026

Run eglantine.exe and choose Yes for the recommended graphics settings.
This is a test build; locked 60 FPS is not yet confirmed.

The user confirmed that the first test removes item-icon flickering. With detailed
profiling off, the user reports roughly 55 FPS. Thirteen low-shader samples from
200-330 seconds average 55.4 FPS (43.0-61.0); these remain periodic samples rather
than frame percentiles. The log still shows repeated texture eviction and uploads
while the system commit limit restricts the cache below the available VRAM budget.

The second test removes an unused 2 MiB rewind stack from every fiber allocation.
It keeps the active stack size and fiber-switching logic unchanged. Optional
profiling now separates deferred commands, worker waits, GPU waits and readbacks.
Windows thread names also work in the UCRT64 build. Profiling stays off for the
normal FPS comparison. The second test's gameplay improvement is still unmeasured.

Changes:

- Yield the guest core during presentation waits and honour the game's swap interval.
- Submit BC3 texture decodes before encoding, using upstream's flashing workaround.
- Avoid filling unused fiber stacks and descriptor storage to reduce memory pressure.
- Evict older textures before repeatedly replacing recent ones. Keep GPU-written
  images out of this extra eviction pass and guard invalid buffer addresses.

The original race log includes ten low-shader-count samples between 200 and 340
seconds, averaging 50.6 FPS (41.01-59.99). These are periodic samples, not frame
percentiles. The RTX 4070 log reports limited system commit headroom. A later run
overlapped compilation and is not a clean performance comparison.

The current fork already includes upstream's 31.25 MHz guest counter and uses
wall time in multicore mode. The new presentation changes free the guest core
while waiting; they do not guarantee that a game with frame-dependent simulation
will run at full speed below its intended render rate.

Native Windows PGO build and 92 regression cases pass, including present timing,
slow-frame recovery, and fiber switching. The second test still needs comparison.
Use the same track, game version, camera, and 1x graphics settings for both runs.
Check the item icon and race timer as well as FPS. A profiling run includes extra
instrumentation; follow it with a normal run for the FPS comparison.

Adapted from Duck912's upstream f8d3860/e7973da presentation changes and whovian64's
a407703 texture and memory changes, plus the unmapped-buffer guard from ymatrix1's
46b2577. Other fork changes remain in place.

Assisted by Codex.
