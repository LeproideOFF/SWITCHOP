#include "app.hpp"



SDL_Window* window = nullptr;

SDL_Renderer* renderer = nullptr;

TTF_Font *fontLarge, *fontMedium, *fontSmall;

PadState pad;

User currentUser;

AppState currentState = STATE_DASHBOARD;



vector<string> categories = {"TOUT", "JEU", "APP"};

vector<string> catDisplay = {"TOUT", "JEUX", "APPS"};

int currentCatIndex = 0;

int scrollOffset = 0;

int newsScrollOffset = 0; // Scroll dédié aux actus

string searchQuery = "";

string broadcastMsg = "Chargement des infos...";



extern string loginMessage;

extern void sendClickPoint();



// --- AJOUT : File d'attente ---

vector<HomebrewApp> downloadQueue;



NewsItem currentNews;



string getFileNameFromUrl(string url) {

size_t splitLoc = url.find_last_of('/');

return url.substr(splitLoc + 1);

}



// Utilitaire pour la recherche sans tenir compte des majuscules

bool containsSearch(string haystack, string needle) {

if (needle.empty()) return true;

auto it = search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),

[](char ch1, char ch2) { return toupper(ch1) == toupper(ch2); });

return it != haystack.end();

}



int main(int argc, char **argv) {

socketInitializeDefault();

padConfigureInput(1, HidNpadStyleSet_NpadStandard);

padInitializeDefault(&pad);

psmInitialize();

initGraphics();


// LOGIN

drawRoundedBox(0, 0, 1280, 720, 0, C_BG);

drawText("Connexion...", 640, 360, fontMedium, C_TEXT, true);

SDL_RenderPresent(renderer);

loginUser();


// POPUP BONUS

if (!loginMessage.empty()) {

for(int i=0; i<120; i++) {

drawRoundedBox(0, 0, 1280, 720, 0, C_BG);

drawRoundedBox(340, 260, 600, 200, 20, C_CARD_BG);

drawRoundedBox(340, 260, 600, 60, 20, C_ACCENT);

drawText("NOTIFICATION", 640, 275, fontMedium, C_BTN_TEXT, true);

drawText(loginMessage, 640, 360, fontMedium, C_TEXT, true);

SDL_RenderPresent(renderer);

}

}



// DATA

string jsonStr = fetchJson("/shop/data");

vector<HomebrewApp> allApps;

vector<NewsItem> newsList;



if(!jsonStr.empty()) {

try {

auto j = json::parse(jsonStr);

if(j.contains("broadcast")) broadcastMsg = j["broadcast"];

if (j.contains("music_url")) {

string musicUrl = j["music_url"];

string localPath = "sdmc:/switch/" + getFileNameFromUrl(musicUrl);

if(access(localPath.c_str(), F_OK) == -1) downloadFile(musicUrl.c_str(), localPath.c_str());

playMusic(localPath);

}

for(auto& i : j["apps"]) {

HomebrewApp a;

a.name = i["name"]; a.url = i["url"]; a.category = i["category"];

a.cost = i["cost"]; a.description = i.contains("desc") ? (string)i["desc"] : "";

a.patchNotes = i.contains("patch_notes") ? (string)i["patch_notes"] : "";

a.color = generateColor(a.name); a.iconLetter = a.name[0];

allApps.push_back(a);

}

for(auto& n : j["news"]) newsList.push_back({n["title"], n["content"]});

} catch(...) {}

}



int cursor = 0;

HomebrewApp selectedApp;

bool running = true;

bool pointAnimation = false;

int animTimer = 0;



// --- AJOUT : Optimisation Filtrage ---

vector<HomebrewApp> displayApps = allApps;

bool needFilter = true;



while(running && appletMainLoop()) {

padUpdate(&pad);

u64 kDown = padGetButtonsDown(&pad);


int touchX = -1, touchY = -1;

bool touched = false;

SDL_Event event;

while (SDL_PollEvent(&event)) {

if (event.type == SDL_FINGERDOWN) {

touchX = event.tfinger.x * 1280; touchY = event.tfinger.y * 720; touched = true;

} else if (event.type == SDL_MOUSEBUTTONDOWN) {

touchX = event.button.x; touchY = event.button.y; touched = true;

}

// Défilement tactile des news si on touche la partie gauche

if (event.type == SDL_FINGERMOTION && event.tfinger.x * 1280 < 380) {

newsScrollOffset -= event.tfinger.dy * 720;

if(newsScrollOffset < 0) newsScrollOffset = 0;

}

}



if (kDown & HidNpadButton_X) {

searchQuery = openKeyboard();

cursor = 0; scrollOffset = 0; needFilter = true;

}



if (kDown & HidNpadButton_Plus) currentState = STATE_SETTINGS;

// --- AJOUT : Bouton file d'attente ---

if (kDown & HidNpadButton_Minus) currentState = STATE_QUEUE;



// --- AJOUT : Logique de filtrage ---

if (needFilter) {

displayApps.clear();

for(auto &a : allApps) {

bool matchesCat = (currentCatIndex == 0 || a.category == categories[currentCatIndex]);

bool matchesSearch = (searchQuery == "" || containsSearch(a.name, searchQuery));

if (matchesCat && matchesSearch) displayApps.push_back(a);

}

needFilter = false;

}



// RENDU FOND

if (currentState != STATE_SETTINGS) {

SDL_SetRenderDrawColor(renderer, C_BG.r, C_BG.g, C_BG.b, 255);

SDL_RenderClear(renderer);

drawSystemBar();

// BROADCAST FIXE

drawText(broadcastMsg, 640, 30, fontSmall, C_ACCENT, true);

}



if (currentState == STATE_SETTINGS) {

handleSettingsInput(running);

if (kDown & HidNpadButton_B) currentState = STATE_DASHBOARD;

extern int settingsCursor;

if ((kDown & HidNpadButton_A) && settingsCursor == 3) currentState = STATE_DASHBOARD;

drawSettingsMenu();

drawFooter("[A] Valider [B] Retour");

}


else if (currentState == STATE_DASHBOARD) {

// 1. INTERFACE CLICKER

int farmX = 1000, farmY = 70, farmW = 240, farmH = 80;

drawRoundedBox(farmX, farmY, farmW, farmH, 20, C_CARD_BG);

drawText("CLIQUER ICI", farmX + 120, farmY + 20, fontSmall, C_TEXT, true);

drawText("+1 POINT", farmX + 120, farmY + 50, fontMedium, C_ACCENT, true);



if (touched && isPointInRect(touchX, touchY, farmX, farmY, farmW, farmH)) {

currentUser.points += 1;

pointAnimation = true;

animTimer = 10;

playSfx();

sendClickPoint();

}



if (pointAnimation && animTimer > 0) {

drawText("+1", farmX + 120, farmY - 20 + (10 - animTimer), fontLarge, C_ACCENT, true);

animTimer--;

if(animTimer <= 0) pointAnimation = false;

}



// 2. ONGLETS CATEGORIES

int tabStartX = 400, tabY = 80;

for(int i=0; i<(int)catDisplay.size(); i++) {

int tabX = tabStartX + i * 150;

bool isCatSel = (i == currentCatIndex);

drawRoundedBox(tabX, tabY, 140, 40, 20, isCatSel ? C_ACCENT : C_CARD_BG);

drawText(catDisplay[i], tabX + 70, tabY + 10, fontSmall, isCatSel ? C_BTN_TEXT : C_TEXT_DIM, true);

if (touched && isPointInRect(touchX, touchY, tabX, tabY, 140, 40)) {

currentCatIndex = i; cursor = 0; scrollOffset = 0; playSfx(); needFilter = true;

}

}

if (touched && isPointInRect(touchX, touchY, 880, 0, 150, 60)) { currentState = STATE_SETTINGS; playSfx(); }



// 3. NEWS avec DÉFILEMENT

drawText("ACTUALITÉS", 40, 100, fontMedium, C_ACCENT);

for(int i=0; i<(int)newsList.size(); i++) {

int y = 140 + i*150 - newsScrollOffset;

if (y < 60 || y > 650) continue;



drawRoundedBox(40, y, 300, 130, 15, C_CARD_BG);

drawText(newsList[i].title, 60, y+15, fontMedium, C_TEXT);

string shortContent = newsList[i].content;

if(shortContent.length() > 60) shortContent = shortContent.substr(0, 57) + "...";

drawText(shortContent, 60, y+50, fontSmall, C_TEXT_DIM, false, 260);

drawText("Lire la suite >", 200, y+100, fontSmall, C_ACCENT);



if (touched && isPointInRect(touchX, touchY, 40, y, 300, 130)) {

currentNews = newsList[i];

currentState = STATE_NEWS_DETAILS;

playSfx();

}

}



// 4. GRILLE APPS

int startX = 400, COLS = 2, startY = 170;

if (displayApps.empty()) drawText("Aucune app.", 800, 360, fontMedium, C_TEXT_DIM, true);



for(int i=0; i<(int)displayApps.size(); i++) {

int r = i / COLS, c = i % COLS;

int x = startX + c * 420;

int y = startY + r * 180 - scrollOffset;



if (y < 120 || y > 720) continue;



bool isSel = (i == cursor);

if (isSel) drawRoundedBox(x-5, y-5, 410, 170, 20, C_ACCENT);

drawRoundedBox(x, y, 400, 160, 20, C_CARD_BG);

drawRoundedBox(x+20, y+20, 80, 80, 20, displayApps[i].color);


string letter(1, displayApps[i].iconLetter);

drawText(letter, x+50, y+40, fontLarge, C_BTN_TEXT);

drawText(displayApps[i].name, x+120, y+20, fontMedium, C_TEXT);

drawText(displayApps[i].category, x+120, y+60, fontSmall, C_TEXT_DIM);

string price = (displayApps[i].cost == 0) ? "GRATUIT" : to_string(displayApps[i].cost) + " PTS";

drawRoundedBox(x+120, y+100, 120, 30, 10, C_BG);

drawText(price, x+130, y+105, fontSmall, C_ACCENT);



if (touched && isPointInRect(touchX, touchY, x, y, 400, 160)) {

cursor = i; selectedApp = displayApps[cursor]; currentState = STATE_DETAILS; playSfx();

}

}



if(!displayApps.empty()) {

if (kDown & HidNpadButton_Right) { cursor++; playSfx(); }

if (kDown & HidNpadButton_Left) { cursor--; playSfx(); }

if (kDown & HidNpadButton_Down) { cursor += 2; scrollOffset += 50; newsScrollOffset += 30; playSfx(); }

if (kDown & HidNpadButton_Up) { cursor -= 2; scrollOffset -= 50; newsScrollOffset -= 30; playSfx(); }


if(cursor >= (int)displayApps.size()) cursor = displayApps.size() - 1;

if(cursor < 0) cursor = 0;

if(scrollOffset < 0) scrollOffset = 0;

if(newsScrollOffset < 0) newsScrollOffset = 0;



if (kDown & HidNpadButton_A) { selectedApp = displayApps[cursor]; currentState = STATE_DETAILS; playSfx(); }

}



// FOOTER CENTRÉ

drawFooter("[A] Voir [X] Chercher [-] File [+] Parametres");

}


else if (currentState == STATE_QUEUE) {

drawRoundedBox(200, 100, 880, 500, 30, C_CARD_BG);

drawText("FILE D'ATTENTE", 640, 150, fontLarge, C_ACCENT, true);

if (downloadQueue.empty()) drawText("Vide", 640, 300, fontMedium, C_TEXT_DIM, true);

else {

for(int i=0; i<(int)downloadQueue.size(); i++) drawText(to_string(i+1) + ". " + downloadQueue[i].name, 250, 220 + i*40, fontMedium, C_TEXT);

int bX = 440, bY = 520, bW = 400, bH = 60;

drawRoundedBox(bX, bY, bW, bH, 30, C_ACCENT);

drawText("TOUT TÉLÉCHARGER", 640, 535, fontMedium, C_BTN_TEXT, true);

if (touched && isPointInRect(touchX, touchY, bX, bY, bW, bH)) {

for(auto& app : downloadQueue) downloadFile(app.url.c_str(), ("sdmc:/switch/"+app.name+".nsp").c_str());

downloadQueue.clear();

}

}

if (kDown & HidNpadButton_B) currentState = STATE_DASHBOARD;

drawFooter("[B] Retour");

}



else if (currentState == STATE_NEWS_DETAILS) {

int retX = 20, retY = 80, retW = 150, retH = 50;

drawRoundedBox(retX, retY, retW, retH, 15, C_CARD_BG);

drawText("< Retour", retX + 75, retY + 25, fontMedium, C_TEXT, true);

if ((touched && isPointInRect(touchX, touchY, retX, retY, retW, retH)) || (kDown & HidNpadButton_B)) { currentState = STATE_DASHBOARD; playSfx(); }



drawRoundedBox(200, 100, 880, 500, 30, C_CARD_BG);

drawText(currentNews.title, 640, 150, fontLarge, C_ACCENT, true);

drawText(currentNews.content, 640, 250, fontMedium, C_TEXT, true, 800);

drawFooter("[B] Retour");

}


else if (currentState == STATE_DETAILS) {

int retX = 20, retY = 80, retW = 150, retH = 50;

drawRoundedBox(retX, retY, retW, retH, 15, C_CARD_BG);

drawText("< Retour", retX + 75, retY + 25, fontMedium, C_TEXT, true);

if ((touched && isPointInRect(touchX, touchY, retX, retY, retW, retH)) || (kDown & HidNpadButton_B)) { currentState = STATE_DASHBOARD; playSfx(); }


drawRoundedBox(200, 100, 880, 500, 30, C_CARD_BG);

drawRoundedBox(240, 140, 100, 100, 25, selectedApp.color);

drawText(selectedApp.name, 360, 150, fontLarge, C_TEXT);

drawText("DESCRIPTION", 240, 280, fontMedium, C_ACCENT);

drawText(selectedApp.description, 240, 310, fontSmall, C_TEXT, false, 800);



int btnX = 440, btnY = 530, btnW = 400, btnH = 60;

drawRoundedBox(btnX, btnY, btnW, btnH, 30, C_ACCENT);

drawText("AJOUTER À LA FILE", 640, 545, fontMedium, C_BTN_TEXT, true);



if ((kDown & HidNpadButton_A) || (touched && isPointInRect(touchX, touchY, btnX, btnY, btnW, btnH))) {

downloadQueue.push_back(selectedApp);

currentState = STATE_QUEUE;

}

drawFooter("[A] Ajouter [B] Retour");

}



SDL_RenderPresent(renderer);

SDL_Delay(16); // FIX LAG

}



cleanupGraphics();

psmExit();

socketExit();

return 0;

}