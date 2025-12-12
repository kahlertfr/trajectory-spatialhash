# CI Configuration

This directory is reserved for CI-related configuration and scripts.

## GitHub Actions

The main CI workflow is located at `.github/workflows/build_and_test.yml`.

It provides:
- Multi-platform builds (Windows, macOS, Linux)
- Automated testing on all platforms
- Library artifact uploads per platform
- ThirdParty package artifact for Unreal Engine integration

## Workflow Features

- **Build Matrix**: Tests on ubuntu-latest, windows-latest, macos-latest
- **Test Execution**: Runs CTest on all platforms
- **Artifact Upload**: Uploads compiled libraries and CLI tools
- **ThirdParty Packaging**: Creates a ready-to-use package for UE plugins

## Accessing Artifacts

After a successful workflow run:

1. Go to the [Actions tab](https://github.com/kahlertfr/trajectory-spatialhash/actions)
2. Select a completed workflow run
3. Download artifacts:
   - `trajectory_spatialhash-{platform}-Release` - Platform-specific libraries
   - `tsh-cli-{platform}-Release` - CLI executables
   - `trajectory_spatialhash-thirdparty-all-platforms` - Complete ThirdParty layout

## Local CI Testing

To test the build locally before pushing:

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# Build
cmake --build build --config Release

# Test
cd build && ctest --output-on-failure

# Install
cmake --install build --prefix install
```
