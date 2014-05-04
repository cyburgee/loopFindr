#pragma once
#include "ofMain.h"

class loopFindThread;
class loopFoundEventArgs : public ofEventArgs {
public:
    loopFindThread *loopThread;
    vector<int> indices;
    /*ofxTimeline* sender;
	float currentPercent;
	float currentTime;
	int currentFrame;
	float durationInSeconds;
	int durationInFrames;*/
};

class loopFindEvents {
public:
    ofEvent<loopFoundEventArgs> loopFoundEvent;
};