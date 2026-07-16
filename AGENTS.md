# Agent instructions

## Keep the application launcher current

After making any change in this repository, do not report the task complete until the application launcher points to the newly verified build.

Run these finish steps in order:

1. Build the desktop application:

   ```bash
   cmake --build build -j2 --target say-count
   ```

2. Run tests appropriate to the change. For code or build-system changes, run the complete suite:

   ```bash
   ctest --test-dir build --output-on-failure
   ```

3. Only after the build and required tests pass, update the executable used by the application launcher:

   ```bash
   install -m 755 build/say-count /home/artemis/.local/bin/say-count
   ```

4. Confirm the installed executable is byte-identical to the verified build:

   ```bash
   sha256sum build/say-count /home/artemis/.local/bin/say-count
   ```

The two hashes must match. Never replace the installed executable when the build or required tests fail. Mention the launcher-binary update and verification in the final handoff.
