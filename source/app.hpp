#pragma once
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/statvfs.h>
#include <map>
#include <algorithm>

using json = nlohmann::json;
using namespace std;

// CONFIGURATION
const char* const SERVER_URL = "http://31.37.87.78:3000"; 
const char* const FONT_PATH = "sdmc:/switch/font.ttf";

struct AppConfig {
    bool musicMuted = false;
    bool musicPaused = false;
    bool darkMode = false;
    string lang = "fr";
};
extern AppConfig globalConfig;

struct User {
    string deviceId;
    int points;
    bool loggedIn;
    bool isBanned;
};
extern User currentUser;

extern SDL_Color C_BG, C_HEADER, C_TEXT, C_ACCENT, C_CARD_BG, C_TEXT_DIM, C_BTN_TEXT, C_NEW_TAG;

struct NewsItem { string title; string content; };

// STRUCTURE CORRIGEE (Une seule fois)
struct HomebrewApp {
    string name; string url; string category; string description; string patchNotes;
    int cost; SDL_Color color; char iconLetter; bool isNew; int downloads;
    float rating; // Note moyenne
    int voteCount; // Nombre de votes (pour faire plus pro)
};

// --- FILE D'ATTENTE ---
extern vector<HomebrewApp> downloadQueue;

struct Achievement { string title; bool unlocked; };
extern vector<Achievement> myAchievements;

struct ProgressData { 
    curl_off_t last_bytes_drawn; 
    time_t last_time; 
    curl_off_t last_dlnow;
};

struct LeaderboardEntry { string id; int points; };
struct Particle { float x, y, vx, vy; int life; SDL_Color color; };

enum AppState { 
    STATE_DASHBOARD, 
    STATE_DETAILS, 
    STATE_SETTINGS, 
    STATE_NEWS_DETAILS, 
    STATE_EXTRAS, 
    STATE_LEADERBOARD, 
    STATE_REDEEM,
    STATE_QUEUE 
};

extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern TTF_Font *fontLarge, *fontMedium, *fontSmall;
extern PadState pad;

// GRAPHICS & AUDIO
bool initGraphics();
void cleanupGraphics();
void updateTheme();
void drawRoundedBox(int x, int y, int w, int h, int radius, SDL_Color c);
void drawText(string text, int x, int y, TTF_Font* font, SDL_Color c, bool centered = false, int wrapWidth = 0);
void drawSystemBar();
void drawFooter(string text);
int progress_callback(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
bool isPointInRect(int tx, int ty, int x, int y, int w, int h);
void spawnParticles(int x, int y, int count, SDL_Color c);
void updateAndDrawParticles();
void drawWheel();
extern bool showWheel;
extern bool wheelSpinning;
extern float wheelSpeed;
void playSfx();
void playMusic(string path);
void toggleMute();
void togglePause();
SDL_Color generateColor(string name);
string TR(string key);
string getFreeSpace();
string formatSpeed(double bytes_per_sec);
string openKeyboard();

// NETWORK & INSTALL
string getDeviceId();
void loginUser();
bool buyApp(HomebrewApp app);
void sendClickPoint();
void sendRedeem(string code);
void sendRating(string appName, int score); // Prototype ajouté ici
vector<LeaderboardEntry> fetchLeaderboard();
string fetchJson(string endpoint);
void downloadFile(const char* url, const char* path);
bool installNSP(HomebrewApp app); 

// SETTINGS
void drawSettingsMenu();
void handleSettingsInput(bool &running);