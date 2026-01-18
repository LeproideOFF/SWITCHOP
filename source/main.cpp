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
int newsScrollOffset = 0;
string searchQuery = "";
string broadcastMsg = "Chargement des infos...";

extern string loginMessage;
extern void sendClickPoint();

vector<HomebrewApp> downloadQueue;
vector<DailyMission> dailyMissions;
vector<DownloadHistory> downloadHistory;
SortMode currentSort = SORT_NAME;
FilterMode currentFilter = FILTER_ALL;

NewsItem currentNews;
int ratingScore = 0;

string getFileNameFromUrl(string url) {
    size_t splitLoc = url.find_last_of('/');
    return url.substr(splitLoc + 1);
}

bool containsSearch(string haystack, string needle) {
    if (needle.empty()) return true;
    auto it = search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
        [](char ch1, char ch2) { return toupper(ch1) == toupper(ch2); });
    return it != haystack.end();
}

string getCurrentDate() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[20];
    sprintf(buffer, "%02d/%02d/%04d", ltm->tm_mday, 1 + ltm->tm_mon, 1900 + ltm->tm_year);
    return string(buffer);
}

void sortApps(vector<HomebrewApp>& apps) {
    switch(currentSort) {
        case SORT_PRICE:
            sort(apps.begin(), apps.end(), [](const HomebrewApp& a, const HomebrewApp& b) { 
                return a.cost < b.cost; 
            });
            break;
        case SORT_RATING:
            sort(apps.begin(), apps.end(), [](const HomebrewApp& a, const HomebrewApp& b) { 
                return a.rating > b.rating; 
            });
            break;
        case SORT_DOWNLOADS:
            sort(apps.begin(), apps.end(), [](const HomebrewApp& a, const HomebrewApp& b) { 
                return a.downloads > b.downloads; 
            });
            break;
        default:
            sort(apps.begin(), apps.end(), [](const HomebrewApp& a, const HomebrewApp& b) { 
                return a.name < b.name; 
            });
    }
}

void filterApps(vector<HomebrewApp>& apps, const vector<HomebrewApp>& allApps) {
    apps.clear();
    for(auto &a : allApps) {
        bool matchesCat = (currentCatIndex == 0 || a.category == categories[currentCatIndex]);
        bool matchesSearch = (searchQuery == "" || containsSearch(a.name, searchQuery));
        bool matchesFilter = (currentFilter == FILTER_ALL) || 
                            (currentFilter == FILTER_FREE && a.cost == 0) ||
                            (currentFilter == FILTER_PAID && a.cost > 0);
        if (matchesCat && matchesSearch && matchesFilter) apps.push_back(a);
    }
    sortApps(apps);
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
                a.cost = i["cost"]; 
                a.description = i.contains("desc") ? (string)i["desc"] : "";
                a.patchNotes = i.contains("patch_notes") ? (string)i["patch_notes"] : "";
                a.rating = i.contains("rating") ? (float)i["rating"] : 0.0f;
                a.voteCount = i.contains("voteCount") ? (int)i["voteCount"] : 0;
                a.downloads = i.contains("downloads") ? (int)i["downloads"] : 0;
                a.isFavorite = i.contains("isFavorite") ? (bool)i["isFavorite"] : false;
                a.color = generateColor(a.name); 
                a.iconLetter = a.name[0];
                allApps.push_back(a);
            }
            
            for(auto& n : j["news"]) newsList.push_back({n["title"], n["content"]});
            
            if(j.contains("missions")) {
                for(auto& m : j["missions"]) {
                    DailyMission mission;
                    mission.title = m["title"];
                    mission.description = m["description"];
                    mission.reward = m["reward"];
                    mission.progress = m.contains("progress") ? (int)m["progress"] : 0;
                    mission.target = m["target"];
                    mission.completed = m.contains("completed") ? (bool)m["completed"] : false;
                    dailyMissions.push_back(mission);
                }
            }
            
            if(j.contains("history")) {
                for(auto& h : j["history"]) {
                    DownloadHistory entry;
                    entry.appName = h["appName"];
                    entry.date = h["date"];
                    entry.cost = h["cost"];
                    downloadHistory.push_back(entry);
                }
            }
        } catch(...) {}
    }

    int cursor = 0;
    HomebrewApp selectedApp;
    bool running = true;
    bool pointAnimation = false;
    int animTimer = 0;

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
        if (kDown & HidNpadButton_Minus) currentState = STATE_QUEUE;
        if (kDown & HidNpadButton_L) currentState = STATE_MISSIONS;
        if (kDown & HidNpadButton_R) currentState = STATE_FAVORITES;
        if (kDown & HidNpadButton_ZL) currentState = STATE_HISTORY;

        if (needFilter) {
            filterApps(displayApps, allApps);
            needFilter = false;
        }

        // RENDU FOND
        if (currentState != STATE_SETTINGS) {
            SDL_SetRenderDrawColor(renderer, C_BG.r, C_BG.g, C_BG.b, 255);
            SDL_RenderClear(renderer);
            drawSystemBar();
            drawText(broadcastMsg, 640, 30, fontSmall, C_ACCENT, true);
        }

        if (currentState == STATE_SETTINGS) {
            handleSettingsInput(running);
            if (kDown & HidNpadButton_B) currentState = STATE_DASHBOARD;
            extern int settingsCursor;
            if ((kDown & HidNpadButton_A) && settingsCursor == 4) currentState = STATE_DASHBOARD;
            drawSettingsMenu();
            drawFooter("[A] Valider [B] Retour");
        }

        else if (currentState == STATE_DASHBOARD) {
            // CLICKER
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

            // ONGLETS
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

            // TRI
            int sortX = 900, sortY = 130;
            vector<string> sortLabels = {"NOM", "€", "★", "DL"};
            drawText("Tri:", sortX - 40, sortY + 5, fontSmall, C_TEXT_DIM);
            for(int i = 0; i < 4; i++) {
                int btnX = sortX + i * 70;
                bool selected = (currentSort == (SortMode)i);
                drawRoundedBox(btnX, sortY, 65, 30, 10, selected ? C_ACCENT : C_CARD_BG);
                drawText(sortLabels[i], btnX + 32, sortY + 8, fontSmall, selected ? C_BTN_TEXT : C_TEXT_DIM, true);
                if (touched && isPointInRect(touchX, touchY, btnX, sortY, 65, 30)) {
                    currentSort = (SortMode)i; needFilter = true; playSfx();
                }
            }

            // NEWS
            drawText("ACTUALITÉS", 40, 100, fontMedium, C_ACCENT);
            for(int i=0; i<(int)newsList.size(); i++) {
                int y = 140 + i*150 - newsScrollOffset;
                if (y < 60 || y > 650) continue;

                drawRoundedBox(40, y, 300, 130, 15, C_CARD_BG);
                drawText(newsList[i].title, 60, y+15, fontMedium, C_TEXT);
                string shortContent = newsList[i].content;
                if(shortContent.length() > 60) shortContent = shortContent.substr(0, 57) + "...";
                drawText(shortContent, 60, y+50, fontSmall, C_TEXT_DIM, false, 260);
                drawText("Lire >", 250, y+100, fontSmall, C_ACCENT);

                if (touched && isPointInRect(touchX, touchY, 40, y, 300, 130)) {
                    currentNews = newsList[i];
                    currentState = STATE_NEWS_DETAILS;
                    playSfx();
                }
            }

            // GRILLE APPS
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
                
                if(displayApps[i].voteCount > 0) {
                    char ratingText[16];
                    sprintf(ratingText, "★%.1f (%d)", displayApps[i].rating, displayApps[i].voteCount);
                    drawText(ratingText, x+120, y+45, fontSmall, C_ACCENT);
                    drawText(displayApps[i].category, x+120, y+70, fontSmall, C_TEXT_DIM);
                } else {
                    drawText(displayApps[i].category, x+120, y+60, fontSmall, C_TEXT_DIM);
                }
                
                string price = (displayApps[i].cost == 0) ? "GRATUIT" : to_string(displayApps[i].cost) + " PTS";
                drawRoundedBox(x+120, y+100, 120, 30, 10, C_BG);
                drawText(price, x+130, y+105, fontSmall, C_ACCENT);
                
                if(displayApps[i].isFavorite) {
                    drawText("♥", x+370, y+10, fontMedium, {255, 100, 100, 255});
                }

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

            drawFooter("[A] Voir [X] Chercher [L] Missions [R] Favoris [ZL] Historique");
        }

        else if (currentState == STATE_MISSIONS) {
            drawRoundedBox(200, 80, 880, 560, 30, C_CARD_BG);
            drawText("MISSIONS QUOTIDIENNES", 640, 120, fontLarge, C_ACCENT, true);
            
            if(dailyMissions.empty()) {
                drawText("Aucune mission", 640, 300, fontMedium, C_TEXT_DIM, true);
            } else {
                for(int i = 0; i < (int)dailyMissions.size(); i++) {
                    int y = 180 + i * 120;
                    auto& m = dailyMissions[i];
                    
                    drawRoundedBox(240, y, 800, 100, 15, m.completed ? C_ACCENT : C_BG);
                    drawText(m.title, 260, y + 15, fontMedium, m.completed ? C_BTN_TEXT : C_TEXT);
                    drawText(m.description, 260, y + 45, fontSmall, m.completed ? C_BTN_TEXT : C_TEXT_DIM);
                    
                    float progress = (float)m.progress / m.target;
                    drawRoundedBox(260, y + 70, 500, 15, 7, {100,100,100,255});
                    if(progress > 0) drawRoundedBox(260, y + 70, (int)(500*progress), 15, 7, m.completed ? C_BTN_TEXT : C_ACCENT);
                    
                    char txt[32];
                    sprintf(txt, "%d/%d", m.progress, m.target);
                    drawText(txt, 780, y + 68, fontSmall, m.completed ? C_BTN_TEXT : C_TEXT_DIM);
                    
                    if(m.completed) {
                        drawText("✓", 950, y + 30, fontLarge, C_BTN_TEXT);
                    } else {
                        sprintf(txt, "+%d pts", m.reward);
                        drawText(txt, 920, y + 35, fontMedium, C_ACCENT);
                    }
                }
            }
            
            if (kDown & HidNpadButton_B) currentState = STATE_DASHBOARD;
            drawFooter("[B] Retour");
        }

        else if (currentState == STATE_HISTORY) {
            drawRoundedBox(200, 80, 880, 560, 30, C_CARD_BG);
            drawText("HISTORIQUE", 640, 120, fontLarge, C_ACCENT, true);
            
            if(downloadHistory.empty()) {
                drawText("Aucun téléchargement", 640, 300, fontMedium, C_TEXT_DIM, true);
            } else {
                for(int i = 0; i < min(12, (int)downloadHistory.size()); i++) {
                    int y = 180 + i * 40;
                    drawText(downloadHistory[i].appName, 250, y, fontMedium, C_TEXT);
                    drawText(downloadHistory[i].date, 600, y, fontSmall, C_TEXT_DIM);
                    string cost = downloadHistory[i].cost == 0 ? "Gratuit" : to_string(downloadHistory[i].cost) + " pts";
                    drawText(cost, 850, y, fontSmall, C_ACCENT);
                }
            }
            
            if (kDown & HidNpadButton_B) currentState = STATE_DASHBOARD;
            drawFooter("[B] Retour");
        }

        else if (currentState == STATE_FAVORITES) {
            drawRoundedBox(200, 80, 880, 560, 30, C_CARD_BG);
            drawText("MES FAVORIS", 640, 120, fontLarge, C_ACCENT, true);
            
            vector<HomebrewApp> favs;
            for(auto& app : allApps) if(app.isFavorite) favs.push_back(app);
            
            if(favs.empty()) {
                drawText("Aucun favori", 640, 300, fontMedium, C_TEXT_DIM, true);
            } else {
                for(int i = 0; i < (int)favs.size(); i++) {
                    int y = 180 + i * 70;
                    drawRoundedBox(240, y, 800, 60, 15, C_BG);
                    drawRoundedBox(250, y+5, 50, 50, 10, favs[i].color);
                    string letter(1, favs[i].iconLetter);
                    drawText(letter, 275, y+18, fontMedium, C_BTN_TEXT);
                    drawText(favs[i].name, 320, y+15, fontMedium, C_TEXT);
                    
                    string price = favs[i].cost == 0 ? "GRATUIT" : to_string(favs[i].cost) + " PTS";
                    drawText(price, 900, y+20, fontSmall, C_ACCENT);
                    
                    if (touched && isPointInRect(touchX, touchY, 240, y, 800, 60)) {
                        selectedApp = favs[i];
                        currentState = STATE_DETAILS;
                        playSfx();
                    }
                }
            }
            
            if (kDown & HidNpadButton_B) currentState = STATE_DASHBOARD;
            drawFooter("[B] Retour");
        }

        else if (currentState == STATE_RATE_APP) {
            drawRoundedBox(300, 200, 680, 320, 30, C_CARD_BG);
            drawText("NOTER " + selectedApp.name, 640, 240, fontLarge, C_ACCENT, true);
            
            for(int i = 1; i <= 5; i++) {
                int starX = 390 + (i-1) * 100;
                bool selected = (i <= ratingScore);
                drawText("★", starX, 320, fontLarge, selected ? C_ACCENT : C_TEXT_DIM);
                
                if (touched && isPointInRect(touchX, touchY, starX-30, 300, 80, 80)) {
                    ratingScore = i;
                    playSfx();
                }
            }
            
            drawText(to_string(ratingScore) + "/5", 640, 410, fontMedium, C_TEXT, true);
            
            int btnX = 440, btnY = 450;
            drawRoundedBox(btnX, btnY, 400, 50, 25, C_ACCENT);
            drawText("VALIDER", 640, 465, fontMedium, C_BTN_TEXT, true);
            
            if ((touched && isPointInRect(touchX, touchY, btnX, btnY, 400, 50)) || (kDown & HidNpadButton_A)) {
                if(ratingScore > 0) {
                    sendRating(selectedApp.name, ratingScore);
                    ratingScore = 0;
                    currentState = STATE_DETAILS;
                }
            }
            
            if (kDown & HidNpadButton_B) { ratingScore = 0; currentState = STATE_DETAILS; }
            drawFooter("[A] Valider [B] Annuler");
        }

        else if (currentState == STATE_QUEUE) {
            drawRoundedBox(200, 100, 880, 500, 30, C_CARD_BG);
            drawText("FILE D'ATTENTE", 640, 150, fontLarge, C_ACCENT, true);
            
            if (downloadQueue.empty()) {
                drawText("Vide", 640, 300, fontMedium, C_TEXT_DIM, true);
            } else {
                int totalCost = 0;
                for(int i=0; i<(int)downloadQueue.size(); i++) {
                    string itemText = to_string(i+1) + ". " + downloadQueue[i].name + " (" + to_string(downloadQueue[i].cost) + " pts)";
                    drawText(itemText, 250, 220 + i*40, fontMedium, C_TEXT);
                    totalCost += downloadQueue[i].cost;
                }
                
                drawText("Total: " + to_string(totalCost) + " points (WIP)", 640, 450, fontMedium, C_ACCENT, true);
                
                int bX = 440, bY = 520, bW = 400, bH = 60;
                
                if (currentUser.points >= totalCost) {
                    drawRoundedBox(bX, bY, bW, bH, 30, C_ACCENT);
                    drawText("TOUT TÉLÉCHARGER", 640, 535, fontMedium, C_BTN_TEXT, true);
                    
                    if (touched && isPointInRect(touchX, touchY, bX, bY, bW, bH)) {
                        for(auto& app : downloadQueue) {
                            downloadFile(app.url.c_str(), ("sdmc:/switch/"+app.name+".nsp").c_str());
                            currentUser.points -= app.cost;
                            
                            DownloadHistory entry;
                            entry.appName = app.name;
                            entry.date = getCurrentDate();
                            entry.cost = app.cost;
                            downloadHistory.insert(downloadHistory.begin(), entry);
                        }
                        
                        sendClickPoint();
                        downloadQueue.clear();
                        currentState = STATE_DASHBOARD;
                    }
                } else {
                    drawRoundedBox(bX, bY, bW, bH, 30, C_TEXT_DIM);
                    drawText("POINTS INSUFFISANTS", 640, 535, fontMedium, C_BTN_TEXT, true);
                }
            }
            
            if (kDown & HidNpadButton_B) currentState = STATE_DASHBOARD;
            drawFooter("[B] Retour");
        }

        else if (currentState == STATE_NEWS_DETAILS) {
            int retX = 20, retY = 80, retW = 150, retH = 50;
            drawRoundedBox(retX, retY, retW, retH, 15, C_CARD_BG);
            drawText("< Retour", retX + 75, retY + 25, fontMedium, C_TEXT, true);
            if ((touched && isPointInRect(touchX, touchY, retX, retY, retW, retH)) || (kDown & HidNpadButton_B)) { 
                currentState = STATE_DASHBOARD; playSfx(); 
            }

            drawRoundedBox(200, 100, 880, 500, 30, C_CARD_BG);
            drawText(currentNews.title, 640, 150, fontLarge, C_ACCENT, true);
            drawText(currentNews.content, 640, 250, fontMedium, C_TEXT, true, 800);
            drawFooter("[B] Retour");
        }

        else if (currentState == STATE_DETAILS) {
            int retX = 20, retY = 80, retW = 150, retH = 50;
            drawRoundedBox(retX, retY, retW, retH, 15, C_CARD_BG);
            drawText("< Retour", retX + 75, retY + 25, fontMedium, C_TEXT, true);
            if ((touched && isPointInRect(touchX, touchY, retX, retY, retW, retH)) || (kDown & HidNpadButton_B)) { 
                currentState = STATE_DASHBOARD; 
                playSfx(); 
            }

            drawRoundedBox(200, 100, 880, 500, 30, C_CARD_BG);
            drawRoundedBox(240, 140, 100, 100, 25, selectedApp.color);
            string letter(1, selectedApp.iconLetter);
            drawText(letter, 290, 165, fontLarge, C_BTN_TEXT);
            
            drawText(selectedApp.name, 360, 150, fontLarge, C_TEXT);
            
            if(selectedApp.voteCount > 0) {
                char ratingText[32];
                sprintf(ratingText, "★ %.1f/5 (%d avis)", selectedApp.rating, selectedApp.voteCount);
                drawText(ratingText, 360, 185, fontSmall, C_ACCENT);
            }
            
            drawText("DESCRIPTION", 240, 260, fontMedium, C_ACCENT);
            drawText(selectedApp.description, 240, 290, fontSmall, C_TEXT, false, 800);

            string priceText = (selectedApp.cost == 0) ? "GRATUIT" : to_string(selectedApp.cost) + " POINTS";
            drawText(priceText, 240, 400, fontMedium, C_ACCENT);
            
            // Bouton favori
            int favX = 240, favY = 450, favW = 180, favH = 50;
            drawRoundedBox(favX, favY, favW, favH, 25, selectedApp.isFavorite ? C_ACCENT : C_BG);
            drawText(selectedApp.isFavorite ? "♥ Retirer" : "♥ Favoris", favX + 90, favY + 15, fontMedium, 
                     selectedApp.isFavorite ? C_BTN_TEXT : C_TEXT, true);
            
            if (touched && isPointInRect(touchX, touchY, favX, favY, favW, favH)) {
                selectedApp.isFavorite = !selectedApp.isFavorite;
                for(auto& app : allApps) {
                    if(app.name == selectedApp.name) {
                        app.isFavorite = selectedApp.isFavorite;
                        break;
                    }
                }
                toggleFavorite(selectedApp.name);
                playSfx();
            }

            // Bouton noter
            int rateX = 450, rateY = 450;
            drawRoundedBox(rateX, rateY, 180, 50, 25, C_CARD_BG);
            drawText("★ Noter", rateX + 90, rateY + 15, fontMedium, C_TEXT, true);
            
            if (touched && isPointInRect(touchX, touchY, rateX, rateY, 180, 50)) {
                currentState = STATE_RATE_APP;
                ratingScore = 0;
                playSfx();
            }

            // Bouton ajouter à la file
            int btnX = 240, btnY = 530, btnW = 800, btnH = 60;
            bool canAfford = (currentUser.points >= selectedApp.cost);
            drawRoundedBox(btnX, btnY, btnW, btnH, 30, canAfford ? C_ACCENT : C_TEXT_DIM);
            drawText("AJOUTER À LA FILE", 640, 545, fontMedium, C_BTN_TEXT, true);

            if (canAfford && ((kDown & HidNpadButton_A) || (touched && isPointInRect(touchX, touchY, btnX, btnY, btnW, btnH)))) {
                downloadQueue.push_back(selectedApp);
                currentState = STATE_QUEUE;
                playSfx();
            }
            
            drawFooter("[A] Ajouter [B] Retour");
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    cleanupGraphics();
    psmExit();
    socketExit();
    return 0;
}