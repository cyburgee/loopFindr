#pragma once
#include "ofMain.h"

class loopFindThread;
class loopFoundEventArgs : public ofEventArgs {
public:
    loopFindThread *loopThread;
    vector<int> indices;
};

class loopFindEvents {
public:
    ofEvent<loopFoundEventArgs> loopFoundEvent;
};