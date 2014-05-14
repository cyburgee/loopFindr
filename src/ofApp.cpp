#include "ofApp.h"

using namespace cv;
using namespace ofxCv;

void ofApp::setup(){
    ofSetWindowTitle("Loop Findr");
    
    videoLoaded = false;
    videoGood = true;
    gifSaveThreadRunning = false;
    ofSetLogLevel(OF_LOG_VERBOSE);
    
    numLoopsInRow = 3;
    
    minMovementThresh = 1;
    minMovementBool = true;
    maxMovementThresh = 20;
    maxMovementBool = false;
    loopThresh = 80.0;
    minChangeRatio = MAXFLOAT;
    minPeriodSecs = 0.5;
    maxPeriodSecs = 2.0;
    
    loopFound = false;
    loopSelected = -1;
    
    
    gifNum = 0;
    ofAddListener(ofxGifEncoder::OFX_GIF_SAVE_FINISHED, this, &ofApp::onGifSaved);
    
    //GUI STUFF
    loopsOnDisplay.push_back(1);
    loopsOnDisplay.push_back(2);
    loopsOnDisplay.push_back(3);
    loopPage = 0;
    hideGUI = false;
    guiMatch = NULL;
    setGuiMatch();
    guiLoops = NULL;
    setGuiLoops();
    
    //LOAD VIDEO FILE
    videoTrack = NULL;
    ofFileDialogResult result = ofSystemLoadDialog("Choose A Video File",false);
    if(result.bSuccess){
        loadVideo(result.getPath(),result.getName());
   }
    
    
    font.loadFont("sans-serif", 15, true, true);
	font.setLineHeight(34.0f);
	font.setLetterSpacing(1.035);
    
    ofSetBackgroundColor(127);
    
    ofAddListener(timeline.events().playbackEnded, this, &ofApp::playStopped);
    ofAddListener(timeline.events().playbackStarted, this, &ofApp::playStarted);
    ofAddListener(timeline.events().playheadScrubbed, this, &ofApp::playScrubbed);
    ofAddListener(timeline.events().playbackLooped, this, &ofApp::playLooped);
    ofAddListener(thread.loopEvents.loopFoundEvent,this,&ofApp::foundLoop);

}


//------------------------------------------------------------------------------------
void ofApp::initEnds(){
    if (potentialLoopEnds.size() > 0){
        potentialLoopEnds.clear();
    }
    
    vidPlayer.setPaused(true);
    vidPlayer.setFrame(frameStart);
    vidPlayer.update();
    
    for (int i = frameStart; i < vidPlayer.getTotalNumFrames() && i < frameStart + maxPeriod; i++) {
        cv::Mat currGray;
        cv::cvtColor(toCv(vidPlayer), currGray, CV_RGB2GRAY);
        cv::Mat currFrame;
        cv::resize(currGray, currFrame, imResize,INTER_AREA);
        
        potentialLoopEnds.push_back(currFrame);
        vidPlayer.nextFrame();
        vidPlayer.update();
    }
    
    vidPlayer.setFrame(frameStart);
    vidPlayer.update();
    needToInitEnds = false;
}


//------------------------------------------------------------------------------------
void ofApp::populateLoopEnds(){
    if (potentialLoopEnds.size() > 0){
        potentialLoopEnds.erase(potentialLoopEnds.begin());
        
        int endIdx = frameStart + maxPeriod;
        if (endIdx < vidPlayer.getTotalNumFrames()){
            vidPlayer.setFrame(endIdx);
            vidPlayer.update();
            
            cv::Mat endGray;
            cv::cvtColor(toCv(vidPlayer), endGray, CV_RGB2GRAY);
            cv::Mat endFrame;
            cv::resize(endGray, endFrame, imResize,INTER_AREA);
            
            potentialLoopEnds.push_back(endFrame);
        }
    }
    else{
        initEnds();
    }
    
    vidPlayer.setFrame(frameStart);
    vidPlayer.update();
}


//------------------------------------------------------------------------------------
bool ofApp::ditchSimilarLoop(){
    if (displayLoops.size() == 1)
        return false;
    
    cv::Mat prevLoopMat;
    loopStartMats.at(loopStartMats.size()-2).copyTo(prevLoopMat);
    cv::Mat newLoopMat;
    loopStartMats.at(loopStartMats.size()-1).copyTo(newLoopMat);
    
    float prevLoopSum = cv::sum(prevLoopMat)[0] + 1;
    
    cv::Mat diff;
    cv::absdiff(prevLoopMat, newLoopMat, diff);
    float endDiff = cv::sum(diff)[0] + 1;
    
    float changeRatio = endDiff/prevLoopSum;
    float changePercent = (changeRatio*100);
    if((changeRatio*100) <= (100-loopThresh)){ // the first frames are very similar
        
        if (loopQuality.at(loopQuality.size() - 2) < loopQuality.at(loopQuality.size() -1)) {
            cout << "erasing last one" << endl;
            loopQuality.erase(loopQuality.end() - 1);
            loopLengths.erase(loopLengths.end() - 1);
            loopPlayIdx.erase(loopPlayIdx.end() - 1);
            vector<ofImage *> disp = displayLoops.at(displayLoops.size() - 1);
            for (std::vector<ofImage *>::iterator i = disp.begin(); i != disp.end(); ++i){
                delete *i;
            }
            displayLoops.erase(displayLoops.end() - 1);
            loopIndeces.erase(loopIndeces.end() - 1);
            loopStartMats.erase(loopStartMats.end() - 1);
        }
        else{
            cout << "erasing second to last" << endl;
            loopQuality.erase(loopQuality.end() - 2 );
            loopLengths.erase(loopLengths.end() - 2);
            loopPlayIdx.erase(loopPlayIdx.end() - 2);
            vector<ofImage *> disp = displayLoops.at(displayLoops.size() - 2);
            for (std::vector<ofImage *>::iterator i = disp.begin(); i != disp.end(); ++i){
                delete *i;
            }
            displayLoops.erase(displayLoops.end() - 2);
            loopIndeces.erase(loopIndeces.end() - 2);
            loopStartMats.erase(loopStartMats.end() - 2);
        }
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------------
void ofApp::update(){
    if (newVideo) {
        timeVid.setFrame(0);
        timeVid.update();
        newVideo = false;
    }
    if (finishedVideo) {
        timeVid.setFrame(0);
        timeVid.update();
        timeline.stop();
    }
    if (loopFound) {
        addLoopFrames();
    }
    if (!refiningLoop && videoGood && !finishedVideo){
        frameStart = timeVid.getCurrentFrame();
        if (videoLoaded && timeline.getIsPlaying() && !needToInitEnds) {
            
            if (!thread.isThreadRunning()) {
                thread.lock();
                thread.setup(&loops, &loopIndeces, &displayLoops, &loopStartMats, &loopLengths, &loopPlayIdx, &loopQuality, loopWidth, loopHeight, minPeriod, maxPeriod, frameStart, &potentialLoopEnds, minMovementThresh, maxMovementThresh, loopThresh, minChangeRatio, minMovementBool, maxMovementBool, videoFilePath, &threadPlayer);
                thread.startThread(true, false);
                thread.unlock();
                thread.lock();
                populateLoopEnds();
                thread.unlock();
                timeVid.nextFrame();
                timeVid.update();
            }
            guiLoops->getCanvasTitle()->setLabel("                                  Processing Video");
        }
        else if (videoLoaded && needToInitEnds)
            initEnds();
        else if(videoLoaded && !timeline.getIsPlaying())
            guiLoops->getCanvasTitle()->setLabel("");
    }
    else{
        needToInitEnds = false;
        if(timeline.getIsPlaying()){
            timeVid.nextFrame();
            timeVid.update();
        }
        guiLoops->getCanvasTitle()->setLabel("");
    }
    if(!refiningLoop){
        for (int i = 0; i < loopPlayIdx.size(); i++) {
            loopPlayIdx.at(i)++;
            if(loopPlayIdx.at(i) >= loopLengths.at(i))
                loopPlayIdx.at(i) = 0;
        }
    }
}

//------------------------------------------------------------------------------------
void ofApp::draw(){
    ofBackground(127);
    ofPushStyle();
    ofSetColor(64, 64 , 64, 200);
    ofRect(0, 0, guiMatch->getGlobalCanvasWidth(), ofGetHeight());
    ofPopStyle();
    if(videoLoaded && videoGood){
        ofPushStyle();
        ofSetColor(64, 64 , 64, 200);
        ofRect(guiMatch->getGlobalCanvasWidth(), 0, ofGetWidth() - guiMatch->getGlobalCanvasWidth(), ofGetHeight()- loopHeight);
        ofPopStyle();
        
        int firstLoop = loopsOnDisplay.at(0) - 1;
        int secondLoop = loopsOnDisplay.at(1) - 1;
        int thirdLoop = loopsOnDisplay.at(2) - 1;
        if (displayLoops.size() > firstLoop) {
            (*displayLoops.at(firstLoop).at(loopPlayIdx.at(firstLoop))).draw(drawLoopPoints.at(0));
        }
        if (displayLoops.size() > secondLoop) {
            (*displayLoops.at(secondLoop).at(loopPlayIdx.at(secondLoop))).draw(drawLoopPoints.at(1));
        }
        if (displayLoops.size() > thirdLoop) {
            (*displayLoops.at(thirdLoop).at(loopPlayIdx.at(thirdLoop))).draw(drawLoopPoints.at(2));
        }
        
        
        if (loopSelected >= 0){
            ofPushStyle();
            ofNoFill();
            ofSetColor(0, 255, 255);
            ofSetLineWidth(2);
            ofRect(loopDrawRects.at(loopSelected%3));
            ofPopStyle();
        }
        if (!savingGif){
            ofPushMatrix();
            ofTranslate(0, timeline.getBottomLeft().y);
            timeVid.draw(vidRect);
            ofPopMatrix();
        }
        
    }
    else if(!videoLoaded && videoGood){
        string loadVidInstruct = "No video loaded. Please click 'Load Video' and choose a valid video file.";
        float width = font.getStringBoundingBox(loadVidInstruct, 0, 0).width;
        font.drawString(loadVidInstruct, (guiMatch->getGlobalCanvasWidth() + ofGetWidth())/2 - width/2, 3*ofGetHeight()/4);
        guiLoops->setVisible(false);
    }
    else if(!videoGood && !videoLoaded){
        string loadVidInstruct = "The video loaded is not the proper format.\nPlease click 'Load Video' and choose a valid video file.";
        float width = font.getStringBoundingBox(loadVidInstruct, 0, 0).width;
        font.drawString(loadVidInstruct, (guiMatch->getGlobalCanvasWidth() + ofGetWidth())/2 - width/2, 3*ofGetHeight()/4);
        guiLoops->setVisible(false);
    }
    if(videoGood){
        timeline.draw();
        guiMatch->setPosition(0, timeline.getBottomLeft().y);
        guiLoops->setPosition(guiMatch->getGlobalCanvasWidth(), timeline.getBottomLeft().y+vidRect.getHeight());
        guiLoops->setWidth(ofGetWidth()-guiMatch->getGlobalCanvasWidth());
        guiLoops->setHeight(ofGetHeight()-loopHeight - vidRect.getHeight() - timeline.getBottomLeft().y);
        guiLoops->setVisible(true);
    }
    else{
        guiLoops->setVisible(false);
    }
    
}



//------------------------------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
}


//------------------------------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    if (!refiningLoop) {
        for (int i = 0; i < loopDrawRects.size(); i++) {
            if (loopDrawRects.at(i).inside(x, y)) {
                loopSelected = loopPage*3 + i;
                if (loopSelected >= displayLoops.size()) {
                    loopSelected = -1;
                    return;
                }
                instructions = "Press 's' to save loop as a GIF. Press 'r' to keep your changes to the loop. Press 'd' to delete loop.";
                setGuiInstructions();
                setGuiLoopStatus();
            }
        }
    }
}

//------------------------------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
}


//------------------------------------------------------------------------------------
void ofApp::saveGif(int i){
    savingGif = true;
    bool timeWasPlaying = timeline.getIsPlaying();
    if (timeWasPlaying) {
        timeline.stop();
    }
    ofFileDialogResult result = ofSystemSaveDialog(outputPrefix + ".gif", "Save Loop as Gif");
    if(result.bSuccess){
        
        ofxGifEncoder *giffy = new ofxGifEncoder();
        (*giffy).setup(vidPlayer.getWidth(), vidPlayer.getHeight(), 1/fps, 256);
        int currFrame = vidPlayer.getCurrentFrame();
        //int saveStartFrame = (int)(loopIndeces.at(i).x);
        int saveStartFrame = (loopIndeces.at(i))[0];
        cout << "saveStartFrame: " << saveStartFrame << endl;
        //int saveEndFrame = (int)(loopIndeces.at(i).y);
        int saveEndFrame = (loopIndeces.at(i))[1];
        cout << "saveEndFrame: " << saveEndFrame << endl;
        
        ofQTKitDecodeMode decodeMode = OF_QTKIT_DECODE_PIXELS_ONLY;
        ofQTKitPlayer frameSaver;
        frameSaver.loadMovie(videoFilePath,decodeMode);
        frameSaver.setSynchronousSeeking(true);
        //FIX THIS!!-----------------------------------------------------
        //int currFrameNum = vidPlayer.getCurrentFrame();
        frameSaver.play();
        frameSaver.setPaused(true);
        frameSaver.setFrame(saveStartFrame-1);
        frameSaver.update();
        frameSaver.nextFrame();
        frameSaver.update();
        
        while (frameSaver.getCurrentFrame() <= saveEndFrame) {
            (*giffy).addFrame(frameSaver.getPixels(), frameSaver.getWidth(),frameSaver.getHeight());
            frameSaver.nextFrame();
            frameSaver.update();
        }
        frameSaver.closeMovie();
        frameSaver.close();
        string gifFileName = result.getPath();
        if (gifFileName.size() >= 3  && gifFileName.substr(gifFileName.size() - 4) != ".gif")
            gifFileName += ".gif";
        else if (gifFileName.size() <= 3)
            gifFileName += ".gif";
        cout << "gifFileName: " << gifFileName << endl;
        (*giffy).save(gifFileName);
        gifNum++;
        gifses.push_back(giffy);
        
        gifSaveThreadRunning = true;
        setGuiInstructions();
    }
    
    if (timeWasPlaying) {
        timeline.play();
    }
    savingGif = false;
}


//-------------------------------------------------------------------
void ofApp::onGifSaved(string &fileName) {
    gifSaveThreadRunning = false;
    cout << "gif saved as " << fileName << endl;
    (*gifses.at(gifses.size() -1 )).exit();
    setGuiInstructions();
}


//-------------------------------------------------------------------
void ofApp::exit() {
    thread.waitForThread(); //FIXXX
    for (int i = 0; i < gifses.size(); i++) {
        (*gifses.at(i)).exit();
    }
    for(int i = 0; i < threads.size(); i++){
        threads.at(i)->stopThread();
    }
    if(videoLoaded){
        delete guiMatch;
    }
}


//-------------------------------------------------------------------
void ofApp::keyPressed(int key){
    if(guiMatch->hasKeyboardFocus())
    {
        return;
    }
	switch (key)
	{
        case 's':
            if (loopSelected >=0 && !refiningLoop){
                //timeline.stop();
                saveGif(loopSelected);
            }
            break;
        case 'r':
            if (loopSelected >=0 && !refiningLoop) {
                timeline.stop();
                frameBeforeRefine = timeVid.getCurrentFrame();
                refineLoop();
            }
            else if(refiningLoop){
                timeline.stop();
                timeline.clearInOut();
                timeline.getZoomer()->setViewRange(oldRange);
                refiningLoop = false;
                updateRefinedLoop();
                timeVid.setFrame(frameBeforeRefine);
                update();
                instructions = "Press 's' to save loop as a GIF. Press 'r' to keep your changes to the loop. Press 'd' to delete loop.";
                setGuiInstructions();
                needToInitEnds = true;
            }
            break;
        case 'd':
            if (!refiningLoop && loopSelected >= 0) {
                deleteLoop();
            }
            break;
        case 'n':
            if (refiningLoop){
                timeline.stop();
                //ofRange inOutTotal = ofRange(0.0,1.0);
                //timeline.setInOutRange(inOutTotal);
                timeline.clearInOut();
                timeline.getZoomer()->setViewRange(oldRange);
                instructions = "Press 's' to save loop as a GIF. Press 'r' to keep your changes to the loop. Press 'd' to delete loop.";
                setGuiInstructions();
                timeVid.setFrame(frameBeforeRefine);
                timeVid.update();
                needToInitEnds = true;
                refiningLoop = false;
            }
            break;
        case ' ':
            if (timeline.getIsPlaying()){
                pauseInstruct = "Press the spacebar to run. ";
            }
            else{
                pauseInstruct = "Press the spacebar to pause. ";
            }
            setGuiInstructions();
            break;
        case 'o':
            if(refiningLoop){
                if (tempLoopIndeces[1] > tempLoopIndeces[0]+1){
                    tempLoopIndeces[1] --;
                    int end = tempLoopIndeces[1];
                    timeline.setOutPointAtFrame(end);
                    timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                    if (!timeline.getIsPlaying()) {
                        timeVid.setFrame(end);
                        timeVid.update();
                    }
                    setGuiLoopStatus();
                }
            }
            break;
        case 'p':
            if(refiningLoop){
                tempLoopIndeces[1] ++;
                int end = tempLoopIndeces[1];
                timeline.setOutPointAtFrame(end);
                timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                if (!timeline.getIsPlaying()) {
                    timeVid.setFrame(end);
                    timeVid.update();
                }
                setGuiLoopStatus();
            }
            break;
        case 'k':
            if(refiningLoop){
                    tempLoopIndeces[0] --;
                    int start = tempLoopIndeces[0];
                    timeline.setInPointAtFrame(start);
                    timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                    if (!timeline.getIsPlaying()) {
                        timeVid.setFrame(start);
                        timeVid.update();
                    }
                    setGuiLoopStatus();
            }
            break;
        case 'l':
            if(refiningLoop){
                if (tempLoopIndeces[0] < tempLoopIndeces[1]-1){
                    tempLoopIndeces[0] ++;
                    int start = tempLoopIndeces[0];
                    timeline.setInPointAtFrame(start);
                    timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                    if (!timeline.getIsPlaying()) {
                        timeVid.setFrame(start);
                        timeVid.update();
                    }
                    setGuiLoopStatus();
                }
            }
            break;
		default:
			break;
	}
}


//------------------------------------------------------------------------------------
void ofApp::refineLoop(){
    oldRange = timeline.getZoomer()->getViewRange();
    refiningLoop = true;
    int start = (loopIndeces.at(loopSelected))[0];
    int end = (loopIndeces.at(loopSelected))[1];
    tempLoopIndeces[0] = start;
    tempLoopIndeces[1] = end;
    timeVid.setFrame(start);
    timeVid.update();
    timeline.setInPointAtFrame(start);
    timeline.setOutPointAtFrame(end);
    timeline.getZoomer()->setViewRange(timeline.getInOutRange());
    instructions = "Press 'r' to keep your changes to the loop. Press 'n' to cancel and go back. Press 'o' and 'p' to move the end one frame back or forward. Press 'k' and 'l' to move the beginning one frame back or forward.";
    setGuiInstructions();
}


//------------------------------------------------------------------------------------
void ofApp::updateRefinedLoop(){
    int prevFrame = vidPlayer.getCurrentFrame();
    vector<ofImage*> display;
    vidPlayer.setPaused(true);
    vector<int> indices;
    indices.push_back(tempLoopIndeces[0]);
    indices.push_back(tempLoopIndeces[1]);
    vector< vector<int> >::iterator itIn = loopIndeces.begin() + loopSelected;
    itIn = loopIndeces.erase(itIn);
    loopIndeces.insert(itIn,indices);
    
    int startIndx = (loopIndeces.at(loopSelected))[0];
    if (startIndx > 0) {
        vidPlayer.setFrame(startIndx - 1);
        vidPlayer.update();
        vidPlayer.nextFrame();
    }
    else
        vidPlayer.setFrame(startIndx);
    
    vidPlayer.update();
    cv::Mat startGray;
    cv::cvtColor(toCv(vidPlayer), startGray, CV_RGB2GRAY);
    cv::Mat start;
    cv::resize(startGray, start, imResize,INTER_AREA);
    cv::Scalar startMean = cv::mean(startMean);
    cv::subtract(start,startMean,start);
    float startSum = cv::sum(start)[0] + 1;
    
    while(vidPlayer.getCurrentFrame() <= (loopIndeces.at(loopSelected))[1]){
        ofImage loopee;
        loopee.setFromPixels(vidPlayer.getPixels(), vidPlayer.getWidth(), vidPlayer.getHeight(), OF_IMAGE_COLOR);
        loopee.update();
        ofImage *displayIm = new ofImage();
        displayIm->clone(loopee);
        displayIm->resize(loopWidth,loopHeight);
        displayIm->update();
        display.push_back(displayIm);
        vidPlayer.nextFrame();
        vidPlayer.update();
    }
    
    vidPlayer.previousFrame();
    vidPlayer.update();
    cv::Mat endGray;
    cv::cvtColor(toCv(vidPlayer),endGray,CV_RGB2GRAY);
    cv::Mat end;
    cv::resize(endGray, end, imResize);
    cv::subtract(end, startMean, end);
    cv::Mat diff;
    cv::absdiff(start, end, diff);
    float endDiff = cv::sum(diff)[0] + 1;
    float changeRatio = endDiff/startSum;
    
    vidPlayer.setFrame(prevFrame);
    vidPlayer.update();
    
    vector<int>::iterator it = loopLengths.begin()+loopSelected;
    it = loopLengths.erase(it);
    loopLengths.insert(it, display.size());
    vector<vector<ofImage*> >::iterator it2 = displayLoops.begin()+loopSelected;
    for (int i = 0; i < displayLoops.at(loopSelected).size(); i++) {
        delete displayLoops.at(loopSelected).at(i);
    }
    it2 = displayLoops.erase(displayLoops.begin()+loopSelected);
    displayLoops.insert(it2, display);
    vector<int>::iterator it3 = loopPlayIdx.begin()+loopSelected;
    it3 = loopPlayIdx.erase(it3);
    loopPlayIdx.insert(it3, 0);
    vector<float>::iterator it4 = loopQuality.begin()+loopSelected;
    it4 = loopQuality.erase(it4);
    loopQuality.insert(it4, changeRatio);
    vector<cv::Mat>::iterator it5 = loopStartMats.begin()+loopSelected;
    it5 = loopStartMats.erase(it5);
    loopStartMats.insert(it5, start);
    //loopLengths.at(loopSelected) = display.size();
    //displayLoops.at(loopSelected) = display;
    //loopPlayIdx.at(loopSelected) = 0;
    //loopQuality.at(loopSelected) = changeRatio;
    //loopStartMats.at(loopSelected) = start;
}

void ofApp::deleteLoop(){
    if (loopSelected < 0) {
        return;
    }
    vector<int>::iterator it = loopLengths.begin()+loopSelected;
    it = loopLengths.erase(it);
    vector<vector<ofImage*> >::iterator it2 = displayLoops.begin()+loopSelected;
    for (int i = 0; i < displayLoops.at(loopSelected).size(); i++) {
        delete displayLoops.at(loopSelected).at(i);
    }
    it2 = displayLoops.erase(displayLoops.begin()+loopSelected);
    vector<int>::iterator it3 = loopPlayIdx.begin()+loopSelected;
    it3 = loopPlayIdx.erase(it3);
    vector<float>::iterator it4 = loopQuality.begin()+loopSelected;
    it4 = loopQuality.erase(it4);
    vector<cv::Mat>::iterator it5 = loopStartMats.begin()+loopSelected;
    it5 = loopStartMats.erase(it5);
    vector<vector<int> >::iterator it6 = loopIndeces.begin()+loopSelected;
    it6 = loopIndeces.erase(it6);
    loopSelected = -1;
    if (loopPage*3 >= displayLoops.size() && loopPage > 0) {
        loopPage--;
    }
    loopsOnDisplay.at(0) = loopPage*3 + 1;
    loopsOnDisplay.at(1) = loopPage*3 + 2;
    loopsOnDisplay.at(2) = loopPage*3 + 3;
    loopsFoundLabel->setTextString("                              Number of Loops Found: " + ofToString(displayLoops.size()));
    loopsIndexLabel->setTextString("                            Current Loops Displayed: " + ofToString(loopsOnDisplay.at(0)) + " - " + ofToString(loopsOnDisplay.at(2)));
    setGuiLoopStatus();
}


//------------------------------------------------------------------------------------
void ofApp::keyReleased(int key){
}


//------------------------------------------------------------------------------------
//GUI STUFF
void ofApp::guiEvent(ofxUIEventArgs &e)
{
	string name = e.getName();
    if(name == "Load Video"){
		ofxUIButton *button = (ofxUIButton *) e.getButton();
        if (button->getValue()){
            timeline.stop();
            thread.waitForThread();
            loopFound = false;
            
            if (displayLoops.size() > 0)
                ofSystemAlertDialog("Loading a new video will delete all previously found loops.");
            
            ofFileDialogResult result = ofSystemLoadDialog("Choose a Video File",false);
            if(result.bSuccess){
                videoLoaded = false;
                int numLoops = displayLoops.size();
                for (int i = 0 ; i < numLoops; i++) {
                    loopSelected = 0;
                    deleteLoop();
                }
                loadVideo(result.getPath(),result.getName());
            }
        }
	}
    else if (name == "Toggle Min Movement"){
        minMovementBool = e.getToggle()->getValue();
        minMove->setVisible(minMovementBool);
    }
    else if (name == "Toggle Max Movement"){
        maxMovementBool = e.getToggle()->getValue();
        maxMove->setVisible(maxMovementBool);
    }
    else if(name == "Min Movement") {
        minMovementThresh = e.getSlider()->getValue();
    }
    else if(name == "Max Movement") {
        maxMovementThresh = e.getSlider()->getValue();
    }
    else if(name == "Length Range"){
        if(loopLengthSlider->getState() != OFX_UI_STATE_DOWN){
            timeline.stop();
            minPeriod = (int)(fps*minPeriodSecs);
            maxPeriod = (int)(fps*maxPeriodSecs);
        }
    }
    else if(name == "<<Page Left<<"){
        ofxUIButton *button = (ofxUIButton *) e.getButton();
        if (button->getValue() && loopPage > 0) {
            instructions = "";
            setGuiInstructions();
            loopSelected = -1;
            loopPage--;
            loopsOnDisplay.at(0) = loopPage*3 + 1;
            loopsOnDisplay.at(1) = loopPage*3 + 2;
            loopsOnDisplay.at(2) = loopPage*3 + 3;
            loopsIndexLabel->setTextString("                            Current Loops Displayed: " + ofToString(loopsOnDisplay.at(0)) + " - " + ofToString(loopsOnDisplay.at(2)));
            setGuiLoopStatus();
        }
    }
    else if(name == ">>Page Right>>"){
        ofxUIButton *button = (ofxUIButton *) e.getButton();
        if (button->getValue() && loopPage*3 + 3 < displayLoops.size()) {
            instructions = "";
            setGuiInstructions();
            loopSelected = -1;
            loopPage++;
            loopsOnDisplay.at(0) = loopPage*3 + 1;
            loopsOnDisplay.at(1) = loopPage*3 + 2;
            loopsOnDisplay.at(2) = loopPage*3 + 3;
            loopsIndexLabel->setTextString("                            Current Loops Displayed: " + ofToString(loopsOnDisplay.at(0)) + " - " + ofToString(loopsOnDisplay.at(2)));
            setGuiLoopStatus();
        }
    }
    
}


//------------------------------------------------------------------------------------
void ofApp::setGuiInstructions(){
    

    if (gifSaveSpacer != NULL) {
        guiMatch->removeWidget(gifSaveSpacer);
        gifSaveSpacer = NULL;
        guiMatch->removeWidget(gifSaveStatusLabel);
        gifSaveStatusLabel = NULL;
    }

    
    instructLabel->setTextString(pauseInstruct + instructions);
    instructLabel->stateChange();
    instructLabel->update();
    instructLabel->setVisible(true);
    if (gifSaveThreadRunning) {
        gifSaveSpacer = guiMatch->addSpacer("gifSaveSpacer");
        gifSaveSpacer->setVisible(true);
        gifSaveStatusLabel = guiMatch->addLabel("Gif Save Status", "Gif Saving. Don't exit.");
        gifSaveSpacer->setVisible(true);
    }
    
    guiMatch->update();
}


//------------------------------------------------------------------------------------
void ofApp::setGuiLoopStatus(){
    int loopIn;
    int loopOut;
    if (loopSelected < 0) {
        string loopStatusString = " ";
        loopStatsLabelTime->setTextString(loopStatusString);
        loopStatsLabelFrame->setTextString(loopStatusString);
        guiLoops->update();
        return;
    }
    
    if (refiningLoop) {
        loopIn = tempLoopIndeces[0];
        loopOut = tempLoopIndeces[1];
    }
    else{
        loopIn = loopIndeces.at(loopSelected).at(0);
        loopOut = loopIndeces.at(loopSelected).at(1);
    }
    
    string inTime = timeline.getTimecode().timecodeForFrame(loopIn);
    string outTime = timeline.getTimecode().timecodeForFrame(loopOut);
    string inFrame = ofToString(loopIn);
    string outFrame = ofToString(loopOut);
    string loopStatusFrameString = "                     Frame In: " + inFrame + "                Frame Out: " + outFrame;
    string loopStatusTimeString = "                  Time In: " + inTime + "         Time Out: " + outTime;

    loopStatsLabelTime->setTextString(loopStatusTimeString);
    loopStatsLabelFrame->setTextString(loopStatusFrameString);
    guiLoops->update();

}




//------------------------------------------------------------------------------------
void ofApp::setGuiMatch(){
	if (guiMatch != NULL) {
        guiMatch->~ofxUISuperCanvas();
    }
    
	guiMatch = new ofxUISuperCanvas("Loop Settings");
    
    guiMatch->addSpacer();
    guiMatch->addLabel("Load Video", OFX_UI_FONT_SMALL);
    guiMatch->addLabelButton("Load Video", false);
    
    guiMatch->addSpacer();
    guiMatch->addLabel("Valid Loop Lengths",OFX_UI_FONT_SMALL);
    loopLengthSlider = guiMatch->addRangeSlider("Length Range", 0.25, 8.0, &minPeriodSecs, &maxPeriodSecs);
    loopLengthSlider->setTriggerType(OFX_UI_TRIGGER_ALL);
    loopLengthSlider->setValueHigh(maxPeriodSecs);
    loopLengthSlider->setValueLow(minPeriodSecs);
    
    guiMatch->addSpacer();
	guiMatch->addLabel("Match Slider (percent)",OFX_UI_FONT_SMALL);
	ofxUISlider *match = guiMatch->addSlider("Accuracy", 70.0, 100.0, &loopThresh);
    match->setValue(loopThresh);
    match->setTriggerType(OFX_UI_TRIGGER_ALL);
    
	guiMatch->addSpacer();
    guiMatch->addLabel("Min Movement (% change)",OFX_UI_FONT_SMALL);
    guiMatch->addToggle( "Toggle Min Movement", &minMovementBool);
    minMove = guiMatch->addSlider("Min Movement", 0.0, 100.0, &minMovementThresh);
    minMove->setValue(minMovementThresh);
    minMove->setTriggerType(OFX_UI_TRIGGER_ALL);
    minMove->setVisible(minMovementBool);
    
    guiMatch->addSpacer();
    guiMatch->addLabel("Max Movement (% change)",OFX_UI_FONT_SMALL);
    guiMatch->addToggle("Toggle Max Movement", &maxMovementBool);
	maxMove = guiMatch->addSlider("Max Movement", 0.0, 100.0, &maxMovementThresh);
    maxMove->setValue(maxMovementThresh);
    maxMove->setTriggerType(OFX_UI_TRIGGER_ALL);
    
    guiMatch->addSpacer();
    guiMatch->addLabel("Instructions");
    pauseInstruct = "Press the spacebar to run.";
    instructions = "";
    instructLabel = guiMatch->addTextArea("Instructions", pauseInstruct + instructions, OFX_UI_FONT_SMALL);
    
    gifSaveSpacer = guiMatch->addSpacer("gifSaveSpacer");
    gifSaveSpacer->setVisible(false);
    gifSaveStatusLabel = guiMatch->addLabel("Gif Save Status", "Gif Saving. Don't exit.");
    gifSaveStatusLabel->setVisible(false);
    
    guiMatch->setHeight(ofGetHeight());
    maxMove->setVisible(maxMovementBool);
    
    guiMatch->setDrawOutline(true);
    
    guiMatch->update();
	ofAddListener(guiMatch->newGUIEvent,this,&ofApp::guiEvent);
    
}



//------------------------------------------------------------------------------------
void ofApp::setGuiLoops(){
    if (guiLoops != NULL) {
        guiLoops->~ofxUISuperCanvas();
    }
    guiLoops = new ofxUISuperCanvas("");
    guiLoops->setWidth(ofGetWidth()-guiMatch->getGlobalCanvasWidth());
    
    guiLoops->addSpacer();
    loopsFoundLabel = guiLoops->addTextArea("Number of Loops Found", "                              Number of Loops Found: " + ofToString(displayLoops.size()));
    loopsIndexLabel = guiLoops->addTextArea("Loop Page", "                            Current Loops Displayed: " + ofToString(loopsOnDisplay.at(0)) + " - " + ofToString(loopsOnDisplay.at(2)));
    guiLoops->addSpacer();
    pageLeft = false;
    pageRight = false;
    guiLoops->addLabelButton("<<Page Left<<", &pageLeft);
    guiLoops->addLabelButton(">>Page Right>>" ,&pageRight);
    guiLoops->addSpacer();
    guiLoops->addLabel("                                    Loop Info");
    loopStatsLabelFrame = guiLoops->addTextArea("Frame Loop Stats", " ");
    loopStatsLabelTime = guiLoops->addTextArea("Loop Stats Time", " ");
    
    guiLoops->setDrawOutline(true);
    ofAddListener(guiLoops->newGUIEvent, this, &ofApp::guiEvent);
}


//--------------------------------------------------------------
void ofApp::loadVideo(string videoPath, string videoName){
    int lastindex = videoName.find_last_of(".");
    outputPrefix = videoName.substr(0, lastindex);
    
    
    //TIMELINE STUFF
    timeline.clear();
    timeline.setup();
    timeline.removeFromThread();
    timeline.setAutosave(false);
    timeline.setEditableHeaders(false);
    timeline.setFootersHidden(true);

    timeline.setShowTicker(false);
    timeline.setShowBPMGrid(false);
    timeline.clearInOut();
    timeline.getZoomer()->setViewExponent(1);
    //set big initial duration, longer than the video needs to be
	timeline.setDurationInFrames(20000);
	timeline.setLoopType(OF_LOOP_NORMAL);
    timeline.stop();
    timeline.setFrameBased(true);
    
    loopSelected = -1;
    
    cout << "vidPath: " << videoPath << endl;
    cout << "vidName: " << videoName << endl;
    if(videoTrack == NULL){
	    videoTrack = timeline.addVideoTrack(videoName, videoPath);
        videoLoaded = (videoTrack != NULL);
        videoTrack = timeline.getVideoTrack(videoName);
    }
    else{
        videoLoaded = videoTrack->load(videoPath);
        videoTrack->setName(videoName);
    }
    if(videoLoaded){
        
        float vidHeight = std::min(videoTrack->getPlayer()->getHeight(),360.0f);
        float scaleRatio = videoTrack->getPlayer()->getHeight()/vidHeight;
        int vidWidth = videoTrack->getPlayer()->getWidth()/scaleRatio;
        vidRect = ofRectangle(guiMatch->getGlobalCanvasWidth() + (ofGetWidth() - guiMatch->getGlobalCanvasWidth())/2 - vidWidth/2, 0, vidWidth, vidHeight);
        frameBuffer.allocate(vidRect.getWidth(),vidRect.getHeight(),GL_RGB);
        
        timeline.setDurationInFrames(videoTrack->getPlayer()->getTotalNumFrames());
        cout << "totes frames: " << videoTrack->getPlayer()->getTotalNumFrames() << endl;
        cout << timeline.getDurationInFrames() << endl;
        timeline.setTimecontrolTrack(videoTrack); //video playback will control the time
        videoTrack->getPlayer()->setVolume(0);
        videoTrack->getPlayer()->stop();
        timeVid = *(videoTrack->getPlayer());
        
        
        //Checking Format
        timeVid.setPaused(true);
        timeVid.setFrame((int)(timeVid.getTotalNumFrames()/2));
        timeVid.update();
        if (!timeVid.isFrameNew()){
            videoGood = false;
            videoLoaded = false;
            videoTrack->clear();
            timeVid.close();
            timeVid.closeMovie();
            timeline.stop();
            timeline.clear();
            pauseInstruct = "";
            instructions = "Load a different video file.";
            setGuiInstructions();
            newVideo = false;
            return;
        }
        else{
            pauseInstruct = "Press the spacebar to run.";
            setGuiInstructions();
            videoGood = true;
        }
        timeVid.setFrame(0);
        timeVid.update();
        frameStart = timeVid.getCurrentFrame();
        timeline.setFrameBased(true);
        
        //scale = 10;
        //imResize = cv::Size(videoTrack->getPlayer()->getWidth()/scale,videoTrack->getPlayer()->getHeight()/scale);
        imResize = cv::Size(64,64);
        guiMatch->setPosition(0, timeline.getBottomLeft().y);
        ofQTKitDecodeMode decodeMode = OF_QTKIT_DECODE_PIXELS_ONLY;
        
        vidPlayer.loadMovie(videoPath,decodeMode);
        threadPlayer.loadMovie(videoPath,decodeMode);
        vidPlayer.setSynchronousSeeking(true);
        threadPlayer.setSynchronousSeeking(true);
        
        vidPlayer.play();
        threadPlayer.play();
        vidPlayer.setPaused(true);
        threadPlayer.setPaused(true);
        videoFilePath = videoPath;
        
        fps = (float)vidPlayer.getTotalNumFrames()/vidPlayer.getDuration();
        minPeriod = (int)(fps*minPeriodSecs);
        maxPeriod = (int)(fps*maxPeriodSecs);
        loopWidth = (ofGetWidth()-guiMatch->getGlobalCanvasWidth())/numLoopsInRow;
        loopHeight = vidRect.getHeight()*(loopWidth/vidRect.getWidth());
        if (loopHeight > 153) {
            ofSetWindowShape(ofGetWidth(),ofGetHeight()+(loopHeight-152));
            setGuiMatch();
            setGuiLoops();
        }
        else{
            ofSetWindowShape(1024, 768);
        }
        
        ofPoint firstPoint = ofPoint(guiMatch->getGlobalCanvasWidth(), ofGetHeight()-loopHeight);
        ofPoint secondPoint = ofPoint(guiMatch->getGlobalCanvasWidth()+loopWidth, ofGetHeight()-loopHeight);
        ofPoint thirdPoint = ofPoint(guiMatch->getGlobalCanvasWidth()+2*loopWidth, ofGetHeight()-loopHeight);
        drawLoopPoints.clear();
        drawLoopPoints.push_back(firstPoint);
        drawLoopPoints.push_back(secondPoint);
        drawLoopPoints.push_back(thirdPoint);
        loopDrawRects.clear();
        loopDrawRects.push_back(ofRectangle(firstPoint,loopWidth,loopHeight));
        loopDrawRects.push_back(ofRectangle(secondPoint,loopWidth,loopHeight));
        loopDrawRects.push_back(ofRectangle(thirdPoint,loopWidth,loopHeight));
        
        ofSetFrameRate(fps);
        
        refiningLoop = false;
        needToInitEnds = true;
        timeVid.setFrame(0);
        timeVid.update();
        newVideo = true;
        savingGif = false;
        timeline.clearInOut();
        finishedVideo = false;
    }
    else{
        videoPath = "";
        
    }
}

//--------------------------------------------------------------
void ofApp::playStopped(ofxTLPlaybackEventArgs& bang){
    pauseFrameNum = timeline.getCurrentFrame();
    cout << "paused" << endl;
}

//--------------------------------------------------------------
void ofApp::playStarted(ofxTLPlaybackEventArgs& bang){
    finishedVideo = false;
    if (timeline.getCurrentFrame() != pauseFrameNum){
        cout << "playhead changed" << endl;
        needToInitEnds = true;
    }
}

//--------------------------------------------------------------
void ofApp::playScrubbed(ofxTLPlaybackEventArgs& bang){
    cout << "playhead Scrubbed" << endl;
    //timeline.stop();
    needToInitEnds = true;
}

//--------------------------------------------------------------
void ofApp::playLooped(ofxTLPlaybackEventArgs& bang){
    if (timeline.getInOutRange() == ofRange(0.0,1.0)) {
        finishedVideo = true;
        needToInitEnds = true;
    }
}


//--------------------------------------------------------------
void ofApp::foundLoop(loopFoundEventArgs& loopArgs){
    loopFound = true;
    //thread.waitForThread();
}


//--------------------------------------------------------------
void ofApp::addLoopFrames(){
    thread.waitForThread(); ///FIXXXX
    thread.lock();
    ditchSimilarLoop();
    vector<ofImage*> lastLoop = displayLoops.at(displayLoops.size() - 1);
    for (int i = 0; i < lastLoop.size(); i++) {
        (displayLoops.at(displayLoops.size() - 1).at(i))->setUseTexture(true);
        (displayLoops.at(displayLoops.size() - 1).at(i))->reloadTexture();
    }
    loopFound = false;
    thread.unlock();
    loopsFoundLabel->setTextString("                              Number of Loops Found: " + ofToString(displayLoops.size()));
    guiLoops->update();
    
}

