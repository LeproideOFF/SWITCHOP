#include "app.hpp"
#include <sys/socket.h>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <curl/curl.h>

using namespace std;

string loginMessage = "";

// Structure pour téléchargement avec CURL
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Callback pour CURL (download en mémoire)
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Callback CURL pour écrire dans std::string
static size_t WriteMem(void *contents, size_t size, size_t nmemb, void *userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Générer ou lire le Device ID
string getDeviceId() {
    string path = "sdmc:/switch/auth.id";
    ifstream file(path);
    string id;
    if (file.good()) { 
        getline(file, id); 
    } else {
        srand(time(NULL));
        id = "SWITCH-" + to_string(rand() % 999999);
        ofstream outfile(path); 
        outfile << id; 
        outfile.close();
    }
    return id;
}

// POST JSON → retourne nlohmann::json
nlohmann::json postJson(const std::string& endpoint, const nlohmann::json& data) {
    CURL *curl = curl_easy_init();
    string buffer;
    if(curl) {
        string js = data.dump();
        struct curl_slist *h = curl_slist_append(NULL, "Content-Type: application/json");

        string full_url = string(SERVER_URL) + endpoint;

        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, js.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SWITCHOP-Agent/1.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMem);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(h);
    }

    try {
        return nlohmann::json::parse(buffer);
    } catch(...) {
        return nlohmann::json(); // retourne JSON vide si erreur
    }
}

// Connexion utilisateur
void loginUser() {
    currentUser.deviceId = getDeviceId();
    json p; p["deviceId"] = currentUser.deviceId;
    json j = postJson("/auth/login", p);
    if(j.contains("success") && j["success"]) {
        currentUser.points = j["user"]["points"];
        currentUser.loggedIn = true;
    }
}

// Click point
void sendClickPoint() {
    json p; p["deviceId"] = currentUser.deviceId;
    json j = postJson("/auth/click", p);
    if(j.contains("points")) currentUser.points = j["points"];
}

// Acheter une app
bool buyApp(HomebrewApp app) {
    json p; p["deviceId"] = currentUser.deviceId; p["appName"] = app.name;
    json j = postJson("/shop/buy", p);
    if(j.contains("success") && j["success"]) {
        currentUser.points = j["new_points"];
        return true;
    }
    return false;
}

// Fetch JSON depuis serveur
string fetchJson(string endpoint) {
    CURL *curl = curl_easy_init();
    string buffer;
    if(curl) {
        string full_url = string(SERVER_URL) + endpoint;

        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SWITCHOP-Agent/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMem);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return buffer;
}

// Envoyer une note
void sendRating(string appName, int score) {
    json p; p["deviceId"] = currentUser.deviceId; p["appName"] = appName; p["rating"] = score;
    postJson("/shop/rate", p);
}

// Toggle favori
void toggleFavorite(string appName) {
    json p; p["deviceId"] = currentUser.deviceId; p["appName"] = appName;
    postJson("/shop/favorite", p);
}

// Terminer mission
void completeMission(int missionId) {
    json p; p["deviceId"] = currentUser.deviceId; p["missionId"] = missionId;
    json j = postJson("/shop/mission/complete", p);
    if(j.contains("success") && j["success"]) currentUser.points = j["new_points"];
}

// Télécharger un fichier
void downloadFile(const char* url, const char* path) {
    CURL *curl = curl_easy_init();
    if(curl) {
        struct MemoryStruct chunk;
        chunk.memory = (char*)malloc(1); 
        chunk.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SWITCHOP-Agent/1.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        ProgressData d = {0, 0, 0};
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &d);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        CURLcode res = curl_easy_perform(curl);

        if(res == CURLE_OK) {
            FILE *fp = fopen(path, "wb");
            if(fp) {
                fwrite(chunk.memory, 1, chunk.size, fp);
                fclose(fp);
            }
        }

        if(chunk.memory) free(chunk.memory);
        curl_easy_cleanup(curl);
    }
}

// Popup d’erreur
void showError(const std::string& msg) {
    for(int i = 0; i < 120; i++) {
        drawRoundedBox(340, 260, 600, 200, 20, C_CARD_BG);
        drawRoundedBox(340, 260, 600, 60, 20, {255,50,50,255});
        drawText("ERREUR", 640, 280, fontMedium, {255,255,255,255}, true);
        drawText(msg, 640, 360, fontMedium, {255,255,255,255}, true);
        SDL_RenderPresent(renderer);
    }
}

// Installer NSP
bool installNSP(HomebrewApp app) {
    ncmInitialize();
    downloadFile(app.url.c_str(), "sdmc:/switch/tmp.nsp");
    ncmExit();
    return true;
}
