# Koriki Developer Quick Start Guide

A quick reference for developers working on Koriki firmware for Miyoo Mini devices.

## Prerequisites

### Required Tools

```bash
# Docker (for toolchain)
sudo apt-get install docker.io

# Build essentials
sudo apt-get install build-essential git rsync gzip parted
```

### Get the Toolchain

```bash
# Clone shauninman's toolchain
git clone https://github.com/shauninman/union-miyoomini-toolchain
cd union-miyoomini-toolchain

# Build Docker image
docker build -t miyoomini-toolchain .
```

## Quick Start

### 1. Clone Repository

```bash
git clone https://github.com/Rparadise-Team/Koriki
cd Koriki
git submodule update --init --recursive
```

### 2. Build Components

#### Using Docker Toolchain

```bash
# Run toolchain container
docker run -it -v $(pwd):/work miyoomini-toolchain

# Inside container:
cd /work/src/keymon
make clean && make

cd /work/src/charging
make clean && make

cd /work/src/bootScreenSelector
make clean && make

cd /work/src/systemInfo
make clean && make
```

#### Build SimpleMenu

```bash
# Install dependencies first (in toolchain)
# libini
git clone https://github.com/pcercuei/libini
cd libini && make && make install

# libopk
git clone https://github.com/pcercuei/libopk
cd libopk && make && make install

# Build SimpleMenu
cd /work/src/simplemenu-beta
make PLATFORM=MMIYOO MM_NOQUIT=1 NOLOADING=1
```

### 3. Deploy Binaries

```bash
# Copy built binaries to base directory
cp src/keymon/keymon base/Koriki/bin/
cp src/charging/charging base/Koriki/bin/
cp src/bootScreenSelector/bootScreenSelector base/App/Bootscreen_Selector/
cp src/systemInfo/systemInfo base/App/System_Info/
cp src/simplemenu-beta/simplemenu base/.simplemenu/
```

### 4. Create SD Card Image

```bash
./makeimg.sh
# Output: Koriki_v{VERSION}.img.gz
```

### 5. Create Update Package

```bash
./update_make.sh ./old_version ./base ./update_output 1.7
# Output: update_koriki_v1.7.zip
```

## Project Structure

```
Koriki/
├── src/                       # Source code
│   ├── keymon/               # Input daemon (C)
│   ├── charging/             # Charging screen (C + SDL)
│   ├── bootScreenSelector/   # Boot screen selector (C + SDL)
│   ├── systemInfo/           # System info viewer (C + SDL)
│   ├── simplemenu-beta/      # Frontend (Git submodule)
│   └── [other components]/
│
├── base/                      # Deployment structure
│   ├── .simplemenu/          # SimpleMenu + launchers
│   ├── Koriki/               # System files
│   │   ├── bin/              # Binaries
│   │   ├── lib/              # Libraries
│   │   └── assets/           # Config files
│   ├── RetroArch/            # Emulation
│   └── App/                  # User applications
│
├── makeimg.sh                 # Image builder
└── update_make.sh             # Update generator
```

## Common Tasks

### Add New Application

1. **Create source directory:**
   ```bash
   mkdir -p src/myapp
   cd src/myapp
   ```

2. **Create Makefile:**
   ```makefile
   CC = arm-linux-gnueabihf-gcc
   CFLAGS = -mtune=cortex-a7 -mfpu=neon-vfpv4 -O2
   LIBS = -lSDL -lSDL_image
   
   myapp: main.c
   	$(CC) $(CFLAGS) -o myapp main.c $(LIBS)
   
   clean:
   	rm -f myapp
   ```

3. **Build and deploy:**
   ```bash
   make
   cp myapp ../../base/App/MyApp/
   ```

### Test on Device

1. **Copy to SD card:**
   ```bash
   # Mount SD card
   sudo mount /dev/sdX1 /mnt
   
   # Copy files
   sudo rsync -av base/ /mnt/
   
   # Unmount
   sudo umount /mnt
   ```

2. **Boot device with SD card**

### Debug on Device

#### Via UART (Serial Console)

```bash
# Connect USB-to-TTL adapter to UART pins
# Use 115200 baud rate
screen /dev/ttyUSB0 115200

# Or with minicom
minicom -D /dev/ttyUSB0 -b 115200
```

#### Via SSH (MMP/Flip only)

```bash
# Enable WiFi on device first
# Connect to same network
ssh root@<device-ip>
# Password: usually "root" or blank
```

### View Logs

```bash
# On device (via UART or SSH)
dmesg                     # Kernel messages
cat /tmp/koriki.log       # Koriki logs (if implemented)
ps aux                    # Running processes
top                       # System resources
```

## Device Variants

### Detection at Runtime

All variants detected automatically via:
```bash
# Screen resolution
dmesg | grep "FB_WIDTH"

# Model (MM vs MMP/Flip)
[ -f /customer/app/axp_test ] && echo "MMP/Flip" || echo "MM"
```

### Variant Overview

| Variant | Screen | Detection | Config File |
|---------|--------|-----------|-------------|
| MM v1-v3 | 640×480 | FB_WIDTH=640, no axp_test | system.json |
| MM v4 | 752×560 | FB_WIDTH=752, no axp_test | system-v4.json |
| MM Plus | 640×480 | FB_WIDTH=640, has axp_test | system.mmp.json |
| MM Flip | 752×560 | FB_WIDTH=752, has axp_test | system.mmf.json |

### Variant-Specific Code

```c
// Detect variant
int is_plus_or_flip = (access("/customer/app/axp_test", F_OK) == 0);

// Get screen size from environment
int width = atoi(getenv("SCREEN_WIDTH") ?: "640");
int height = atoi(getenv("SCREEN_HEIGHT") ?: "480");

// Battery reading
if (is_plus_or_flip) {
    // Use AXP chip
    system("cd /customer/app/ ; ./axp_test");
} else {
    // Use GPIO pin 59
    int fd = open("/sys/devices/gpiochip0/gpio/gpio59/value", O_RDONLY);
}
```

## Useful Commands

### Build System

```bash
# Build all components
for dir in src/*/; do
    (cd "$dir" && [ -f Makefile ] && make clean && make)
done

# Create release image
./makeimg.sh

# Create update package (old -> new)
./update_make.sh ./v1.6 ./base ./update 1.7
```

### File Operations

```bash
# Find all C source files
find src -name "*.c"

# Find large files
find base -type f -size +1M -exec ls -lh {} \;

# Check image size
du -sh base/

# Count lines of code
find src -name "*.c" -o -name "*.h" | xargs wc -l
```

### Git Operations

```bash
# Update submodules
git submodule update --remote

# Check what changed
git status
git diff

# Commit changes
git add .
git commit -m "Description"
git push
```

## Common Issues

### Issue: Binary doesn't run on device

**Symptoms:** Binary fails with "not found" or "illegal instruction"

**Solutions:**
1. Verify cross-compilation target:
   ```bash
   file mybinary
   # Should show: ARM, EABI5, dynamically linked
   ```

2. Check dependencies:
   ```bash
   arm-linux-gnueabihf-readelf -d mybinary
   ```

3. Ensure correct flags:
   ```makefile
   CFLAGS = -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
   ```

### Issue: Screen not displaying correctly

**Symptoms:** Graphics garbled or wrong resolution

**Solutions:**
1. Check screen size environment variables
2. Use SDL_SetVideoMode with correct dimensions
3. Test on different variants

### Issue: No input response

**Symptoms:** Buttons don't work

**Solutions:**
1. Verify keymon is running:
   ```bash
   ps aux | grep keymon
   ```

2. Check event device:
   ```bash
   cat /dev/input/event0 | hexdump
   # Press buttons and verify output
   ```

3. Ensure correct button mapping in code

### Issue: Audio not working

**Symptoms:** No sound or crackling

**Solutions:**
1. Check audiofix setting in system.json
2. Use libpadsp for compatibility:
   ```bash
   LD_PRELOAD=/mnt/SDCARD/Koriki/lib/libpadsp.so ./myapp
   ```

3. Verify audio device:
   ```bash
   aplay -l
   ```

## Development Tips

### Optimization for Miyoo Mini

```c
// Use NEON for better performance
#ifdef __ARM_NEON
    // NEON-optimized code
#endif

// Minimize memory allocations
// Stack allocation preferred over malloc for small buffers

// Use framebuffer direct access when possible
// SDL is convenient but slower

// CPU frequency scaling
system("echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
```

### SDL Best Practices

```c
// Initialize SDL
SDL_Init(SDL_INIT_VIDEO);

// Set video mode (use environment variables)
int width = atoi(getenv("SCREEN_WIDTH") ?: "640");
int height = atoi(getenv("SCREEN_HEIGHT") ?: "480");
SDL_Surface *screen = SDL_SetVideoMode(width, height, 16, SDL_HWSURFACE);

// Load images efficiently
SDL_Surface *image = IMG_Load("image.png");
SDL_BlitSurface(image, NULL, screen, &dest_rect);
SDL_Flip(screen);

// Clean up
SDL_FreeSurface(image);
SDL_Quit();
```

### Power Management

```c
// Set CPU governor
void set_cpu_governor(const char *governor) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
        "echo %s > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",
        governor);
    system(cmd);
}

// Governors: ondemand (default), performance, powersave, conservative

// Screen brightness (0-10)
void set_brightness(int level) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
        "echo %d > /sys/class/pwm/pwmchip0/pwm0/duty_cycle",
        level * 10);
    system(cmd);
}
```

### Configuration Management

```c
#include "cJSON.h"

// Read config
cJSON *load_config(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    
    cJSON *json = cJSON_Parse(buffer);
    free(buffer);
    return json;
}

// Usage
cJSON *config = load_config("/mnt/SDCARD/system.json");
int brightness = cJSON_GetObjectItem(config, "lumination")->valueint;
cJSON_Delete(config);
```

## Testing

### Manual Testing Checklist

- [ ] Test on MM v1-v3 (640×480)
- [ ] Test on MM v4 (752×560)
- [ ] Test on MM Plus (with WiFi)
- [ ] Test on MM Flip (clamshell)
- [ ] Test battery display
- [ ] Test charging mode
- [ ] Test brightness controls
- [ ] Test volume controls
- [ ] Test sleep/wake
- [ ] Test all buttons
- [ ] Test with headphones
- [ ] Test SD card removal/insertion

### Automated Testing (Recommended)

```bash
# Unit tests (example with Check framework)
# Install: apt-get install check

# Test file: test_config.c
#include <check.h>

START_TEST(test_load_config) {
    cJSON *config = load_config("test_system.json");
    ck_assert_ptr_nonnull(config);
    ck_assert_int_eq(get_int_value(config, "lumination"), 8);
    cJSON_Delete(config);
}
END_TEST

# Run tests
gcc -o test_config test_config.c -lcheck -lm -lpthread
./test_config
```

## Resources

### Documentation

- **Main README:** [README.md](README.md)
- **Code Review:** [CODE_REVIEW_ANALYSIS.md](CODE_REVIEW_ANALYSIS.md)
- **Wiki:** https://github.com/Rparadise-Team/Koriki/wiki
- **SimpleMenu Docs:** https://github.com/fgl82/simplemenu

### External Links

- **Toolchain:** https://github.com/shauninman/union-miyoomini-toolchain
- **RetroArch:** https://github.com/libretro/RetroArch
- **Cores:** https://rparadise-team.github.io/Koriki/
- **Community:** https://t.me/Koriki_MiyooMini

### Hardware Info

- **CPU:** Allwinner F1C500s (ARM Cortex-A7, 900MHz)
- **RAM:** 64MB DDR1
- **Display:** 3.5" or 4.2" IPS LCD
- **Resolution:** 640×480 or 752×560
- **Storage:** microSD (up to 256GB)
- **Battery:** 2000mAh Li-ion
- **Input:** D-pad + 4 face buttons + 4 shoulder + Select/Start/Menu/Power

### Key Files to Know

| File | Purpose |
|------|---------|
| `base/.tmp_update/runtime.sh` | Boot initialization |
| `src/keymon/keymon.c` | Input daemon |
| `base/.simplemenu/config.ini` | SimpleMenu config |
| `base/Koriki/assets/system.json` | System settings |
| `base/.simplemenu/launchers/*` | Emulator launchers |

## Contributing

### Code Style

```c
// Use K&R style bracing
void function_name(int param) {
    if (condition) {
        // code
    } else {
        // code
    }
}

// Naming conventions
#define CONSTANT_NAME 123
typedef struct { ... } StructName;
int global_variable;
void my_function(void);
```

### Pull Request Checklist

- [ ] Code compiles without warnings
- [ ] Tested on at least one device variant
- [ ] No memory leaks (checked with valgrind)
- [ ] Documentation updated (if needed)
- [ ] Commit message is descriptive
- [ ] No unnecessary files committed (build artifacts, etc.)

### Getting Help

- **GitHub Issues:** https://github.com/Rparadise-Team/Koriki/issues
- **Telegram:** https://t.me/Koriki_MiyooMini
- **Discord:** Miyoo Mini community servers

---

**Last Updated:** February 3, 2026  
**Maintainer:** Rparadise-Team  
**License:** See individual components for licenses
