#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


// Buffer size for reading configuration files and version strings
#define BUFFER_LEN 1024

// Vertical spacing between lines of text in pixels
#define LINE_SPACING 40

/**
 * Load entire file contents into dynamically allocated buffer.
 * 
 * @param path Path to file to load
 * @return Pointer to allocated buffer containing file contents, or NULL on error.
 *         Caller must free() the returned buffer when done.
 */
char* load_file(char const* path) {
  char* buffer = NULL;
  long length = 0;
  FILE * f = fopen(path, "rb");

  if (!f) {
    fprintf(stderr, "Error: Cannot open file '%s'\n", path);
    return NULL;
  }

  // Get file size
  if (fseek(f, 0, SEEK_END) != 0) {
    fprintf(stderr, "Error: Cannot seek to end of file '%s'\n", path);
    fclose(f);
    return NULL;
  }

  length = ftell(f);
  if (length < 0) {
    fprintf(stderr, "Error: Cannot determine size of file '%s'\n", path);
    fclose(f);
    return NULL;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fprintf(stderr, "Error: Cannot seek to start of file '%s'\n", path);
    fclose(f);
    return NULL;
  }

  // Allocate buffer
  buffer = (char*) malloc((length + 1) * sizeof(char));
  if (!buffer) {
    fprintf(stderr, "Error: Cannot allocate %ld bytes for file '%s'\n", length + 1, path);
    fclose(f);
    return NULL;
  }

  // Read file
  if (fread(buffer, sizeof(char), length, f) != (size_t)length) {
    fprintf(stderr, "Error: Cannot read full contents of file '%s'\n", path);
    free(buffer);
    fclose(f);
    return NULL;
  }

  fclose(f);
  buffer[length] = '\0';

  return buffer;
}

/**
 * Load first line of file into provided buffer.
 * 
 * @param buffer Buffer to store file contents (must be at least BUFFER_LEN bytes)
 * @param path Path to file to load
 */
void load_file2(char* buffer, char const* path) {
  FILE * f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Warning: Cannot open file '%s'\n", path);
    buffer[0] = '\0';
    return;
  }

  if (!fgets(buffer, BUFFER_LEN, f)) {
    fprintf(stderr, "Warning: Cannot read from file '%s'\n", path);
    buffer[0] = '\0';
  }

  fclose(f);
}

int main(int argc , char* argv[]) {
  SDL_Surface* video = NULL;
  SDL_Surface* screen = NULL;
  char sys_version_str[BUFFER_LEN];
  char* kor_version_str = NULL;
  SDL_Color color_white = {255, 255, 255, 0};
  int running = 1;
  long unsigned int i;

  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Error: SDL_Init failed: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_ShowCursor(SDL_DISABLE);

  // Initialize SDL_ttf
  if (TTF_Init() < 0) {
    fprintf(stderr, "Error: TTF_Init failed: %s\n", TTF_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  // Load font
  TTF_Font* font = TTF_OpenFont("resources/Pmc.ttf", 48);
  if (!font) {
    fprintf(stderr, "Error: Cannot load font 'resources/Pmc.ttf': %s\n", TTF_GetError());
    TTF_Quit();
    SDL_Quit();
    return EXIT_FAILURE;
  }

  // Set video mode
  video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
  if (!video) {
    fprintf(stderr, "Error: SDL_SetVideoMode failed: %s\n", SDL_GetError());
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return EXIT_FAILURE;
  }

  // Create screen surface
  screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);
  if (!screen) {
    fprintf(stderr, "Error: SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return EXIT_FAILURE;
  }

  // Load and display background
  SDL_Surface* background = IMG_Load("resources/background.png");
  if (background) {
    SDL_BlitSurface(background, NULL, screen, NULL);
    SDL_FreeSurface(background);
  } else {
    fprintf(stderr, "Warning: Cannot load background image: %s\n", IMG_GetError());
    // Continue anyway with black background
  }

  // Get system version from firmware environment
  sys_version_str[0] = '\0';
  FILE* cmd_output = popen("/etc/fw_printenv miyoo_version", "r");
  if (cmd_output) {
    if (fgets(sys_version_str, BUFFER_LEN, cmd_output)) {
      // Parse "miyoo_version=..." format
      char* version_start = strchr(sys_version_str, '=');
      if (version_start != NULL) {
          version_start++;
          sscanf(version_start, "%s", sys_version_str);
      }
    }
    pclose(cmd_output);
  } else {
    fprintf(stderr, "Warning: Cannot read miyoo_version\n");
    strncpy(sys_version_str, "Unknown", BUFFER_LEN);
  } 
  
  // Render system version with word wrapping
  SDL_Surface* sysVersion;
  SDL_Rect rectSysVersion;
  char prevLine[BUFFER_LEN];
  prevLine[0] = 0;
  char line[BUFFER_LEN];
  line[0] = 0;
  int w, h;
  short int y = 84;  // Starting Y position for system version text
  char delim[] = " ";
  char *word = strtok(sys_version_str, delim);

  // Word-wrap system version text to fit display width (565 pixels max)
  while(word != NULL) {
    strncat(line, word, 256);
    strncat(line, " ", 1);
    TTF_SizeText(font, line, &w, &h);
    
    if (w > 565) {  // Line too wide, render previous line and start new one
      sysVersion = TTF_RenderUTF8_Blended(font, prevLine, color_white);
      if (sysVersion) {
        rectSysVersion = {35, y, 565, 50};
        SDL_BlitSurface(sysVersion, NULL, screen, &rectSysVersion);
        SDL_FreeSurface(sysVersion);
      }
      
      prevLine[0] = 0;
      strncpy(line, word, 256);
      strncat(line, " ", 1);
      y += LINE_SPACING;
    }
    
    strncpy(prevLine, line, BUFFER_LEN);
    word = strtok(NULL, delim);
  }

  // Render final line, replacing newlines with spaces
  for (i = 0; i <= strlen(prevLine); i++) {
    if(prevLine[i] == 10) prevLine[i] = 32;
  }
  sysVersion = TTF_RenderUTF8_Blended(font, prevLine, color_white);
  if (sysVersion) {
    rectSysVersion = {35, y, 565, 50};
    SDL_BlitSurface(sysVersion, NULL, screen, &rectSysVersion);
    SDL_FreeSurface(sysVersion);
  }

  // Load and display Koriki version
#if defined(PLATFORM_PC)
  kor_version_str = load_file("version.txt");
#else
  kor_version_str = load_file("/mnt/SDCARD/Koriki/version.txt");
#endif

  if (kor_version_str && strlen(kor_version_str)) {
    // Replace newlines with spaces
    for (i = 0; i <= strlen(kor_version_str); i++) {
      if(kor_version_str[i] == 10) kor_version_str[i] = 32;
    }
    
    SDL_Surface* korVersion = TTF_RenderUTF8_Blended(font, kor_version_str, color_white);
    if (korVersion) {
      SDL_Rect rectKorVersion = {35, 395, 565, 51};
      SDL_BlitSurface(korVersion, NULL, screen, &rectKorVersion);
      SDL_FreeSurface(korVersion);
    }
  }

  // Free dynamically allocated version string
  if (kor_version_str) {
    free(kor_version_str);
  }

  // Display final screen
  SDL_BlitSurface(screen, NULL, video, NULL);
  SDL_Flip(video);
  
  // Wait for any key press or quit event
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      running = !(event.type == SDL_KEYDOWN || event.type == SDL_QUIT);
    }
  }

  // Cleanup resources
  if (font) TTF_CloseFont(font);
  if (screen) SDL_FreeSurface(screen);
  // Note: video surface is freed by SDL_Quit()
  
  TTF_Quit();
  SDL_Quit();

  return EXIT_SUCCESS;
}