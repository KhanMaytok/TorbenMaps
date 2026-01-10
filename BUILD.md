# Building planet.exe for Windows

This repository includes a Dockerfile to cross-compile `planet.c` into a Windows executable that can run on Windows 10 or Windows 11.

## Prerequisites

- Docker installed on your system (Linux, macOS, or Windows with WSL2)

## Building the Windows Executable

### Step 1: Build the Docker Image

```bash
docker build -t planet-windows .
```

This will:
- Use Debian Bullseye as the base image
- Install MinGW cross-compiler for Windows
- Compile `planet.c` into a 64-bit Windows executable (`planet.exe`)

### Step 2: Extract the Compiled Executable

After building, extract the compiled `planet.exe` from the Docker container:

```bash
# Create a temporary container
docker create --name temp-planet planet-windows

# Copy the exe file out
docker cp temp-planet:/build/planet.exe planet.exe

# Remove the temporary container
docker rm temp-planet
```

### Alternative: One-Line Extract

```bash
docker run --rm -v $(pwd):/output planet-windows sh -c "cp /build/planet.exe /output/planet.exe"
```

## What Gets Built

The Dockerfile produces:
- **planet.exe** - A 64-bit Windows console application (PE32+ executable)
- Statically linked with all dependencies
- Compatible with Windows 10 and Windows 11
- Compiled with optimization level 2 (`-O2`)

## Technical Details

The build process:
1. Uses `x86_64-w64-mingw32-gcc` (MinGW-w64) for cross-compilation
2. Compiles with flags: `-O2 -Wall -D_USE_LIBM_MATH_H`
3. Statically links the math library (`-lm -static`)
4. Produces a standalone executable with no external dependencies

## Troubleshooting

If you encounter issues:
- Make sure Docker is running
- Check that you have sufficient disk space
- Verify that `planet.c` is present in the repository root

## Using the Compiled Executable

Once extracted, transfer `planet.exe` to a Windows 10 or Windows 11 machine and run it from Command Prompt or PowerShell. Refer to `Manual.txt` for usage instructions.
