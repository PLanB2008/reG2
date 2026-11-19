// The settings menu (menu.hpp) in the frontend, on a page the original leaves unused (12), reached
// from the start menu's "Options" in place of the external GTA2 Manager.
#pragma once

#include "Frontend.hpp"
#include "saves.hpp"

namespace PortOptionsMenu
{
// The start menu's corner line, our version in place of the game's, then a newer release in red if there is one.
void VersionText(wchar_t* pOut);
void DrawUpdate(wchar_t* pVersion, s16 font);

// After the original pages are set up.
void Setup(Frontend* pFrontend);

// Input and state of the menu's page; false when another page is current.
bool Update(Frontend* pFrontend);

// After the frontend drew the page.
void Draw(Frontend* pFrontend);

// The player name typed with the gamepad: up and down change the last letter, right adds one, left
// removes one, A keeps the name, B cancels. False when the keyboard is in use.
bool UpdateNameEntry(Frontend* pFrontend);

// Frontend::DrawKeyHint's drawing of pText, hint's text; its width. ENTER, ESC, DEL and BACKSPACE are the pictures of
// the keys, or of the gamepad's buttons once it was used last, and the labels after them lose their colon.
s32 DrawKeyHint(u8 hint, wchar_t* pText, u16 x, u16 y, u16 font, s32 palette);

// The current profile's saves (saves.hpp). RESUME SAVED STATUS is offered on the Play page with any, on
// the Wasted, Quit and Area complete pages with one of that stage. Without a frontend, in game: the levels it read.
bool IsLevel(Frontend* pFrontend, const PortSaves::Header& header);
// LOAD GAME's saves, and the cities it can show if pCities.
s32 ListSaves(Frontend* pFrontend, PortSaves::Entry* pEntries, bool* pCities);
bool HasSaves(Frontend* pFrontend);
bool HasSave(Frontend* pFrontend, u8 mainStage, u8 bonusStage);
// RESUME SAVED STATUS chosen: the saves are listed on the menu's page, even a single one. False to go on as the
// original if there is none to list.
bool Resume(Frontend* pFrontend);
// In LoadMapFilenamesFromSvg: the save to boot in place of plyslotN.svg, if Resume chose one.
void ResumeFileName(char_type* pFileName);
// After the original pages took the input: a new game that would replace the city's autosave (AUTOSAVE on) waits
// for a CONTINUE in the delete player dialog's look.
void AfterPageInput(Frontend* pFrontend);
// That dialog's input and drawing; false for the delete player dialog.
bool UpdateDialog(Frontend* pFrontend);
bool DrawDialog(Frontend* pFrontend);
// The frontend is set up, at the start or after a game: its levels kept for the pause menu's LOAD GAME, and the save
// booted at once if RETRY or that LOAD GAME ended the game.
void AfterGame(Frontend* pFrontend);
} // namespace PortOptionsMenu
