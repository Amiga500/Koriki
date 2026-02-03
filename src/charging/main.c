#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <poll.h>

// Animation timing: 200ms delay between frames (200,000 microseconds)
#define ANIMATION_DELAY 200000

// Number of times to loop the full animation before going to black screen
#define ANIMATION_LOOPS 10

// Total number of animation frames (chargingState0.png through chargingState5.png)
#define ANIMATION_IMAGES 6

// ioctl commands for MI audio system
#define MI_AO_SETVOLUME 0x4008690b
#define MI_AO_GETVOLUME 0xc008690c

// Display resolution for Miyoo Mini (may be 640x480 or 752x560 depending on variant)
#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 480

//  Button key code definitions (Linux input event codes)
#define BUTTON_MENU   KEY_ESC
#define BUTTON_POWER  KEY_POWER
#define BUTTON_SELECT KEY_RIGHTCTRL
#define BUTTON_START  KEY_ENTER
#define BUTTON_L1     KEY_E
#define BUTTON_R1     KEY_T
#define BUTTON_L2     KEY_TAB
#define BUTTON_R2     KEY_BACKSPACE
#define BUTTON_A      KEY_SPACE
#define BUTTON_B      KEY_LEFTCTRL
#define BUTTON_X      KEY_LEFTSHIFT
#define BUTTON_Y      KEY_LEFTALT
#define BUTTON_UP     KEY_UP
#define BUTTON_DOWN   KEY_DOWN
#define BUTTON_LEFT   KEY_LEFT
#define BUTTON_RIGHT  KEY_RIGHT
#define BUTTON_VOLUMEUP		KEY_VOLUMEUP
#define BUTTON_VOLUMEDOWN	KEY_VOLUMEDOWN

//  Input event value definitions
#define RELEASED  0  // Button released
#define PRESSED   1  // Button pressed
#define REPEAT    2  // Button held (repeat event)


//  Global Variables
static struct input_event ev;
static int  input_fd = 0;
static struct pollfd fds[1];
static int is_charging = 0;  // 1 if device is currently charging, 0 otherwise
static bool running = true;
static bool screen_on = true;
static int animation_image = 0;   // Current animation frame (0-5)
static int animation_loop = 0;    // Current animation loop count
static int mmp = 0;               // 1 if Miyoo Mini Plus/Flip (has AXP chip), 0 otherwise
static time_t last_activity_time;


/**
 * Check if device is currently charging.
 * 
 * Detection method varies by device variant:
 * - MM Plus/Flip: Uses AXP power management chip via axp_test binary
 * - MM v1-v4: Reads GPIO pin 59 (0 = charging, 1 = not charging)
 * 
 * Updates global variable: is_charging
 */
void checkCharging(void) {
  int charging = 0;
  
  // Check for AXP chip (Miyoo Mini Plus/Flip)
  if (access("/customer/app/axp_test", F_OK) == 0) {
    mmp = 1;
    char *cmd = "cd /customer/app/ ; ./axp_test";
    int axp_response_size = 100;
    char buf[axp_response_size];
    int battery = 0;
    int voltage = 0;

    FILE *fp = popen(cmd, "r");
    if (!fp) {
      fprintf(stderr, "Error: Cannot execute axp_test\n");
      is_charging = 0;
      return;
    }

    // Parse JSON response: {"battery":XX, "voltage":XXXX, "charging":X}
    if (fgets(buf, axp_response_size, fp) != NULL) {
      if (sscanf(buf, "{\"battery\":%d, \"voltage\":%d, \"charging\":%d}", 
                 &battery, &voltage, &charging) != 3) {
        fprintf(stderr, "Warning: Cannot parse axp_test output\n");
        charging = 0;
      }
    }
    pclose(fp);
    is_charging = charging;
  } else {
    // Miyoo Mini v1-v4: Read GPIO pin 59
    FILE *file = fopen("/sys/devices/gpiochip0/gpio/gpio59/value", "r");
    if (!file) {
      fprintf(stderr, "Error: Cannot open GPIO59 for charging detection\n");
      is_charging = 0;
      return;
    }

    if (fscanf(file, "%i", &charging) != 1) {
      fprintf(stderr, "Warning: Cannot read GPIO59 value\n");
      charging = 0;
    }
    fclose(file);
    
    // GPIO: 0 = charging, 1 = not charging (inverted logic)
    is_charging = !charging;
  }
}

/**
 * Log debug message to file (only if log file exists).
 * Used for troubleshooting charging mode behavior.
 * 
 * @param Message Message string to log
 */
void logMessage(char* Message) {
  const char* log_path = "/mnt/SDCARD/.tmp_update/log_charging_Message.txt";
  
  // Only log if debug log file already exists
  if (access(log_path, F_OK) != 0) {
    // Create log file if it doesn't exist
    FILE *file = fopen(log_path, "w");
    if (!file) {
      fprintf(stderr, "Warning: Cannot create log file\n");
      return;
    }
    fclose(file);
  }
  
  FILE *file = fopen(log_path, "a");
  if (!file) {
    fprintf(stderr, "Warning: Cannot open log file for appending\n");
    return;
  }
  
  fprintf(file, "%s\n", Message);
  fclose(file);
}

/**
 * Set screen brightness level.
 * 
 * @param value Brightness level from 0 (off) to 10 (maximum)
 */
void SetBrightness(int value) {
  if (value < 0) value = 0;
  if (value > 10) value = 10;
  
  int fd = open("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", O_WRONLY);
  if (fd < 0) {
    fprintf(stderr, "Error: Cannot open PWM duty_cycle for brightness control\n");
    return;
  }
  
  // PWM duty cycle: value * 10 (0-100 range)
  dprintf(fd, "%d", value * 10);
  close(fd);
}

static void sigHandler(int sig) {
  switch (sig) {
    case SIGINT:
    case SIGTERM:
      running = false;
      break;
    default: break;
  }
}

int main(void) {
  signal(SIGINT, sigHandler);
  signal(SIGTERM, sigHandler);

  checkCharging();
  if (is_charging == 0){
    return EXIT_SUCCESS;
  }

  // Prepare for Poll button input
  input_fd = open("/dev/input/event0", O_RDONLY);
  int fd = open("/dev/mi_ao", O_RDWR);
  memset(&fds, 0, sizeof(fds));
  fds[0].fd = input_fd;
  fds[0].events = POLLIN;

  SDL_Init(SDL_INIT_VIDEO);
  SDL_ShowCursor(SDL_DISABLE);

  SDL_Surface* video = SDL_SetVideoMode(640,480, 32, SDL_HWSURFACE);
  SDL_Surface* screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640,480, 32, 0,0,0,0);
  SDL_Surface* black_image = IMG_Load("/mnt/SDCARD/Koriki/images/black.png");

  int image_index = 0;
  SDL_Surface *image;
  SDL_Surface *images[6];
  char image_path[100];
  for (int i = 0; i < ANIMATION_IMAGES; i++) {
    snprintf(image_path, 99, "/mnt/SDCARD/Koriki/images/chargingState%d.png", i);
    if ((image = IMG_Load(image_path)))
      images[image_index++] = image;
  }

  SetBrightness(8);

  bool power_pressed = false;
  int repeat_power = 0;
  bool screen_on = true;
  bool sound = false;
  last_activity_time = time(NULL);

  while (running) {
    if (animation_loop < ANIMATION_LOOPS) {
      SDL_BlitSurface(images[animation_image++], NULL, screen, NULL);
      SDL_BlitSurface(screen, NULL, video, NULL);
      SDL_Flip(video);
      if (animation_image == ANIMATION_IMAGES) {
        animation_image = 0;
        animation_loop++;
      }
    } else {
      if (screen_on) SetBrightness(0);
	    SDL_BlitSurface(black_image, NULL, screen, NULL);
	    SDL_BlitSurface(screen, NULL, video, NULL);
	    SDL_Flip(video);
      screen_on = false;
    }

    checkCharging();
    if (is_charging == 0){
        system("shutdown; sleep 10");
    }

    if (!screen_on && !sound) {
      if (fd >= 0) {
	       int buf2[] = {0, 0};
	       uint64_t buf1[] = {sizeof(buf2), (uintptr_t)buf2};
	       ioctl(fd, MI_AO_GETVOLUME, buf1);
	       int recent_volume = buf2[1];
	       buf2[1] = -60;
	       if (buf2[1] != recent_volume) 
		        ioctl(fd, MI_AO_SETVOLUME, buf1);
		     }
      system("tinymix set 6 40 &");
	    system("echo powersave > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
		  sound = true;
    } else if (screen_on && sound) {
      if (fd >= 0) {
	       int buf2[] = {0, 0};
	       uint64_t buf1[] = {sizeof(buf2), (uintptr_t)buf2};
	       ioctl(fd, MI_AO_GETVOLUME, buf1);
	       int recent_volume = buf2[1];
	       buf2[1] = 0;
	       if (buf2[1] != recent_volume) 
	          ioctl(fd, MI_AO_SETVOLUME, buf1);
            }
	    system("tinymix set 6 100 &");
      sound = false;
    }
      
    while (poll(fds, 1, 0)) {
      read(input_fd, &ev, sizeof(ev));
      if (ev.type != EV_KEY || ev.value > REPEAT) continue;

      if (ev.code == BUTTON_POWER) {
        if (ev.value == PRESSED) {
          power_pressed = true;
          repeat_power = 0;
        } else if (ev.value == RELEASED && power_pressed) {
          power_pressed = false;
	        screen_on = true;
	        SetBrightness(8);
	        animation_loop = 0;
        } else if (ev.value == REPEAT) {
          if (repeat_power >= 5) {
            screen_on = true;
            SetBrightness(8);
		        system("echo ondemand > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
		        system("echo 1200000 > /sys/devices/system/cpu/cpu1/cpufreq/scaling_max_freq");
            running = false; // power on
          }
          repeat_power++;
        }
      }
    }
    
    if (mmp) { // autopoweroff MMP in 60s
            time_t current_time = time(NULL);
            if (current_time - last_activity_time >= 60) {
                system("shutdown; sleep 10");
                running = false;
            }
        }

    usleep(ANIMATION_DELAY);
  }

  SDL_FreeSurface(screen);
  SDL_FreeSurface(video);
  SDL_FreeSurface(black_image);
  SDL_Quit();

  return EXIT_SUCCESS;
}
