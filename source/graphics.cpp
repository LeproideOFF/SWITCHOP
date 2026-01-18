#include "app.hpp"

SDL_Color C_BG, C_HEADER, C_TEXT, C_TEXT_DIM, C_ACCENT, C_CARD_BG, C_BTN_TEXT, C_NEW_TAG;
Mix_Chunk* sfxClick = nullptr; Mix_Music* bgMusic = nullptr;
AppConfig globalConfig; 

map<string, string> dictFR = {
    {"WELCOME", "Connexion..."}, {"PTS", "Pts"}, {"FREE", "Gratuit"}, {"DL_FAST", "INSTALLATION..."},
    {"NEW", "NOUVEAU"}, {"LDR_BOARD", "CLASSEMENT"}, {"REDEEM", "CODE CADEAU"}, {"WHEEL_SPIN", "ROUE !"}
};
map<string, string> dictEN = {
    {"WELCOME", "Connecting..."}, {"PTS", "Pts"}, {"FREE", "Free"}, {"DL_FAST", "INSTALLING..."},
    {"NEW", "NEW"}, {"LDR_BOARD", "LEADERBOARD"}, {"REDEEM", "REDEEM"}, {"WHEEL_SPIN", "SPIN!"}
};

string TR(string key) {
    if (globalConfig.lang == "en") return dictEN.count(key) ? dictEN[key] : key;
    return dictFR.count(key) ? dictFR[key] : key;
}

string openKeyboard() {
    SwkbdConfig swkbd;
    static char out_text[51];
    Result rc = swkbdCreate(&swkbd, 0);
    if (R_SUCCEEDED(rc)) {
        swkbdConfigSetGuideText(&swkbd, "Rechercher une application...");
        swkbdConfigSetOkButtonText(&swkbd, "Chercher");
        rc = swkbdShow(&swkbd, out_text, sizeof(out_text));
        swkbdClose(&swkbd);
        if (R_SUCCEEDED(rc)) return string(out_text);
    }
    return "";
}

void updateTheme() {
    if (globalConfig.darkMode) {
        C_BG={20,20,25,255}; C_HEADER={35,35,40,255}; C_TEXT={240,240,240,255};
        C_TEXT_DIM={160,160,160,255}; C_ACCENT={0,160,255,255}; C_CARD_BG={45,45,50,255}; C_BTN_TEXT={0,0,0,255}; C_NEW_TAG={255, 50, 50, 255};
    } else {
        C_BG={240,242,245,255}; C_HEADER={255,255,255,255}; C_TEXT={30,30,35,255};
        C_TEXT_DIM={120,120,120,255}; C_ACCENT={0,122,255,255}; C_CARD_BG={255,255,255,255}; C_BTN_TEXT={255,255,255,255}; C_NEW_TAG={255, 80, 80, 255};
    }
}

bool initGraphics() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO); TTF_Init(); Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    sfxClick = Mix_LoadWAV("sdmc:/switch/click.wav");
    window = SDL_CreateWindow("Shop", 0, 0, 1280, 720, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    fontLarge = TTF_OpenFont(FONT_PATH, 36); fontMedium = TTF_OpenFont(FONT_PATH, 24); fontSmall = TTF_OpenFont(FONT_PATH, 18);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    updateTheme(); return true;
}

void cleanupGraphics() {
    if(sfxClick) Mix_FreeChunk(sfxClick);
    if(bgMusic) Mix_FreeMusic(bgMusic);
    Mix_CloseAudio(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit();
}

void toggleMute() { globalConfig.musicMuted = !globalConfig.musicMuted; Mix_VolumeMusic(globalConfig.musicMuted ? 0 : 128); }
void togglePause() { globalConfig.musicPaused = !globalConfig.musicPaused; if (globalConfig.musicPaused) Mix_PauseMusic(); else Mix_ResumeMusic(); }

vector<Particle> particles;
void spawnParticles(int x, int y, int count, SDL_Color c) {
    for(int i=0; i<count; i++) particles.push_back({(float)x, (float)y, (float)(rand()%10-5), (float)(rand()%10-5), 60, c});
}
void updateAndDrawParticles() {
    for(auto it = particles.begin(); it != particles.end();) {
        it->x += it->vx; it->y += it->vy; it->life--;
        if(it->life <= 0) it = particles.erase(it);
        else {
            SDL_Color c = it->color; c.a = (Uint8)((it->life/60.0f)*255);
            boxRGBA(renderer, (int)it->x, (int)it->y, (int)it->x+4, (int)it->y+4, c.r, c.g, c.b, c.a);
            ++it;
        }
    }
}

bool showWheel = false; float wheelAngle = 0; float wheelSpeed = 0; bool wheelSpinning = false;
void drawWheel() {
    if (!showWheel) return;
    drawRoundedBox(0, 0, 1280, 720, 0, {0,0,0,200});
    int cx = 640, cy = 360, r = 200;
    filledCircleRGBA(renderer, cx, cy, r, 255, 200, 0, 255);
    filledTrigonRGBA(renderer, cx, cy-r-20, cx-20, cy-r+20, cx+20, cy-r+20, 255, 0, 0, 255);
    if (wheelSpinning) {
        wheelAngle += wheelSpeed; wheelSpeed *= 0.98;
        if (wheelSpeed < 0.5) {
            wheelSpinning = false; showWheel = false;
            currentUser.points += (rand() % 5 + 1) * 50;
            spawnParticles(cx, cy, 50, {255, 255, 0, 255});
        }
    }
}

string getFreeSpace() {
    struct statvfs info;
    if (statvfs("sdmc:/", &info) == 0) {
        double gb = (double)(info.f_bavail * info.f_frsize) / 1024 / 1024 / 1024;
        char buf[16]; sprintf(buf, "SD: %.1fGB", gb); return string(buf);
    }
    return "";
}

string formatSpeed(double bytes_per_sec) {
    if (bytes_per_sec > 1024*1024) { char b[32]; sprintf(b, "%.1f MB/s", bytes_per_sec/1024/1024); return string(b); }
    return to_string((int)bytes_per_sec/1024) + " KB/s";
}

int progress_callback(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    ProgressData *data = (ProgressData *)p;
    time_t now = time(NULL);
    if (now == data->last_time && dlnow != dltotal) return 0;
    double speed = (dlnow - data->last_dlnow) / (double)(now - data->last_time + 1);
    data->last_time = now; data->last_dlnow = dlnow;
    padUpdate(&pad); if (padGetButtonsDown(&pad) & HidNpadButton_B) return 1;
    drawRoundedBox(0, 0, 1280, 720, 0, {0, 0, 0, 220});
    drawRoundedBox(340, 235, 600, 250, 20, C_CARD_BG);
    drawText(TR("DL_FAST"), 640, 275, fontMedium, C_TEXT, true);
    drawText(formatSpeed(speed), 640, 345, fontSmall, C_ACCENT, true);
    float percent = (dltotal > 0) ? (float)dlnow / (float)dltotal : 0.0f;
    drawRoundedBox(390, 385, 500, 20, 10, {200, 200, 200, 255});
    if (percent > 0) drawRoundedBox(390, 385, (int)(500 * percent), 20, 10, C_ACCENT);
    SDL_RenderPresent(renderer); return 0;
}

void drawRoundedBox(int x, int y, int w, int h, int radius, SDL_Color c) { roundedBoxRGBA(renderer, x, y, x + w, y + h, radius, c.r, c.g, c.b, c.a); }

void drawText(string text, int x, int y, TTF_Font* font, SDL_Color c, bool centered, int wrapWidth) {
    if(text.empty() || !font) return;
    SDL_Surface* surf = (wrapWidth > 0) ? TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), c, wrapWidth) : TTF_RenderUTF8_Blended(font, text.c_str(), c);
    if(surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst = {x, y, surf->w, surf->h};
        if(centered) { dst.x -= surf->w/2; dst.y -= surf->h/2; }
        SDL_RenderCopy(renderer, tex, NULL, &dst); SDL_DestroyTexture(tex); SDL_FreeSurface(surf);
    }
}

void drawSystemBar() {
    drawRoundedBox(0, 0, 1280, 60, 0, C_HEADER);
    drawText("SWITCHOP", 40, 15, fontMedium, C_TEXT);
    drawText(to_string(currentUser.points) + " " + TR("PTS"), 950, 15, fontMedium, C_ACCENT);
    drawText(getFreeSpace(), 1100, 15, fontMedium, C_TEXT_DIM);
}

void drawFooter(string text) { drawRoundedBox(0, 660, 1280, 60, 0, C_HEADER); drawText(text, 1240, 675, fontSmall, C_TEXT_DIM, false); }

SDL_Color generateColor(string name) { unsigned int h=0; for(char c:name) h=c+(h<<6)+(h<<16)-h; return {(Uint8)((h&0xFF0000)>>16),(Uint8)((h&0x00FF00)>>8),(Uint8)(h&0x0000FF),255}; }

bool isPointInRect(int tx, int ty, int x, int y, int w, int h) { return (tx >= x && tx <= x + w && ty >= y && ty <= y + h); }

void playSfx() { if(!globalConfig.musicMuted && sfxClick) Mix_PlayChannel(-1, sfxClick, 0); }

void playMusic(string path) {
    if (Mix_PlayingMusic()) Mix_HaltMusic();
    if (bgMusic) Mix_FreeMusic(bgMusic);
    bgMusic = Mix_LoadMUS(path.c_str());
    if (bgMusic) Mix_PlayMusic(bgMusic, -1);
}