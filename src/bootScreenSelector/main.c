#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "sys/ioctl.h"
#include <dirent.h>

// Maximum number of boot screens that can be stored
#define NUMBER_OF_BS 200

// Maximum length of boot screen directory names
#define MAX_BS_NAME_SIZE 256

// Font outline thickness in pixels for better readability
#define FONT_OUTLINE 2

// Button key mappings (SDL keyboard codes mapped to Miyoo Mini buttons)
#define	BUTTON_A	SDLK_SPACE
#define	BUTTON_B	SDLK_LCTRL
#define	BUTTON_X	SDLK_LSHIFT
#define	BUTTON_Y	SDLK_LALT
#define	BUTTON_START	SDLK_RETURN
#define	BUTTON_SELECT	SDLK_RCTRL
#define	BUTTON_MENU	SDLK_ESCAPE
#define	BUTTON_L2	SDLK_TAB
#define	BUTTON_R2	SDLK_BACKSPACE
#define BUTTON_RIGHT SDLK_RIGHT
#define BUTTON_LEFT SDLK_LEFT

// Path to directory containing boot screen folders
#define BS_PATH "bootScreens"

// Destination path for selected boot screen
#ifdef PLATFORM_PC
#define BOOTSCREEN_PATH "."
#else
// Koriki boot screen location (SimpleMenu loading screen)
#define BOOTSCREEN_PATH "/mnt/SDCARD/.simplemenu/resources/loading.png"
#endif


// Global variables
char bss[NUMBER_OF_BS][MAX_BS_NAME_SIZE];  // Array of boot screen directory names
SDL_Surface* video;
SDL_Surface* screen;
TTF_Font* font40;
TTF_Font* font40_outline;
SDL_Surface* surfaceBSS;
SDL_Surface* surfaceName;
SDL_Surface* surfaceName1;
SDL_Surface* imagePages;
SDL_Surface* imagePages1;
SDL_Surface* surfaceArrowLeft;
SDL_Surface* surfaceArrowRight;

// UI element positions
SDL_Rect rectArrowLeft = {22, 222, 32, 36};    // Left navigation arrow
SDL_Rect rectArrowRight = {586, 222, 32, 36};  // Right navigation arrow
SDL_Rect rectName;                              // Boot screen name position (calculated)
SDL_Rect rectPages = {500, 425, 85, 54};       // Page counter position

// Colors for text rendering
SDL_Color color_white = {255, 255, 255, 0};
SDL_Color color_black = {0, 0, 0, 0};

// State variables
int nCurrentPage = 0;  // Currently displayed boot screen index
int levelPage = 0;     // 0 = browsing, 1 = confirmation dialog
int bsCount = 0;       // Total number of boot screens found


/**
 * Log debug message to file for troubleshooting.
 * 
 * @param Message Message string to log
 */
void logMessage(char* Message) {
    FILE *file = fopen("log_BSS.txt", "a");
    if (!file) {
        fprintf(stderr, "Warning: Cannot open log file\n");
        return;
    }

    fprintf(file, "%s\n", Message);
    fclose(file);
}

/**
 * Check if a file exists.
 * 
 * @param filename Path to file to check
 * @return true if file exists, false otherwise
 */
bool file_exists(char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

/**
 * Case-insensitive comparison function for directory sorting.
 * Used with scandir() to alphabetically sort boot screen directories.
 */
int alphasort_no_case(const struct dirent **a, const struct dirent **b) {
    return strcasecmp((*a)->d_name, (*b)->d_name);
}

/**
 * Display boot screen preview with navigation arrows and page counter.
 * 
 * @param nbs Index of boot screen to display (0 to bsCount-1)
 */
void showBS(int nbs) {
    char cBSPath[250];
    char cName[250];
    char cPages[10];

    // Load and display boot screen image
    snprintf(cBSPath, sizeof(cBSPath), BS_PATH"/%s/bootScreen.png", bss[nbs]);
    surfaceBSS = IMG_Load(cBSPath);
    if (!surfaceBSS) {
        fprintf(stderr, "Error: Cannot load boot screen '%s': %s\n", cBSPath, IMG_GetError());
        return;
    }
    SDL_BlitSurface(surfaceBSS, NULL, screen, NULL);
    SDL_FreeSurface(surfaceBSS);

    // Show left arrow if not on first page
    if (nbs != 0) {
        SDL_BlitSurface(surfaceArrowLeft, NULL, screen, &rectArrowLeft);
    }

    // Show right arrow if not on last page
    if (nbs != (bsCount - 1)) {
        SDL_BlitSurface(surfaceArrowRight, NULL, screen, &rectArrowRight);
    }

    // Render boot screen name with outline for readability
    snprintf(cName, sizeof(cName), "%s", bss[nbs]);
    surfaceName1 = TTF_RenderUTF8_Blended(font40_outline, cName, color_black);
    surfaceName = TTF_RenderUTF8_Blended(font40, cName, color_white);
    
    if (surfaceName && surfaceName1) {
        SDL_Rect rect = {FONT_OUTLINE, FONT_OUTLINE, surfaceName->w, surfaceName->h};
        SDL_BlitSurface(surfaceName, NULL, surfaceName1, &rect);
        SDL_FreeSurface(surfaceName);
        
        // Center the name horizontally
        rectName = {320 - surfaceName1->w / 2, 5, surfaceName1->w, surfaceName1->h};
        SDL_BlitSurface(surfaceName1, NULL, screen, &rectName);
        SDL_FreeSurface(surfaceName1);
    }

    // Render page counter (e.g., "3/10")
    snprintf(cPages, sizeof(cPages), "%d/%d", (nbs + 1), bsCount);
    imagePages1 = TTF_RenderUTF8_Blended(font40_outline, cPages, color_black);
    imagePages = TTF_RenderUTF8_Blended(font40, cPages, color_white);
    
    if (imagePages && imagePages1) {
        SDL_Rect rect = {FONT_OUTLINE, FONT_OUTLINE, imagePages->w, imagePages->h};
        SDL_BlitSurface(imagePages, NULL, imagePages1, &rect);
        SDL_FreeSurface(imagePages);
        SDL_BlitSurface(imagePages1, NULL, screen, &rectPages);
        SDL_FreeSurface(imagePages1);
    }
}


int main(void) {
    int running = 1;
    char cBSPath[250];
    char cCommand[250];

    // Scan bootScreens directory for available boot screens
    struct dirent **files;
    int n = scandir(BS_PATH, &files, NULL, alphasort_no_case);
    if (n < 0) {
        perror("Error: Cannot open bootScreens directory");
        return EXIT_FAILURE;
    }

    // Build list of valid boot screen directories
    for (int i = 0; i < n; i++) {
        struct dirent *ent = files[i];
        
        // Only process directories
        if (ent->d_type == DT_DIR)  {
            snprintf(cBSPath, sizeof(cBSPath), BS_PATH"/%s/bootScreen.png", ent->d_name);
            
            // Check if bootScreen.png exists in this directory
            if (file_exists(cBSPath)) {
                if (bsCount < NUMBER_OF_BS) {
                    strncpy(bss[bsCount], ent->d_name, MAX_BS_NAME_SIZE - 1);
                    bss[bsCount][MAX_BS_NAME_SIZE - 1] = '\0';  // Ensure null termination
                    bsCount++;
                } else {
                    fprintf(stderr, "Warning: Maximum number of boot screens (%d) reached\n", NUMBER_OF_BS);
                }
            }
        }
        free(files[i]);
    }
    free(files);

    if (bsCount == 0) {
        fprintf(stderr, "Error: No valid boot screens found in %s/\n", BS_PATH);
        return EXIT_FAILURE;
    }

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

    // Set video mode
    video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    if (!video) {
        fprintf(stderr, "Error: SDL_SetVideoMode failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Create screen surface
    screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);
    if (!screen) {
        fprintf(stderr, "Error: SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Load UI resources
    surfaceArrowLeft = IMG_Load("resources/arrowLeft.png");
    if (!surfaceArrowLeft) {
        fprintf(stderr, "Warning: Cannot load left arrow: %s\n", IMG_GetError());
    }

    surfaceArrowRight = IMG_Load("resources/arrowRight.png");
    if (!surfaceArrowRight) {
        fprintf(stderr, "Warning: Cannot load right arrow: %s\n", IMG_GetError());
    }

    font40 = TTF_OpenFont("resources/Exo-2-Bold-Italic.ttf", 40);
    if (!font40) {
        fprintf(stderr, "Error: Cannot load font: %s\n", TTF_GetError());
        if (surfaceArrowLeft) SDL_FreeSurface(surfaceArrowLeft);
        if (surfaceArrowRight) SDL_FreeSurface(surfaceArrowRight);
        SDL_FreeSurface(screen);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    font40_outline = TTF_OpenFont("resources/Exo-2-Bold-Italic.ttf", 40);
    if (font40_outline) {
        TTF_SetFontOutline(font40_outline, FONT_OUTLINE);
    }

    // Display initial boot screen
    showBS(nCurrentPage);

    // Main event loop
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN) {
                if (((int)event.key.keysym.sym) == BUTTON_B) {
                    // B button: Cancel/Back
                    if (levelPage == 0) {
                        running = 0;  // Exit program
                    } else {
                        levelPage = 0;  // Cancel confirmation, return to browsing
                    }
                } else if (((int)event.key.keysym.sym) == BUTTON_A) {
                    // A button: Select/Confirm
                    if (levelPage == 1) {
                        // Install selected boot screen
                        snprintf(cCommand, sizeof(cCommand), 
                                "cp \"" BS_PATH "/%s/bootScreen.png\" %s; sync", 
                                bss[nCurrentPage], BOOTSCREEN_PATH);
                        
                        int result = system(cCommand);
                        if (result != 0) {
                            fprintf(stderr, "Error: Failed to install boot screen (exit code: %d)\n", result);
                        }
                        running = 0;
                    } else {
                        levelPage = 1;  // Show confirmation dialog
                    }
                } else if (((int)event.key.keysym.sym) == BUTTON_RIGHT) {
                    // Navigate to next boot screen
                    if (nCurrentPage < (bsCount - 1)) {
                        nCurrentPage++;
                    }
                } else if (((int)event.key.keysym.sym) == BUTTON_LEFT) {
                    // Navigate to previous boot screen
                    if (nCurrentPage > 0) {
                        nCurrentPage--;
                    }
                }
            } else if (event.type == SDL_QUIT) {
                running = 0;
            }

            // Update display with current boot screen
            showBS(nCurrentPage);
            
            if (levelPage == 1) {
                // Show confirmation dialog overlay
                surfaceBSS = IMG_Load("resources/confirm.png");
                if (surfaceBSS) {
                    SDL_BlitSurface(surfaceBSS, NULL, screen, NULL);
                    SDL_FreeSurface(surfaceBSS);
                } else {
                    fprintf(stderr, "Warning: Cannot load confirmation dialog: %s\n", IMG_GetError());
                }
            }
        }
        
        SDL_BlitSurface(screen, NULL, video, NULL);
        SDL_Flip(video);
    }

    // Cleanup resources
    if (font40) TTF_CloseFont(font40);
    if (font40_outline) TTF_CloseFont(font40_outline);
    if (surfaceArrowLeft) SDL_FreeSurface(surfaceArrowLeft);
    if (surfaceArrowRight) SDL_FreeSurface(surfaceArrowRight);
    if (screen) SDL_FreeSurface(screen);
    // Note: video surface is freed by SDL_Quit()

    TTF_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}
