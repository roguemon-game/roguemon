#ifndef GUARD_ROGUE_LOADING_H
#define GUARD_ROGUE_LOADING_H

// The loading screen (src/rogue_loading.c), shown while the game is busy
// making a region or a map on the spot and the main loop stands still: the
// region's name, where the player is headed, a bar and a line from the
// professor or the Director. It moves by itself (the VBlank interrupt walks
// the player along the bar), so it never looks frozen.

enum
{
    ROGUE_LOAD_NEW,      // a new run (after the professor's speech)
    ROGUE_LOAD_CONTINUE, // the main menu's CONTINUE
    ROGUE_LOAD_RUN0,     // RUN 0, or CONTINUE into it: the Director speaks
    ROGUE_LOAD_ENDLESS,  // ENDLESS, or CONTINUE into it: the Director speaks
};

// Sets the screen up (it wipes VRAM, OAM and the sprites) and starts its
// animation. title is the region's name; seed picks the line said.
void Rogue_LoadingShow(u8 kind, const u8 *title, u32 seed);
// Where the player is headed, once it is known ("Setting out for ...").
void Rogue_LoadingSetLine(const u8 *str);
// The work ahead: the bar creeps toward ceiling (percent), getting close in
// about frames frames.
void Rogue_LoadingStage(u8 ceiling, u8 frames);
bool8 Rogue_LoadingActive(void);
// Fills the bar, fades the screen to black and stops the animation. The
// screen is left black, with no windows or sprites, for the field to load.
void Rogue_LoadingHide(void);

#endif // GUARD_ROGUE_LOADING_H
