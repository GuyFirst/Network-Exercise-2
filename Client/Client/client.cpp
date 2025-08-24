
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <windows.h>

#define TIME_PORT 27015

enum RequestCode : unsigned char {
    GET_TIME = 1,
    GET_TIME_NO_DATE,
    GET_TIME_SINCE_EPOCH,
    GET_C2S_DELAY_EST,
    MEASURE_RTT,
    GET_TIME_NO_SECONDS,
    GET_YEAR,
    GET_MONTH_AND_DAY,
    GET_SECONDS_SINCE_MONTH_START,
    GET_WEEK_OF_YEAR,
    GET_DST_FLAG,
    GET_TIME_IN_CITY,
    MEASURE_TIMELAP
};

static int sendReq(SOCKET s, unsigned char code, const string& payload) {
    char buf[512];
    unsigned char len = (unsigned char)min<size_t>(payload.size(), 250);
    buf[0] = (char)code;
    buf[1] = (char)len;
    if (len) memcpy(&buf[2], payload.data(), len);
    return send(s, buf, 2 + len, 0);
}

static int recvStr(SOCKET s, string& out) {
    char rbuf[512];
    int n = recv(s, rbuf, sizeof(rbuf) - 1, 0);
    if (n <= 0) return n;
    rbuf[n] = '\0';
    out.assign(rbuf, rbuf + n);
    return n;
}

static void printMenu() {
    cout << "\n=== Menu ===\n";
    cout << " 1) GetTime\n";
    cout << " 2) GetTimeWithoutDate\n";
    cout << " 3) GetTimeSinceEpoch\n";
    cout << " 4) GetClientToServerDelayEstimation (x100, then average)\n";
    cout << " 5) MeasureRTT (x100, request/response pairs)\n";
    cout << " 6) GetTimeWithoutDateOrSeconds\n";
    cout << " 7) GetYear\n";
    cout << " 8) GetMonthAndDay\n";
    cout << " 9) GetSecondsSinceBeginingOfMonth\n";
    cout << "10) GetWeekOfYear\n";
    cout << "11) GetDaylightSavings\n";
    cout << "12) GetTimeWithoutDateInCity\n";
    cout << "13) MeasureTimeLap (call once to start, once to stop)\n";
    cout << "99) Exit\n";
    cout << "Select: ";
}

int main() {
    WSAData wsaData;
    if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        cout << "Error at WSAStartup()\n";
        return 1;
    }

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        cout << "Error at socket(): " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("10.100.102.7");
    server.sin_port = htons(TIME_PORT);
    if (connect(s, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        cout << "Error at connect(): " << WSAGetLastError() << endl;
        closesocket(s);
        WSACleanup();
        return 1;
    }

    while (true) {
        printMenu();
        int choice;
        if (!(cin >> choice)) break;
        if (choice == 99) break;

        string payload, reply;

        switch (choice) {
        case GET_TIME:
        case GET_TIME_NO_DATE:
        case GET_TIME_SINCE_EPOCH:
        case GET_TIME_NO_SECONDS:
        case GET_YEAR:
        case GET_MONTH_AND_DAY:
        case GET_SECONDS_SINCE_MONTH_START:
        case GET_WEEK_OF_YEAR:
        case GET_DST_FLAG:
        case MEASURE_TIMELAP:
            if (sendReq(s, (unsigned char)choice, "") == SOCKET_ERROR) break;
            if (recvStr(s, reply) <= 0) break;
            cout << ">> " << reply << "\n";
            break;

        case GET_C2S_DELAY_EST:
        {
            const int N = 100;
            vector<DWORD> serverTicks;
            serverTicks.reserve(N);

            for (int i = 0; i < N; ++i) {
                if (sendReq(s, GET_C2S_DELAY_EST, "") == SOCKET_ERROR) {
                    cout << "send error\n";
                    break;
                }

                string reply;
                if (recvStr(s, reply) <= 0) {
                    cout << "recv error at " << i << "\n";
                    break;
                }

                DWORD serverTick = (DWORD)strtoul(reply.c_str(), nullptr, 10);
                serverTicks.push_back(serverTick);
            }

            if (serverTicks.size() >= 2) {
                DWORD sum = 0;
                for (size_t i = 1; i < serverTicks.size(); ++i)
                    sum += (serverTicks[i] - serverTicks[i - 1]);
                double avg = (double)sum / (double)(serverTicks.size() - 1);
                cout << "Estimated avg client->server spacing (ms, server-side measured): " << avg << " ms\n";
            }
            else {
                cout << "Not enough samples.\n";
            }
            break;
        }
        case MEASURE_RTT:
        {
            const int N = 100;
            vector<double> samples;
            samples.reserve(N);
            for (int i = 0; i < N; ++i) {
                DWORD t0 = GetTickCount();
                if (sendReq(s, MEASURE_RTT, "") == SOCKET_ERROR) break;
                if (recvStr(s, reply) <= 0) break;
                DWORD t1 = GetTickCount();
                samples.push_back((double)(t1 - t0));
            }
            if (!samples.empty()) {
                double sum = 0;
                for (double x : samples) sum += x;
                cout << "Average RTT: " << (sum / samples.size()) << " ms\n";
            }
            break;
        }
        case GET_TIME_IN_CITY:
            cout << "Enter city: ";
            getline(cin >> ws, payload);
            if (sendReq(s, GET_TIME_IN_CITY, payload) == SOCKET_ERROR) break;
            if (recvStr(s, reply) <= 0) break;
            cout << ">> " << reply << "\n";
            break;
        default:
            cout << "Unknown choice.\n";
            break;
        }
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
