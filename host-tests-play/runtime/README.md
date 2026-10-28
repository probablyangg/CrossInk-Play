# Play runtime tests

Run `host-tests-play/runtime/run.sh` from the repository root. Requires a C++17
compiler, Python 3 and Git; no device or PlatformIO is needed.

The tests exercise the actual PaintClock, reveal gates, and pinned SDK interaction
buffer. They cover changed actions waiting for a completed paint, unchanged touch
feedback retaining release input, double-buffered publication, and idle waits
not releasing an unpainted screen. A host build of the patched SDK verifies fresh
free-space counts, error versus full-card behavior, and calculations above 4 GB.
The SDK patch tests verify a clean application, an idempotent second run, and
rejection of drift or a partial patch before any further changes are written.

The overlay regression case also verifies that restoring a game waits for that
game's own next build and completed paint, even if unrelated paints occur first.
