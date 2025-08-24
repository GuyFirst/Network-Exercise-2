
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
        cout << "Time Client: Error at WSAStartup()\n";
        return 1;
    }

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        cout << "Time Client: Error at socket(): " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(TIME_PORT);
    if (connect(s, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        cout << "Time Client: Error at connect(): " << WSAGetLastError() << endl;
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
        int sent = 0, recvd = 0;

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
        {
            sent = sendReq(s, (unsigned char)choice, "");
            if (sent == SOCKET_ERROR) { cout << "send error: " << WSAGetLastError() << "\n"; break; }
            recvd = recvStr(s, reply);
            if (recvd <= 0) { cout << "recv timeout/error\n"; break; }
            cout << ">> " << reply << "\n";
            break;
        }
        case GET_C2S_DELAY_EST:
        {
            const int N = 100;
            for (int i = 0; i < N; ++i) {
                if (sendReq(s, GET_C2S_DELAY_EST, "") == SOCKET_ERROR) {
                    cout << "send error: " << WSAGetLastError() << "\n";
                    break;
                }
            }
            vector<unsigned long long> ticks; ticks.reserve(N);
            for (int i = 0; i < N; ++i) {
                if (recvStr(s, reply) <= 0) { cout << "recv timeout/error at " << i << "\n"; break; }
                ticks.push_back(strtoull(reply.c_str(), nullptr, 10));
            }
            if (ticks.size() >= 2) {
                unsigned long long sum = 0;
                for (size_t i = 1; i < ticks.size(); ++i) sum += (ticks[i] - ticks[i - 1]);
                double avg = (double)sum / (double)(ticks.size() - 1);
                cout << "Estimated avg client->server spacing (ms, server-observed): " << avg << "\n";
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
                Sleep(1);
                if (sendReq(s, MEASURE_RTT, "") == SOCKET_ERROR) {
                    cout << "send error at " << i << ": " << WSAGetLastError() << "\n";
                    break;
                }

                string reply;
                if (recvStr(s, reply) <= 0) {
                    cout << "recv timeout/error at " << i << "\n";
                    break;
                }

                DWORD t1 = GetTickCount();
                DWORD rtt = t1 - t0;
                samples.push_back((double)rtt);
            }

            if (!samples.empty()) {
                double sum = 0;
                for (double ms : samples) sum += ms;
                double avg = sum / samples.size();
                cout << "Average RTT over " << samples.size() << " request/response pairs: " << avg << " ms\n";
            }
            else {
                cout << "No RTT samples collected.\n";
            }
            break;
        }
        case GET_TIME_IN_CITY:
        {
            cout << "Enter city (Doha / Prague / New York / Berlin, others -> UTC): ";
            getline(std::cin >> std::ws, payload);

            if (sendReq(s, GET_TIME_IN_CITY, payload) == SOCKET_ERROR) {
                cout << "send error: " << WSAGetLastError() << "\n";
                break;
            }

            int recvd = recvStr(s, reply);
            if (recvd <= 0) {
                cout << "recv timeout/error\n";
                break;
            }
            cout << ">> " << reply << "\n";
            break;
        }
        default:
            cout << "Unknown choice.\n";
            break;
        }
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
