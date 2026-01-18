#include "app.hpp"

const vector<string> SETTINGS_OPTIONS = {
    "Langue: FR/EN",
    "Mode Sombre",
    "Couper le son",
    "Pause Musique",
    "Retour"
};

int settingsCursor = 0;

void drawSettingsMenu() {
    drawRoundedBox(340, 160, 600, 450, 20, C_CARD_BG);
    drawText("PARAMETRES", 640, 180, fontLarge, C_TEXT, true);

    for (int i = 0; i < (int)SETTINGS_OPTIONS.size(); i++) {
        bool isSel = (i == settingsCursor);
        int y = 250 + i * 70;
        
        string label = SETTINGS_OPTIONS[i];
        if (i == 0) label = "Langue: " + globalConfig.lang;
        if (i == 1) label = globalConfig.darkMode ? "Mode Sombre: ON" : "Mode Sombre: OFF";
        if (i == 2) label = globalConfig.musicMuted ? "Son: MUET" : "Son: ACTIF";
        if (i == 3) label = globalConfig.musicPaused ? "Musique: PAUSE" : "Musique: PLAY";

        drawRoundedBox(400, y, 480, 50, 15, isSel ? C_ACCENT : C_BG);
        drawText(label, 640, y + 12, fontMedium, isSel ? C_BTN_TEXT : C_TEXT, true);
    }
}

void handleSettingsInput(bool &running) {
    u64 kDown = padGetButtonsDown(&pad);
    if (kDown & HidNpadButton_Down) { settingsCursor++; if (settingsCursor >= 5) settingsCursor = 0; playSfx(); }
    if (kDown & HidNpadButton_Up) { settingsCursor--; if (settingsCursor < 0) settingsCursor = 4; playSfx(); }

    if (kDown & HidNpadButton_A) {
        if (settingsCursor == 0) { 
            globalConfig.lang = (globalConfig.lang == "fr") ? "en" : "fr"; 
        }
        else if (settingsCursor == 1) { 
            globalConfig.darkMode = !globalConfig.darkMode; updateTheme(); 
        }
        else if (settingsCursor == 2) { 
            toggleMute(); // Appelle la fonction dans graphics.cpp
        }
        else if (settingsCursor == 3) { 
            togglePause(); // Appelle la fonction dans graphics.cpp
        }
        playSfx();
    }
}