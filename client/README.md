# client

This directory contains the Nintendo 3DS homebrew app.

## Current status

The first bootstrap scaffold is in place:
- devkitPro-style project layout
- build Makefile
- simple console app loop
- app metadata
- icon placeholder

The first target is a lightweight `.3dsx` build that runs cleanly on an original Old 3DS.

## Build

Install the normal 3DS devkitPro toolchain, then run:

```bash
make
```

That should produce the first `.3dsx` bootstrap build.
