# ThirdParty Libraries Placeholder

This directory should contain the prebuilt static libraries for the trajectory_spatialhash library.

## Required Structure

```
ThirdParty/trajectory_spatialhash/
├── include/
│   └── trajectory_spatialhash/
│       ├── config.h
│       ├── spatial_hash.h
│       └── ue_wrapper.h
└── lib/
    ├── win64/
    │   └── trajectory_spatialhash.lib
    ├── mac/
    │   └── libtrajectory_spatialhash.a
    └── linux/
        └── libtrajectory_spatialhash.a
```

## How to Obtain Libraries

### Option 1: Download from CI Artifacts (Recommended)

1. Go to the [GitHub Actions page](https://github.com/kahlertfr/trajectory-spatialhash/actions)
2. Select a successful workflow run
3. Download the `trajectory_spatialhash-thirdparty-all-platforms` artifact
4. Extract and copy the contents to this directory

### Option 2: Build Locally

Build for your platform using the provided scripts:

```bash
# From the repository root
./scripts/build-release-libs.sh
./scripts/package-thirdparty.sh
```

This will create the ThirdParty layout in the repository root, which you can then copy here.

### Option 3: Cross-Platform Build

For cross-platform development, build on each platform separately or use the CI workflow.

## Verification

After copying the files, verify the structure matches the layout above. The plugin's Build.cs expects files to be in the exact locations specified.
