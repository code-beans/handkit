# Contributing

Thanks for helping. The most useful contribution right now is a test on a device Handkit hasn't run on yet.

## Report a device result

1. Build the demo and the input self-test:

   ```sh
   ./tools/handkit build
   ./tools/handkit build --target sdk-input-check
   ```

2. Copy `build/Release/dist/SDL3 Demo.pak` and `build/Release/sdk-input-check` to the device, and run both from a shell (ADB or SSH) while the firmware is at its main menu:

   ```sh
   ./sdk-input-check
   cd "SDL3 Demo.pak" && ./app --smoke
   ```

3. Open a [device report](https://github.com/code-beans/handkit/issues/new?template=device-report.yml) with:
   - the device, firmware and firmware version,
   - both programs' full output (the demo prints the GPU driver, renderer and controller it found),
   - whether the buttons match their printed labels in the demo (X and Y switch modes; A and B pause and reset the 3D cube),
   - anything that looked wrong, such as a sideways picture, wrong colors or missing modes.

A failing run is as useful as a passing one. [docs/platforms.md](docs/platforms.md) lists the open questions for each firmware.

## Add a controller

If a built-in controller isn't mapped (buttons are mixed up, or the demo log says `Gamepad=unmapped`):

1. Find its name and button numbers. SDL's `testcontroller`, or the button log in the demo's 2D mode, show the raw numbers.
2. Add an entry to `named_mappings` in [`sdk/src/runtime.c`](sdk/src/runtime.c) in SDL's mapping format, mapping by **position**: SDL's `a` is the bottom button, `b` the right one.
3. Note where the numbers came from in a comment, and add the device to [docs/platforms.md](docs/platforms.md).

If the firmware already sets `SDL_GAMECONTROLLERCONFIG`, no change is needed: that mapping always wins.

## Make changes

- After changing `recipes/`, `sdk/` or `src/platform/`, run `./tools/handkit configure --update-lock` before building. The lockfile pins recipe revisions; [docs/architecture.md](docs/architecture.md) explains why.
- Run the input self-test and `./tools/handkit run --smoke` on a device when you touch input or rendering, and say which device you used in the pull request.
- Keep public API changes in step with the version in `recipes/handkit/conanfile.py` and `HK_VERSION`.

## Code style

One statement per line, and braces for every control-flow block. Leave blank lines between functions and logical sections. C, GLSL, Python and CMake use a 260-column limit; short calls don't need wrapping.

Formatting is defined by `.clang-format` (C), `pyproject.toml` (Black, Python) and `.cmake-format.yaml` (CMake). Format the code you maintain, not generated files or Conan's cache.

## Releases

Pushing a tag `v<version>` builds the demo, the starter and the input self-test on GitHub Actions, and publishes a GitHub release with zipped `.pak` folders and SHA-256 checksums. The tag must match the version in `recipes/handkit/conanfile.py`; a suffix such as `v0.4.0-rc1` marks a prerelease.

```sh
git tag v0.4.0
git push origin v0.4.0
```

To rebuild an existing tag's files, run the **Release** workflow manually with that tag.

## License

By contributing, you agree that your contributions are released under the [MIT License](LICENSE).
