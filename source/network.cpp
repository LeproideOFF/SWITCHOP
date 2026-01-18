#include "app.hpp"
#include <sys/socket.h>

string loginMessage = "";

static size_t WriteMem(void *contents, size_t size, size_t nmemb, void *userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string getDeviceId() {
    string path = "sdmc:/switch/auth.id";
    ifstream file(path);
    string id;
    if (file.good()) { getline(file, id); } 
    else {
        srand(time(NULL));
        id = "SWITCH-" + to_string(rand() % 999999);
        ofstream outfile(path); outfile << id; outfile.close();
    }
    return id;
}

string postJson(string endpoint, json data) {
    CURL *curl = curl_easy_init();
    string buffer;
    if(curl) {
        string js = data.dump();
        struct curl_slist *h = curl_slist_append(NULL, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, (string(SERVER_URL) + endpoint).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, js.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMem);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return buffer;
}

void loginUser() {
    currentUser.deviceId = getDeviceId();
    json p; p["deviceId"] = currentUser.deviceId;
    string res = postJson("/auth/login", p);
    if (!res.empty()) {
        try {
            auto j = json::parse(res);
            if(j["success"]) { currentUser.points = j["user"]["points"]; currentUser.loggedIn = true; }
        } catch(...) {}
    }
}

void sendClickPoint() {
    json p; p["deviceId"] = currentUser.deviceId;
    string res = postJson("/auth/click", p);
    if(!res.empty()) { try { auto j = json::parse(res); currentUser.points = j["points"]; } catch(...) {} }
}

bool buyApp(HomebrewApp app) {
    json p; p["deviceId"] = currentUser.deviceId; p["appName"] = app.name;
    string res = postJson("/shop/buy", p);
    if (!res.empty()) { try { auto j = json::parse(res); if(j["success"]) { currentUser.points = j["new_points"]; return true; } } catch(...) {} }
    return false;
}

string fetchJson(string endpoint) {
    CURL *curl = curl_easy_init();
    string buffer;
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, (string(SERVER_URL) + endpoint).c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMem);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return buffer;
}

void downloadFile(const char* url, const char* path) {
    CURL *curl = curl_easy_init();
    if (curl) {
        FILE *fp = fopen(path, "wb");
        if(fp) {
            ProgressData d = {0, 0, 0};
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &d);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_perform(curl);
            fclose(fp);
        }
        curl_easy_cleanup(curl);
    }
}

bool installNSP(HomebrewApp app) {
    ncmInitialize();
    downloadFile(app.url.c_str(), "sdmc:/switch/tmp.nsp");
    ncmExit();
    return true;
}