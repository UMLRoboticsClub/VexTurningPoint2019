//#define DEBUG
//#define CRCOFF

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <atomic>

#include <cstdio>
#include <cstring>

#include "pros/apix.h"

#include "crc.h"
#include "serial.h"
#include "globals.h"

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace Serial {

    static const char *header = "zz ";
    static const int HEARTBEAT_INTV = 1000; //ms

    //task-safe copy of targets after all have been parsed
    static vector<Point> targetscopy;
    //prevent accessing targets while swapping
    static pros::Mutex mutex;
    //used to check if new data has been received
    static std::atomic<bool> newPacket = false;

    //private
    void parseInput(std::string &input, std::vector<Point> &targets);
    void readVisionData(void*);
    void serialHeartbeat(void*);

    void initialize(){
        Task ser_read(readVisionData);
        Task heartbeat(serialHeartbeat);
    }

    void parseInput(const char *buf, vector<Point> &targets){
        targets.clear();
        int len = strlen(buf);
#ifdef DEBUG
        cout << "input: [" << buf << "]" << endl;
        cout << "size : " << len << endl;
#endif
        int pktIndex = 0;

        //check header
        const char *header = "zz ";
        const int headerSize = strlen(header);

        if(len < headerSize + 8){
#ifdef DEBUG
            cout << "packet messed up: too small" << endl;
#endif
            return;
        }

        while(isspace(buf[pktIndex + 1])) ++pktIndex; //skip spaces

        for(; pktIndex < headerSize; ++pktIndex){
            if(header[pktIndex] != buf[pktIndex]){
#ifdef DEBUG
                cout << "packet messed up: bad header" << endl;
#endif
                return;
            }
        }

#ifdef DEBUG
        cout << "packet header ok" << endl;
#endif

        //while(isspace(buf[pktIndex + 1])) ++pktIndex; //skip spaces

        char *end;
        const char *afterHeader = buf + pktIndex - 1;
        //get size
        const int size = strtol(afterHeader, &end, 10);
        //digits of 'size'
        const int sizeSize = end - afterHeader;
        for(int i = 0; i < size; ++i){
            //get the rest of the numbers
            targets.emplace_back((int)strtol(end, &end, 10), (int)strtol(end, &end, 10));

#ifdef DEBUG
            cout << "point:" << '[' << targets.back().first << "," << targets.back().second << ']' << endl;
#endif

        }

        char *dataStart = (char*)buf + pktIndex + sizeSize;
        const int dataLen = end - buf - (dataStart - buf);

#ifdef DEBUG
        for(int i = 0; i < dataLen; ++i){
            cout << '[' << *(dataStart + i) << ']';
        }
        cout << endl;
#endif

#ifndef CRCOFF

        //need to use unsigned long ver of strtol
        uint32_t repcrc = strtoul(end, NULL, 10);
        uint32_t gencrc = crc32buf(dataStart, dataLen);

#ifdef DEBUG
        cout << "reported crc:  " << repcrc << endl;
        cout << "generated crc: " << gencrc << endl;
#endif

        if(repcrc != gencrc) {

#ifdef DEBUG
            cout << "Bad CRC!" << endl;
            cout << endl;
            fflush(stdout);
#endif

            targets.clear();
            return;
        }

#ifdef DEBUG
        cout << "CRCs match!" << endl;
        cout << endl;
#endif

#endif

        fflush(stdout);
    }

    void readVisionData(void*){
        const int headerLen = strlen(header);
        vector<Point> targets;

        const int BUF_SIZE = 128;
        char buf[BUF_SIZE];

        while(true){
            cin.getline(buf, BUF_SIZE);

            //skip if header doesn't exist
            if(strncmp(buf, header, headerLen) != 0){
#ifdef DEBUG
                cout << "header doesn't exist:[" << buf << "]" << endl;
#endif
                continue;
            }
#ifdef DEBUG
            cout << "header exists" << endl;
#endif

            parseInput(buf, targets);

            lock_guard lock(mutex);
            targetscopy.swap(targets);
            newPacket = true;
        }
    }

    void getTargets(std::vector<Point> &targets){
        while(!newPacket){ delay(5); }
        newPacket = false;

        lock_guard lock(mutex);
        targets.swap(targetscopy);
    }

    void serialHeartbeat(void*){
        using namespace pros;

        uint32_t now = millis();
        while(true){
            cout << endl;
            Task::delay_until(&now, HEARTBEAT_INTV);
        }
    }

};
