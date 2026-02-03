# Building Koriki from Source

This document provides detailed instructions for building Koriki components from source code.

## Prerequisites

### Required Software

- **Docker** (recommended) or native ARM cross-compilation toolchain
- **Git** with submodule support
- **Make** and **GCC** (for host utilities)
- **Python 2.7** (for build scripts)

### Toolchain

Koriki uses the **union-miyoomini-toolchain** by shauninman for cross-compilation.

```bash
# Clone the toolchain repository
git clone https://github.com/shauninman/union-miyoomini-toolchain
cd union-miyoomini-toolchain

# Build the Docker image
docker build -t miyoomini-toolchain .
```

### Required Libraries

The following libraries must be built and installed in the toolchain before building SimpleMenu:

1. **libini** - INI file parser
   ```bash
   git clone https://github.com/pcercuei/libini
   cd libini
   make
   make install
   ```

2. **libopk** - OPK package format support
   ```bash
   git clone https://github.com/pcercuei/libopk
   cd libopk
   make
   make install
   ```

## Getting Started

### 1. Clone Repository

```bash
git clone https://github.com/Rparadise-Team/Koriki
cd Koriki

# Initialize submodules (for SimpleMenu)
git submodule update --init --recursive
```

### 2. Set Up Build Environment

#### Using Docker (Recommended)

```bash
# Run the toolchain container with current directory mounted
docker run -it -v $(pwd):/work miyoomini-toolchain

# Inside the container, you'll have access to:
# - arm-linux-gnueabihf-gcc (cross-compiler)
# - arm-linux-gnueabihf-g++ (C++ cross-compiler)
# - All required libraries
```

#### Native Toolchain (Advanced)

If you have the ARM toolchain installed natively:

```bash
export PATH=/path/to/arm-linux-gnueabihf/bin:$PATH
export CC=arm-linux-gnueabihf-gcc
export CXX=arm-linux-gnueabihf-g++
```

## Building Components

### Core System Components

These are the custom Koriki applications located in the `src/` directory.

#### keymon (Input Daemon)

```bash
cd src/keymon
make clean
make

# Output: keymon binary
# Deploy to: base/Koriki/bin/keymon
```

**Build Options:**
- `PLATFORM_PC=1` - Build for PC testing (default: Miyoo Mini)
- `DEBUG=1` - Enable debug symbols

#### charging (Charging Screen)

```bash
cd src/charging
make clean
make

# Output: charging binary
# Deploy to: base/Koriki/bin/charging
```

**Dependencies:**
- SDL 1.2
- SDL_image
- pthread
- MI libraries (libmi_sys, libmi_gfx)

#### bootScreenSelector (Boot Screen Selector)

```bash
cd src/bootScreenSelector
make clean
make

# Output: bootScreenSelector binary
# Deploy to: base/App/Bootscreen_Selector/bootScreenSelector
```

**Dependencies:**
- SDL 1.2
- SDL_image

#### systemInfo (System Information)

```bash
cd src/systemInfo
make clean
make

# Output: systemInfo binary
# Deploy to: base/App/System_Info/systemInfo
```

**Dependencies:**
- SDL 1.2
- SDL_image
- SDL_ttf

#### showScreen (Display Utility)

```bash
cd src/showScreen
make clean
make

# Output: show binary
# Deploy to: base/Koriki/bin/show
```

### SimpleMenu Frontend

SimpleMenu is the main launcher frontend for Koriki.

```bash
cd src/simplemenu-beta

# Build with Miyoo Mini configuration
make PLATFORM=MMIYOO MM_NOQUIT=1 NOLOADING=1

# Output: simplemenu binary
# Deploy to: base/.simplemenu/simplemenu
```

**Build Flags:**
- `PLATFORM=MMIYOO` - Target Miyoo Mini platform
- `MM_NOQUIT=1` - Disable quit option (prevents exiting to stock menu)
- `NOLOADING=1` - Skip loading screen for faster startup

**Note:** SimpleMenu is a Git submodule. Make sure you've initialized submodules first.

### Other Components

#### volume (Volume Control)

```bash
cd src/volume
make clean
make
```

#### cpuclock (CPU Frequency Control)

```bash
cd src/cpuclock_src
make clean
make
```

## Building Complete Distribution

### Step 1: Build All Components

Use the Docker toolchain to build all components:

```bash
docker run -it -v $(pwd):/work miyoomini-toolchain

# Inside container:
cd /work

# Build each component
for component in keymon charging bootScreenSelector systemInfo showScreen volume cpuclock_src; do
    cd src/$component
    make clean && make
    cd /work
done

# Build SimpleMenu
cd src/simplemenu-beta
make clean
make PLATFORM=MMIYOO MM_NOQUIT=1 NOLOADING=1
cd /work
```

### Step 2: Deploy Binaries

Copy built binaries to the `base/` directory:

```bash
# Copy system binaries
cp src/keymon/keymon base/Koriki/bin/
cp src/charging/charging base/Koriki/bin/
cp src/showScreen/show base/Koriki/bin/
cp src/cpuclock_src/cpuclock base/Koriki/bin/

# Copy applications
cp src/bootScreenSelector/bootScreenSelector base/App/Bootscreen_Selector/
cp src/systemInfo/systemInfo base/App/System_Info/

# Copy SimpleMenu
cp src/simplemenu-beta/simplemenu base/.simplemenu/
```

### Step 3: Create SD Card Image

```bash
# Generate complete SD card image
./makeimg.sh

# Output: Koriki_v{VERSION}.img.gz
# This can be written to an SD card using dd or balenaEtcher
```

**Image Creation Process:**
1. Calculates required size based on `base/` contents
2. Creates FAT32 image with 32KB clusters
3. Copies all files from `base/` directory
4. Compresses with gzip

**Requirements:**
- `parted` for partition management
- `mkfs.vfat` for FAT32 formatting
- `rsync` for file copying
- ~2+ GB free disk space

### Step 4: Create Update Package (Optional)

To create a differential update package:

```bash
# Syntax: ./update_make.sh <old_version> <new_version> <output_dir> <version>
./update_make.sh ./old_release ./base ./update_package 1.7

# Output: update_koriki_v1.7.zip
# Place on SD card root to auto-update on next boot
```

## Testing Builds

### On PC (Limited Testing)

Some components support PC builds for basic testing:

```bash
cd src/bootScreenSelector
make PLATFORM_PC=1

# Run with local paths
./bootScreenSelector
```

**Limitations:**
- No hardware access (GPIO, buttons, etc.)
- Different screen resolution
- Missing Miyoo-specific libraries

### On Device (Full Testing)

1. **Copy binary to SD card:**
   ```bash
   # Mount SD card
   sudo mount /dev/sdX1 /mnt
   
   # Copy binary
   sudo cp src/keymon/keymon /mnt/Koriki/bin/
   
   # Unmount
   sudo umount /mnt
   ```

2. **Boot device with modified SD card**

3. **Access via serial console (if available):**
   ```bash
   screen /dev/ttyUSB0 115200
   ```

4. **Check logs:**
   ```bash
   # On device
   dmesg | tail
   ps aux | grep keymon
   ```

## Common Build Issues

### Issue: "arm-linux-gnueabihf-gcc: command not found"

**Solution:** Ensure you're using the Docker toolchain or have the ARM toolchain in your PATH.

```bash
# Verify toolchain
docker run miyoomini-toolchain arm-linux-gnueabihf-gcc --version
```

### Issue: "SDL/SDL.h: No such file or directory"

**Solution:** SDL libraries are included in the toolchain. Verify you're cross-compiling correctly.

```bash
# Check SDL includes
arm-linux-gnueabihf-gcc -E - -v < /dev/null 2>&1 | grep SDL
```

### Issue: "undefined reference to 'IMG_Load'"

**Solution:** Link SDL_image library explicitly in Makefile.

```makefile
LIBS = -lSDL -lSDL_image
```

### Issue: Binary crashes on device with "Illegal instruction"

**Solution:** Verify ARM compilation flags are correct:

```makefile
CFLAGS = -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
```

### Issue: "libmi_sys.so: cannot open shared object file"

**Solution:** MI libraries are Miyoo-specific and only available on device. Ensure you're cross-compiling, not building for x86.

## Cleaning Build Artifacts

```bash
# Clean all component builds
for dir in src/*/; do
    (cd "$dir" && [ -f Makefile ] && make clean)
done

# Remove generated images
rm -f *.img *.img.gz

# Remove update packages
rm -f update_koriki_*.zip
```

## Build Configuration

### Compiler Flags

Default flags for Miyoo Mini (Allwinner F1C500s):

```makefile
CC = arm-linux-gnueabihf-gcc
CFLAGS = -mtune=cortex-a7        # ARM Cortex-A7 optimizations
CFLAGS += -mfpu=neon-vfpv4       # NEON SIMD support
CFLAGS += -mfloat-abi=hard       # Hardware floating point
CFLAGS += -O2                     # Optimization level 2
CFLAGS += -fdata-sections         # Dead code elimination
CFLAGS += -ffunction-sections     # Dead code elimination
LDFLAGS = -Wl,--gc-sections       # Garbage collect unused sections
```

### Device Variants

Koriki automatically detects device variants at runtime, so the same binaries work across:
- Miyoo Mini v1-v3 (640×480)
- Miyoo Mini v4 (752×560)
- Miyoo Mini Plus (640×480, WiFi)
- Miyoo Mini Flip (752×560, WiFi)

No separate builds are needed for different variants.

## Contributing Builds

When submitting binaries:

1. **Always build with the official toolchain**
2. **Test on actual hardware**
3. **Include build instructions if adding new components**
4. **Document any new dependencies**
5. **Run `make clean` before committing source**

## Additional Resources

- **Toolchain Repository:** https://github.com/shauninman/union-miyoomini-toolchain
- **SimpleMenu Documentation:** https://github.com/fgl82/simplemenu
- **Koriki Wiki:** https://github.com/Rparadise-Team/Koriki/wiki
- **Developer Guide:** [DEVELOPER_QUICK_START.md](DEVELOPER_QUICK_START.md)
- **Code Review:** [CODE_REVIEW_ANALYSIS.md](CODE_REVIEW_ANALYSIS.md)

## License

See individual component licenses. Most components are GPL or MIT licensed.

---

**Last Updated:** February 3, 2026  
**Maintainer:** Rparadise-Team
