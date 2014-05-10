#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxGifEncoder.h"
#include "ofxUI.h"
#include "ofxTimeline.h"
#include "loopFindThread.h"
#include "loopFindEvents.h"

class ofApp : public ofBaseApp{
public:
    ofApp(){
        
    }
    void setup();
    void update();
    void draw();
    
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void keyPressed(int key);
    void keyReleased(int key);
    
    void exit();
    
    bool hasMovement();
    bool checkForLoop();
    void getBestLoop(cv::Mat start,cv::Scalar startMean);
    bool ditchSimilarLoop();
    void populateLoopEnds();
    void initEnds();
    void getMatFromFrameNum(cv::Mat *gray, int frameNum);
    void saveGif(int i);
    void refineLoop();
    void updateRefinedLoop();
    void onGifSaved(string &fileName);
    void loadVideo(string videoPath, string videoName);
    void playStopped(ofxTLPlaybackEventArgs& playbackEvent);
    void playStarted(ofxTLPlaybackEventArgs& playbackEvent);
    void playScrubbed(ofxTLPlaybackEventArgs& playbackEvent);
    void playLooped(ofxTLPlaybackEventArgs& playbackEvent);
    void foundLoop(loopFoundEventArgs& loopArgs);
    
    void addLoopFrames();
    
    ofTrueTypeFont font;
    
    ofImage im;
    
    ofQTKitPlayer vidPlayer;
    ofVideoPlayer timeVid;
    
    ofxTimeline timeline;
    ofxTLVideoTrack* videoTrack;
    ofFbo frameBuffer;
    ofRectangle vidRect;
    
    /*ofxCv::FlowPyrLK flows;
    ofxCv::FlowFarneback flow;
    ofMesh mesh;
    int stepSize, xSteps, ySteps;*/
    
    string fileName;
    string fileExt;
    string outputPrefix;
    string videoFilePath;
    
    vector<cv::Mat> allFrames;
    vector< vector<ofImage> > loops;
    //vector<ofVec2f> loopIndeces;
    vector< vector<int> > loopIndeces;
    int tempLoopIndeces[2];
    vector< vector<ofImage*> > displayLoops;
    vector< cv::Mat > loopStartMats;
    vector<int> matchIndeces;
    vector<int> loopLengths;
    vector<int> loopPlayIdx;
    vector<ofRectangle> loopDrawRects;
    vector<ofPoint> drawLoopPoints;
    vector<int> loopsOnDisplay;
    int loopPage;
    //ofPoint drawLoopPoint;
    vector<float> loopQuality;
    float loopWidth;
    float loopHeight;
    
    int loopSelected;
    int frameBeforeRefine;
    bool refiningLoop;
    
    ofRange oldRange;
    
    int numFrames;
    int minPeriod;
    float minPeriodSecs;
    int maxPeriod;
    float maxPeriodSecs;
    float fps;
    int frameStart;
    int potentialEndIdx;
    int pauseFrameNum;
    bool needToInitEnds;
    
    cv::Mat firstFrame;
    vector< cv::Mat > potentialLoopEnds;
    vector<int> frameNumsInEnds;
    vector<cv::Mat> bestMatches;
    
    cv::Size imResize;
    int scale;
    
    float minMovementThresh;
    float maxMovementThresh;
    float loopThresh;
    float minChangeRatio;
    bool minMovementBool;
    bool maxMovementBool;
    bool rangeChanged;
    
    bool loopFound;
    int loopNum;
    
    int gifNum;
    vector<ofxGifEncoder *> gifses;
    bool savingGif;
    bool gifSaveThreadRunning;
    
    bool pausePlayback;
    bool videoLoaded;
    bool videoGood;
    bool newVideo;
    bool finishedVideo;
    
    int numLoopsInRow;
    ofRectangle loopThumbBounds;
    bool scrolling;
    ofPoint dragPoint;
    int xStart;
    int yStart;
    
    //GUI STUFF
	void setGuiMatch();
    void setGuiLoops();
    void setGuiInstructions();
    void setGuiLoopStatus();
	ofxUISuperCanvas *guiMatch;
    ofxUISuperCanvas *guiLoops;
    ofxUILabel statusLabel;
    ofxUISpacer *gifSaveSpacer;
    ofxUILabel *gifSaveStatusLabel;
    ofxUITextArea *loopsIndexLabel;
    ofxUITextArea *loopsFoundLabel;
    ofxUITextArea *loopStatsLabelTime;
    ofxUITextArea *loopStatsLabelFrame;
    bool pageLeft;
    bool pageRight;
    ofxUISlider *minMove;
    ofxUISlider *maxMove;
    ofxUIRangeSlider *loopLengthSlider;
    ofxUITextArea *instructLabel;
    string instructions;
    string pauseInstruct;
	bool hideGUI;
	void guiEvent(ofxUIEventArgs &e);
    
    //Thread
    vector< loopFindThread*> threads;
    loopFindThread thread;
    ofQTKitPlayer threadPlayer;
    loopFindEvents loopEvents;

};
