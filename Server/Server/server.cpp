#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <cstdio>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <windows.h>

#define TIME_PORT 27015

enum RequestCode : unsigned char {
    GET_TIME = 1,
    GET_TIME_NO_DATE,
    GET_TIME_SINCE_EPOCH,
    GET_C2S_DELAY_EST,      // 4
    MEASURE_RTT,            // 5
    GET_TIME_NO_SECONDS,    // 6
    GET_YEAR,               // 7
    GET_MONTH_AND_DAY,      // 8
    GET_SECONDS_SINCE_MONTH_START, // 9
    GET_WEEK_OF_YEAR,       // 10
    GET_DST_FLAG,           // 11
    GET_TIME_IN_CITY,       // 12
    MEASURE_TIMELAP         // 13
};

struct LapState {
    DWORD startTick = 0;
    bool active = false;
};

static string addrKey(const sockaddr_in& a) {
    char ip[16]{};
    strcpy(ip, inet_ntoa(a.sin_addr));
    return string(ip) + ":" + to_string(ntohs(a.sin_port));
}

static void tm_now_local(tm& out) {
    time_t now = time(nullptr);
    tm* p = localtime(&now);
    out = *p;
}

static string time_fmt_local(const char* fmt) {
    tm t{};
    tm_now_local(t);
    char buf[64];
    strftime(buf, sizeof(buf), fmt, &t);
    return string(buf);
}

static string seconds_since_epoch() {
    time_t now = time(nullptr);
    return to_string((long long)now);
}

static string seconds_since_month_start() {
    time_t now = time(nullptr);
    tm lt = *localtime(&now);
    tm m0 = lt;
    m0.tm_mday = 1;
    m0.tm_hour = 0;
    m0.tm_min = 0;
    m0.tm_sec = 0;
    time_t t0 = mktime(&m0);
    double diff = difftime(now, t0);
    return to_string((long long)diff);
}

static string week_of_year_simple() {
    time_t now = time(nullptr);
    tm lt = *localtime(&now);
    int week = (lt.tm_yday / 7) + 1; 
    return to_string(week);
}

static string dst_flag() {
    tm lt{};
    tm_now_local(lt);
    return (lt.tm_isdst > 0) ? "1" : "0";
}

static string time_in_city(const string& cityRaw) {
    
    string city = cityRaw;
    for (auto& c : city) c = (char)tolower((unsigned char)c);

    int offsetHours = INT32_MIN;
    if (city == "doha") offsetHours = 3;
    else if (city == "prague") offsetHours = 1;
    else if (city == "berlin") offsetHours = 1;
    else if (city == "new york"|| city == "nyc") offsetHours = -5;

    time_t now = time(nullptr);
    if (offsetHours == INT32_MIN) {
       
        tm* g = gmtime(&now);
        char buf[64];
        strftime(buf, sizeof(buf), "%H:%M:%S (UTC)", g);
        return string("City not supported; showing UTC: ") + buf;
    }

    time_t city_time = now + offsetHours * 3600;
    tm* tt = gmtime(&city_time);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", tt);
    return string(buf);
}

int main() {
    WSAData wsaData;
    if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        cout << "Time Server: Error at WSAStartup()\n";
        return 1;
    }

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(TIME_PORT);

    if (SOCKET_ERROR == bind(s, (SOCKADDR*)&server, sizeof(server))) {
        cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
        closesocket(s);
        WSACleanup();
        return 1;
    }

    cout << "Time Server: Wait for clients' requests.\n";

    map<string, LapState> laps;

    while (true) {
        sockaddr_in client{};
        int clen = sizeof(client);
        char recvBuf[512];
        int n = recvfrom(s, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&client, &clen);
        if (n == SOCKET_ERROR) {
            cout << "Time Server: Error at recvfrom(): " << WSAGetLastError() << endl;
            break;
        }
        if (n < 2) {
    
            continue;
        }

        unsigned char code = (unsigned char)recvBuf[0];
        unsigned char len = (unsigned char)recvBuf[1];
        string payload;
        if (len > 0) {
            if (2 + len > n) len = (unsigned char)max(0, n - 2);
            payload.assign(&recvBuf[2], &recvBuf[2 + len]);
        }

        auto& lap = laps[addrKey(client)];
        if (lap.active && (GetTickCount() - lap.startTick >= 180000)) {
            lap.active = false; 
        }

        string reply;
        switch (code) {
        case GET_TIME:
            reply = time_fmt_local("%Y-%m-%d %H:%M:%S");
            break;
        case GET_TIME_NO_DATE:
            reply = time_fmt_local("%H:%M:%S");
            break;
        case GET_TIME_SINCE_EPOCH:
            reply = seconds_since_epoch();
            break;
        case GET_C2S_DELAY_EST:
            reply = to_string((unsigned long long)GetTickCount());
            break;
        case MEASURE_RTT:
        
            reply = to_string((unsigned long long)GetTickCount());
            break;
        case GET_TIME_NO_SECONDS:
            reply = time_fmt_local("%H:%M");
            break;
        case GET_YEAR:
            reply = time_fmt_local("%Y");
            break;
        case GET_MONTH_AND_DAY:
            reply = time_fmt_local("%m %d");
            break;
        case GET_SECONDS_SINCE_MONTH_START:
            reply = seconds_since_month_start();
            break;
        case GET_WEEK_OF_YEAR:
            reply = week_of_year_simple();
            break;
        case GET_DST_FLAG:
            reply = dst_flag();
            break;
        case GET_TIME_IN_CITY:
            reply = time_in_city(payload);
            break;
        case MEASURE_TIMELAP:
            if (!lap.active) {
                lap.active = true;
                lap.startTick = GetTickCount();
                reply = "TimeLap started";
            }
            else {
                DWORD dt = GetTickCount() - lap.startTick;
                lap.active = false;
                reply = string("TimeLap: ") + to_string((unsigned long long)dt) + " ms";
            }
            break;
        default:
            reply = "Unknown code";
            break;
        }

        sendto(s, reply.c_str(), (int)reply.size(), 0, (sockaddr*)&client, clen);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
