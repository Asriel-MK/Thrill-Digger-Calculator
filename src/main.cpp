/*
=================================================================================================
FILE: src/main.cpp

DESCRIPTION:
This file is the main entry point for the Thrill Digger Calculator application.
It implements the Graphical User Interface (GUI) using the native Windows API (Win32).

IMPORTANCE:
It acts as the bridge between the user and the solving logic. It handles:
- Creating the application window.
- Drawing the grid of cells.
- Handling user clicks and combo box selections.
- Calling the `solver` to calculate probabilities.
- Updating the UI colors and text based on those probabilities.

INTERACTION:
- Includes "solver.h" to access the `ThrillDiggerSolver` class.
- Uses Windows API functions (user32, gdi32, comctl32) for rendering and input.
- Defines the `WinMain` function, which is where execution starts for Windows GUI apps.

OVERVIEW OF LOGIC:
The app creates a window with a grid of 5x8 cells (Expert mode). Each cell has:
1. A visual background (color-coded by safety).
2. A probability text label (e.g., "40% Bad").
3. A dropdown (ComboBox) to let the user select what they found in that cell.

When a user changes a dropdown, the app recalculates probabilities and repaints the grid.
=================================================================================================
*/

// Standard definitions for Windows programming
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX            // Prevent Windows macros from conflicting with std::min/max
#include <windows.h>        // Main Windows API header
#include <commctrl.h>       // Common Controls (for visual styles)
#include <windowsx.h>       // Macro helpers for Windows API
#include <cstdio>           // For snprintf, etc.
#include <string>           // C++ string
#include <algorithm>        // Algorithms like std::clamp
#include "solver.h"         // Our custom solver logic

// Link against the Common Controls library automatically.
// This is required for visual styles (like XP/Vista/Win10 look) on controls.
#pragma comment(lib, "comctl32.lib")

// Embed a manifest dependency to tell Windows we want to use Visual Styles (Common Controls v6).
// Without this, the app would look like Windows 95 (grey, blocky buttons).
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// =================================================================================================
// LAYOUT CONSTANTS
// These define the size and spacing of UI elements in pixels.
// =================================================================================================
constexpr int CELL_W = 110;     // Width of one grid cell
constexpr int CELL_H = 60;      // Height of one grid cell
constexpr int GRID_PAD = 6;     // Padding between cells
constexpr int GRID_BORDER = 3;  // Thickness of the border around the grid
constexpr int TOP_MARGIN = 50;  // Space at the top of the window
constexpr int BOTTOM_MARGIN = 50;// Space at the bottom
constexpr int SIDE_MARGIN = 20; // Space on left/right

// =================================================================================================
// CONTROL IDs
// Every UI element (button, dropdown) needs a unique integer ID to identify it in messages.
// =================================================================================================
constexpr int ID_COMBO_BASE = 1000; // Starting ID for the 40 combo boxes (1000 to 1039)
constexpr int ID_RESET_BTN = 2000;  // ID for the "Reset" button

// =================================================================================================
// COLORS
// Standard colors used to represent game elements and probabilities.
// =================================================================================================
static COLORREF colorUndug      = RGB(173, 255, 47);  // Yellow-green (default unknown state)
static COLORREF colorGreen      = RGB(0, 200, 0);     // Green Rupee
static COLORREF colorBlue       = RGB(0, 120, 215);   // Blue Rupee
static COLORREF colorRed        = RGB(220, 50, 50);   // Red Rupee
static COLORREF colorSilver     = RGB(200, 200, 200); // Silver Rupee
static COLORREF colorGold       = RGB(255, 215, 0);   // Gold Rupee
static COLORREF colorRupoor     = RGB(80, 0, 80);     // Rupoor (Dark purple)
static COLORREF colorBomb       = RGB(50, 50, 50);    // Bomb (Dark gray)
static COLORREF colorBg         = RGB(30, 30, 40);    // Window background color (Dark theme)

/*
 * Helper Function: probColor
 * --------------------------
 * Calculates a color gradient based on the probability of a cell being "Bad".
 * 
 * Logic:
 * - 0% Bad   -> Green (Safe)
 * - 50% Bad  -> Yellow (Caution)
 * - 100% Bad -> Red (Danger)
 *
 * It interpolates between these colors to give a visual heat map.
 */
static COLORREF probColor(double prob) {
    int r, g, b;
    if (prob <= 0.0) {
        return RGB(100, 220, 60); // Safe green
    } else if (prob >= 1.0) {
        return RGB(220, 40, 40);  // Danger red
    } else if (prob < 0.5) {
        // Gradient: Green -> Yellow
        double t = prob / 0.5;
        r = (int)(100 + t * 155);
        g = (int)(220 - t * 30);
        b = (int)(60 - t * 40);
    } else {
        // Gradient: Yellow -> Red
        double t = (prob - 0.5) / 0.5;
        r = (int)(255 - t * 35);
        g = (int)(190 - t * 150);
        b = (int)(20 + t * 20);
    }
    // Clamp values to valid 0-255 range just in case
    return RGB(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

// =================================================================================================
// GLOBAL VARIABLES
// In a simple Win32 app, globals are often used to store handles to UI elements.
// =================================================================================================
static HINSTANCE g_hInst;                  // Handle to the application instance
static ThrillDiggerSolver g_solver;        // The logic engine instance
static HWND g_combos[TOTAL_CELLS];         // Array of handles to the 40 dropdowns
static HWND g_cellPanels[TOTAL_CELLS];     // Array of handles to the background panels
static HWND g_probLabels[TOTAL_CELLS];     // Array of handles to the text labels
static HWND g_resetBtn;                    // Handle to Reset button
static HWND g_titleLabel;                  // Handle to Title text
static HWND g_infoLabel;                   // Handle to Status/Info text at bottom

// Fonts
static HFONT g_fontNormal;
static HFONT g_fontBold;
static HFONT g_fontTitle;
static HFONT g_fontSmall;

// Graphics objects (Brushes) for painting backgrounds efficiently
static HBRUSH g_cellBrushes[TOTAL_CELLS];

// Forward declaration of functions
static void UpdateUI(HWND hWnd);

/*
 * RecalcAndUpdate
 * ---------------
 * Runs the solver algorithm and then refreshes the screen.
 * Called whenever the user changes a value.
 */
static void RecalcAndUpdate(HWND hWnd) {
    g_solver.solve();            // Run the math
    UpdateUI(hWnd);              // Update text/colors
    InvalidateRect(hWnd, NULL, TRUE); // Force a repaint of the window
}

/*
 * getCellBgColor
 * --------------
 * Determines what background color a cell should have.
 * - If "Undug" (unknown): Uses the probability gradient color.
 * - If Revealed: Uses the specific color for that item (Green/Red/Bomb etc).
 */
static COLORREF getCellBgColor(int idx) {
    CellContent c = g_solver.grid[idx];
    if (c == CellContent::Undug) {
        return probColor(g_solver.badProb[idx]);
    }
    switch(c) {
        case CellContent::Green:  return colorGreen;
        case CellContent::Blue:   return colorBlue;
        case CellContent::Red:    return colorRed;
        case CellContent::Silver: return colorSilver;
        case CellContent::Gold:   return colorGold;
        case CellContent::Rupoor: return colorRupoor;
        case CellContent::Bomb:   return colorBomb;
        default: return colorUndug;
    }
}

/*
 * UpdateUI
 * --------
 * Iterates through all cells and updates their visual state.
 * 1. Updates the text (e.g., "35% Bad").
 * 2. Creates new brushes for the background colors.
 * 3. Updates the bottom info bar with counts (Revealed / Total Bad).
 */
static void UpdateUI(HWND /*hWnd*/) {
    for (int i = 0; i < TOTAL_CELLS; i++) {
        // Update probability label text
        char buf[64];
        CellContent c = g_solver.grid[i];
        
        if (c == CellContent::Undug) {
            // Convert probability (0.0 - 1.0) to percentage (0 - 100)
            int pct = (int)std::round(g_solver.badProb[i] * 100.0);
            snprintf(buf, sizeof(buf), "%d%% Bad", pct);
        } else {
            // If revealed, we don't show probability text (it's 0% or 100% implicitly)
            buf[0] = '\0'; 
        }
        SetWindowTextA(g_probLabels[i], buf);

        // Update the brush used to paint the cell background
        if (g_cellBrushes[i]) DeleteObject(g_cellBrushes[i]);
        g_cellBrushes[i] = CreateSolidBrush(getCellBgColor(i));

        // Mark the cell area as "dirty" so Windows repaints it soon
        InvalidateRect(g_cellPanels[i], NULL, TRUE);
        InvalidateRect(g_probLabels[i], NULL, TRUE);
    }

    // Calculate stats for the bottom info bar
    int knownBad = 0, revealed = 0;
    for (int i = 0; i < TOTAL_CELLS; i++) {
        if (isRevealedBad(g_solver.grid[i])) knownBad++;
        if (isRevealed(g_solver.grid[i])) revealed++;
    }
    char infoBuf[256];
    snprintf(infoBuf, sizeof(infoBuf),
        "Revealed: %d / %d    |    Bad spots found: %d / %d    |    Remaining bad: %d",
        revealed, TOTAL_CELLS, knownBad, TOTAL_BAD, TOTAL_BAD - knownBad);
    SetWindowTextA(g_infoLabel, infoBuf);
}

/*
 * WndProc
 * -------
 * The main Message Processing Loop for the window.
 * Windows OS sends "messages" here (like "user clicked", "time to paint", "destroy window").
 */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    // WM_COMMAND: Received when a control (button/combo) is interacted with
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int notif = HIWORD(wParam);

        // Reset Button Clicked
        if (id == ID_RESET_BTN && notif == BN_CLICKED) {
            g_solver.reset(); // Reset logic
            // Reset all dropdowns to index 0 ("Undug")
            for (int i = 0; i < TOTAL_CELLS; i++) {
                SendMessage(g_combos[i], CB_SETCURSEL, 0, 0);
            }
            RecalcAndUpdate(hWnd);
            return 0;
        }

        // Combo Box Changed
        if (id >= ID_COMBO_BASE && id < ID_COMBO_BASE + TOTAL_CELLS && notif == CBN_SELCHANGE) {
            int cellIdx = id - ID_COMBO_BASE;
            // Get selected index from dropdown
            int sel = (int)SendMessage(g_combos[cellIdx], CB_GETCURSEL, 0, 0);
            CellContent content = static_cast<CellContent>(sel);
            
            // Update the solver grid
            g_solver.setCell(cellIdx / COLS, cellIdx % COLS, content);
            
            // Recalculate probabilities
            RecalcAndUpdate(hWnd);
            return 0;
        }
        break;
    }

    // WM_CTLCOLORSTATIC: Sent before a static control (label) is drawn.
    // Allows us to customize the background and text color.
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;

        // Check if the control being drawn is one of our grid cells
        for (int i = 0; i < TOTAL_CELLS; i++) {
            if (hCtl == g_probLabels[i] || hCtl == g_cellPanels[i]) {
                COLORREF bg = getCellBgColor(i);
                SetBkColor(hdc, bg); // Set text background

                // Calculate brightness to decide if text should be black or white
                // Standard luma formula: Y = 0.299R + 0.587G + 0.114B
                int brightness = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
                SetTextColor(hdc, brightness > 128 ? RGB(30, 30, 30) : RGB(240, 240, 240));

                if (g_cellBrushes[i]) return (LRESULT)g_cellBrushes[i]; // Return background brush
                break;
            }
        }

        // Custom styling for the Info Label and Title (Dark bg, Light text)
        if (hCtl == g_infoLabel || hCtl == g_titleLabel) {
            SetBkColor(hdc, colorBg);
            SetTextColor(hdc, RGB(220, 220, 220));
            static HBRUSH bgBrush = CreateSolidBrush(colorBg);
            return (LRESULT)bgBrush;
        }

        break;
    }

    // WM_ERASEBKGND: Called to erase the window background before painting.
    // We use this to draw the main dark background and the grid lines.
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        
        // Fill entire window with background color
        HBRUSH brush = CreateSolidBrush(colorBg);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        // Draw the grid container border
        HPEN pen = CreatePen(PS_SOLID, GRID_BORDER, RGB(60, 60, 80));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);

        int gridLeft = SIDE_MARGIN;
        int gridTop = TOP_MARGIN;
        int gridRight = gridLeft + COLS * (CELL_W + GRID_PAD) + GRID_PAD;
        int gridBottom = gridTop + ROWS * (CELL_H + GRID_PAD) + GRID_PAD;

        // Draw rectangle behind the grid cells
        HBRUSH gridBgBrush = CreateSolidBrush(RGB(50, 50, 65));
        RECT gridRect = {gridLeft, gridTop, gridRight, gridBottom};
        FillRect(hdc, &gridRect, gridBgBrush);
        DeleteObject(gridBgBrush);

        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        return 1; // Signal that we handled the erasing
    }

    // WM_DESTROY: Called when the window is closing.
    // Important to clean up resources (brushes, fonts) to avoid memory leaks.
    case WM_DESTROY:
        for (int i = 0; i < TOTAL_CELLS; i++) {
            if (g_cellBrushes[i]) DeleteObject(g_cellBrushes[i]);
        }
        if (g_fontNormal) DeleteObject(g_fontNormal);
        if (g_fontBold) DeleteObject(g_fontBold);
        if (g_fontTitle) DeleteObject(g_fontTitle);
        if (g_fontSmall) DeleteObject(g_fontSmall);
        PostQuitMessage(0); // Tell the message loop to stop
        return 0;

    default:
        break;
    }
    // Default processing for messages we don't handle
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/*
 * CreateFonts
 * -----------
 * Helper to initialize the font objects used in the UI.
 * Creates standard "Segoe UI" fonts of various sizes/weights.
 */
static void CreateFonts() {
    g_fontNormal = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    g_fontBold = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    g_fontTitle = CreateFontA(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    g_fontSmall = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

/*
 * WinMain
 * -------
 * The application entry point.
 * 1. Initializes Common Controls.
 * 2. Registers the Window Class.
 * 3. Creates the main Window.
 * 4. Creates all child controls (buttons, labels, combos) dynamically in loops.
 * 5. Runs the Message Loop.
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;

    // Initialize visual styles
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW; // Redraw on resize
    wc.lpfnWndProc = WndProc;           // Our message handler
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // We handle painting ourselves in WM_ERASEBKGND
    wc.lpszClassName = "ThrillDiggerCalc";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExA(&wc);

    CreateFonts();

    // Calculate window size based on grid dimensions + margins
    int gridW = COLS * (CELL_W + GRID_PAD) + GRID_PAD;
    int gridH = ROWS * (CELL_H + GRID_PAD) + GRID_PAD;
    int winW = gridW + 2 * SIDE_MARGIN;
    int winH = TOP_MARGIN + gridH + BOTTOM_MARGIN;

    // Adjust window size to account for title bar and borders
    RECT adjRect = {0, 0, winW, winH};
    AdjustWindowRect(&adjRect, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
    int adjW = adjRect.right - adjRect.left;
    int adjH = adjRect.bottom - adjRect.top;

    // Create the main window
    HWND hWnd = CreateWindowExA(0, "ThrillDiggerCalc",
        "Thrill Digger Calculator - Expert Mode",
        (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX), // Fixed size window
        CW_USEDEFAULT, CW_USEDEFAULT, adjW, adjH,
        NULL, NULL, hInstance, NULL);

    // Create Title Label
    g_titleLabel = CreateWindowExA(0, "STATIC",
        "Thrill Digger Calculator - Expert Mode (8 Bombs + 8 Rupoors)",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 8, winW, 28, hWnd, NULL, hInstance, NULL);
    SendMessage(g_titleLabel, WM_SETFONT, (WPARAM)g_fontTitle, TRUE);

    // -------------------------------------------------------------------------
    // Create Grid Cells Loop
    // -------------------------------------------------------------------------
    int gridLeft = SIDE_MARGIN;
    int gridTop = TOP_MARGIN;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int idx = r * COLS + c;
            // Calculate pixel position for this cell
            int x = gridLeft + GRID_PAD + c * (CELL_W + GRID_PAD);
            int y = gridTop + GRID_PAD + r * (CELL_H + GRID_PAD);

            // 1. Cell background panel (Static control)
            g_cellPanels[idx] = CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                x, y, CELL_W, CELL_H,
                hWnd, NULL, hInstance, NULL);

            // 2. Probability label (Static control, placed on top)
            g_probLabels[idx] = CreateWindowExA(0, "STATIC", "40% Bad",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                x, y + 2, CELL_W, 18,
                hWnd, NULL, hInstance, NULL);
            SendMessage(g_probLabels[idx], WM_SETFONT, (WPARAM)g_fontBold, TRUE);

            // 3. Combo box (Dropdown list)
            g_combos[idx] = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                x + 4, y + 22, CELL_W - 8, 300,
                hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COMBO_BASE + idx)), hInstance, NULL);
            SendMessage(g_combos[idx], WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            // Add options to the dropdown
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Undug");
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Green rupee");
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Blue rupee");
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Red rupee");
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Silver rupee");
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Gold rupee");
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Rupoor");
            SendMessageA(g_combos[idx], CB_ADDSTRING, 0, (LPARAM)"Bomb");
            
            // Set default selection to "Undug" (index 0)
            SendMessage(g_combos[idx], CB_SETCURSEL, 0, 0);

            // Initialize brush
            g_cellBrushes[idx] = CreateSolidBrush(colorUndug);
        }
    }

    // Create Info Label at the bottom
    int infoY = gridTop + gridH + 8;
    g_infoLabel = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, infoY, winW - 120, 24, hWnd, NULL, hInstance, NULL);
    SendMessage(g_infoLabel, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

    // Create Reset button
    g_resetBtn = CreateWindowExA(0, "BUTTON", "Reset",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        winW - 110, infoY, 90, 28,
        hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_RESET_BTN)), hInstance, NULL);
    SendMessage(g_resetBtn, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

    // Initial calculation (start state)
    g_solver.solve();
    UpdateUI(hWnd);

    // Show the window
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Main Message Loop
    // Keeps the application running until PostQuitMessage is called.
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); // Translate keyboard messages
        DispatchMessage(&msg);  // Send message to WndProc
    }

    return (int)msg.wParam;
}
