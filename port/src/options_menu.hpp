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

// The frontend's key hints (ENTER, ESC, DEL, BACKSPACE) as the gamepad's buttons once it was used last.
void PadKeyHint(u8 hint, wchar_t* pText);

// The current profile's saves (saves.hpp). RESUME SAVED STATUS is offered on the Play page with any, on
// the Wasted, Quit and Area complete pages with one of that stage.
bool IsLevel(Frontend* pFrontend, const PortSaves::Header& header);
bool HasSaves(Frontend* pFrontend);
bool HasSave(Frontend* pFrontend, u8 mainStage, u8 bonusStage);
// RESUME SAVED STATUS chosen: several saves are listed on the menu's page, a single one other than
// plyslotN.svg is booted. False to go on as the original.
bool Resume(Frontend* pFrontend);
// In LoadMapFilenamesFromSvg: the save to boot in place of plyslotN.svg, if Resume chose one.
void ResumeFileName(char_type* pFileName);
// After the original pages took the input: a new game that would replace the city's autosave (AUTOSAVE on) waits
// for a YES in the delete player dialog's look.
void AfterPageInput(Frontend* pFrontend);
// That dialog's input and drawing; false for the delete player dialog.
bool UpdateDialog(Frontend* pFrontend);
bool DrawDialog(Frontend* pFrontend);
// The frontend is back after a game: the retry checkpoint booted at once if RETRY ended it.
void AfterGame(Frontend* pFrontend);
} // namespace PortOptionsMenu
