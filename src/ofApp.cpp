 #include "ofApp.h"

using namespace cv;
using namespace ofxCv;

void ofApp::setup(){
    ofSetWindowTitle("LoopFindr");
    
    videoLoaded = false;
    videoGood = true;
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
    setGuiMatch();
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
    
    //ofSetWindowShape(guiMatch->getGlobalCanvasWidth() + vidPlayer.getWidth(), vidPlayer.getHeight()*2);
    ofSetBackgroundColor(127);
    cout<< "frameRate: " << ofGetFrameRate() << endl;
    //frameStart = 0;
    //initEnds();
    //pausePlayback = true;
    //scrolling = false;
    //dragPoint = ofPoint(0,0);
    
    ofAddListener(timeline.events().playbackEnded, this, &ofApp::playStopped);
    ofAddListener(timeline.events().playbackStarted, this, &ofApp::playStarted);
    ofAddListener(timeline.events().playheadScrubbed, this, &ofApp::playScrubbed);
    ofAddListener(thread.loopEvents.loopFoundEvent,this,&ofApp::foundLoop);
    
    //guiMatch->loadSettings("guiMatchSettings.xml");
    /*if (videoLoaded) {
        loopWidth = (ofGetWidth()-guiMatch->getGlobalCanvasWidth())/numLoopsInRow;
        loopHeight = vidPlayer.getHeight()*(loopWidth/vidPlayer.getWidth());
        drawLoopPoint = ofPoint(guiMatch->getGlobalCanvasWidth() ,timeline.getBottomLeft().y + vidPlayer.getHeight() + loopHeight);
    }*/
    //needToInitEnds = true;
}


//------------------------------------------------------------------------------------
void ofApp::initEnds(){
    if (potentialLoopEnds.size() > 0){
        potentialLoopEnds.clear();
    }

    cout << "frameStart: " << frameStart << endl;
    vidPlayer.setPaused(true);
    vidPlayer.setFrame(frameStart);
    vidPlayer.update();
    
    for (int i = frameStart; i < vidPlayer.getTotalNumFrames() && i < frameStart + maxPeriod; i++) {
        cout << "init ends frame: " << vidPlayer.getCurrentFrame() << endl;
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
    //thread.waitForThread();
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
/*bool ofApp::checkForLoop(){

    if (potentialLoopEnds.size() == 0)
        return false;
    
    cv::Mat start;
    potentialLoopEnds.at(0).copyTo(start);
    cv::Scalar startMean = cv::mean(start);
    cv::subtract(start, startMean, start);
    float startSum = cv::sum(start)[0] + 1; //add one to get rid of division by 0 if screen is black
    cv::Mat currEnd;
    for (int i = minPeriod; i < potentialLoopEnds.size(); i++) {
        cv::Mat currEnd;
        potentialLoopEnds.at(i).copyTo(currEnd);
        cv::subtract(currEnd, startMean, currEnd);
        cv::Mat diff;
        cv::absdiff(start, currEnd, diff);
        float endDiff = cv::sum(diff)[0] + 1;
        float changeRatio = endDiff/startSum;
        
        
        if (changeRatio < minChangeRatio) {
            minChangeRatio = changeRatio;
            //cout << "min Ratio: " << minChangeRatio << endl;
        }
        
        cout << "frameStart: " << frameStart << endl;
        cout << "compare Frame: " << frameStart + i << endl;
        cout << "startSum: " << startSum << endl;
        cout << "endSum: " << cv::sum(currEnd)[0] + 1 << endl;
        cout << "endDiff: " << endDiff << endl;
        cout << "change Ratio: " << changeRatio*100 << endl;
        cout << "test Thresh: " << 100 - loopThresh << endl;
        cout << endl;
        
        if ((changeRatio*100) < (100 - loopThresh)){//set to change percentage
            potentialEndIdx = frameStart + i;
            //cout << "potentialEnd: " << potentialEndIdx << endl;
            if (minMovementBool || maxMovementBool){
                if  (hasMovement()){
                    matchIndeces.push_back(potentialEndIdx);
                    bestMatches.push_back(currEnd);
                }
                else{
                    //timeline.setCurrentFrame(potentialEndIdx);
                    //frameStart = potentialEndIdx;
                    //timeline.setCurrentFrame(frameStart);
                    //vidPlayer.setFrame(timeline.getCurrentFrame());
                    vidPlayer.setFrame(frameStart);
                    vidPlayer.update();
                    return false;
                }
            }
            else{
                matchIndeces.push_back(potentialEndIdx);
                bestMatches.push_back(currEnd);
            }
        }
    }
    
    if (bestMatches.size() > 0) {
        vidPlayer.setFrame(frameStart);
        vidPlayer.update();
        getBestLoop(start,startMean);
        vidPlayer.setFrame(frameStart);
        vidPlayer.update();
        return true;
    }
    
    vidPlayer.setFrame(frameStart);
    vidPlayer.update();
    return false;
}

//------------------------------------------------------------------------------------
bool ofApp::hasMovement(){
    float sumDiff = 0;
    
    cv::Mat prevFrame;
    potentialLoopEnds.at(0).copyTo(prevFrame);
    cv::Scalar prevMean = cv::mean(prevFrame); //DO MEAN INSIDE LOOP?
    cv::subtract(prevFrame,prevMean,prevFrame);
    float prevSum = cv::sum(prevFrame)[0] + 1; //add one to get rid of division by 0 if screen is black

    cv::Mat currFrame;
    
    //for (int i = 1; i <= potentialEndIdx - timeline.getCurrentFrame(); i++) {
    for (int i = 1; i <= potentialEndIdx - frameStart; i++) {
        cv::Mat currFrame;
        potentialLoopEnds.at(i).copyTo(currFrame);
        cv::subtract(currFrame, prevMean, currFrame);
        cv::Mat diff;
        cv::absdiff(currFrame,prevFrame,diff);
        float endDiff = cv::sum(diff)[0] + 1;
        float changeRatio = endDiff;///prevSum;
        //cout << "movement Change Ratio: " << changeRatio << endl;
        sumDiff += changeRatio;
        prevFrame = currFrame;
        //prevSum = cv::sum(prevFrame)[0] + 1;
    }
    sumDiff /= prevSum;
    //cout << "sum Diff: " << sumDiff << endl;
    //cout << "minThreshold: " << minMovementThresh*(potentialEndIdx - frameStart)/100 << endl;
    if (minMovementBool) {
        //if(sumDiff < minMovementThresh*(potentialEndIdx - frameStart)/100 ){
        if(sumDiff < minMovementThresh/100 ){
            cout << "not enough movement" << endl;
            return false;
        }
    }
    else if (maxMovementBool){
        //if(sumDiff > maxMovementThresh*(potentialEndIdx - frameStart)/100 ){
        if(sumDiff > maxMovementThresh/100 ){
            cout << "too much movement" << endl;
            return false;
        }
    }
    else if (minMovementBool && maxMovementBool){
        //if(sumDiff > maxMovementThresh*(potentialEndIdx - frameStart)/100 && sumDiff < minMovementThresh*(potentialEndIdx - frameStart)/100){
        if(sumDiff > maxMovementThresh/100 && sumDiff < minMovementThresh/100){
            cout << "not enough movement or too much" << endl;
            return false;
        }
    }
    
    return true;
}

//------------------------------------------------------------------------------------
void ofApp::getBestLoop(cv::Mat start,cv::Scalar startMean){
    float minChange = MAXFLOAT;
    int bestEnd = -1;
    float startSum = cv::sum(start)[0] + 1; //add one to get rid of division by 0 if screen is black
    
    for (int i = 0; i < bestMatches.size(); i++) {
        cv::Mat currEnd;
        potentialLoopEnds.at(i).copyTo(currEnd);
        cv::subtract(currEnd,startMean,currEnd);
        cv::Mat diff;
        cv::absdiff(start, currEnd, diff);
        float endDiff = cv::sum(diff)[0] + 1;
        float changeRatio = endDiff/startSum;
        
        //cout << "end of loop frame: " << matchIndeces.at(i) << endl;
        //add flow test here
        if (changeRatio <= minChange) {
            minChange = changeRatio;
            bestEnd = i;
        }
    }
    
    //ofQTKitDecodeMode decodeMode = OF_QTKIT_DECODE_PIXELS_ONLY;
    //ofQTKitPlayer loopFrameGrabber;
    //loopFrameGrabber.loadMovie(videoFilePath,decodeMode);
    if (bestEnd >= 0) {
        vector< ofImage> display;
        //vidPlayer.idleMovie();
        vidPlayer.setPaused(true);
        vidPlayer.setFrame(frameStart);
        cout << "Updating Vid Player" << endl;
        vidPlayer.update();
        
        for (int j = frameStart; j <= matchIndeces.at(bestEnd); j++) {
            //loopFrameGrabber.setFrame(j);
            //loopFrameGrabber.update();
            //vidPlayer.setFrame(j);
            //vidPlayer.update();
            ofImage loopee;
            loopee.setFromPixels(vidPlayer.getPixels(), vidPlayer.getWidth(), vidPlayer.getHeight(), OF_IMAGE_COLOR);
            //loopee.setFromPixels(loopFrameGrabber.getPixels(), loopFrameGrabber.getWidth(), loopFrameGrabber.getHeight(), OF_IMAGE_COLOR);
            loopee.update();
            ofImage displayIm;
            displayIm.clone(loopee);
            displayIm.resize(loopWidth,loopHeight);
            //displayIm.resize(vidPlayer.getWidth()/numLoopsInRow, vidPlayer.getHeight()/numLoopsInRow);
            displayIm.update();
            //displayIm.resize(loopFrameGrabber.getWidth()/numLoopsInRow, loopFrameGrabber.getHeight()/numLoopsInRow);
            display.push_back(displayIm);
            vidPlayer.nextFrame();
            cout << "Updating Vid Player" << endl;
            vidPlayer.update();
        }
        vidPlayer.setFrame(frameStart);
        cout << "Updating Vid Player" << endl;
        vidPlayer.update();
        //loopFrameGrabber.close();
        
        //ofVec2f indices = ofVec2f(frameStart, matchIndeces.at(bestEnd));
        //int indices[2] = {frameStart,matchIndeces.at(bestEnd)};
        vector<int> indices;
        indices.push_back(frameStart);
        indices.push_back(matchIndeces.at(bestEnd));
        loopIndeces.push_back(indices);
        loopLengths.push_back(display.size());
        displayLoops.push_back(display);
        loopQuality.push_back(minChange);
        //loopQuality.push_back(1 - (minChange/100));
        loopPlayIdx.push_back(0);
        loopStartMats.push_back(start);
        
        ditchSimilarLoop();
    }
    
    bestMatches.clear();
    matchIndeces.clear();
}*/


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
    cout << "changeRatio: " << changeRatio << endl;
    float changePercent = (changeRatio*100);
    cout << "changePercent: " << changePercent << endl;
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
    cout << "keeping loop" << endl;
    return false;

}

//------------------------------------------------------------------------------------
void ofApp::update(){
    if (loopFound) {
        addLoopFrames();
        //timeVid.setFrame(frameStart+minPeriod);
        //timeVid.update();
        //frameStart = timeVid.getCurrentFrame();
        //initEnds();
    }
    if (newVideo) {
        timeVid.setFrame(0);
        timeVid.update();
        newVideo = false;
    }
    //cout << "frameStart: " << frameStart << endl;
    //cout << "timeVid Frame: " << timeVid.getCurrentFrame() << endl;
    if (!refiningLoop && videoGood){
        frameStart = timeVid.getCurrentFrame();
        if (videoLoaded && timeline.getIsPlaying() && !needToInitEnds) {
            if (frameStart >= vidPlayer.getTotalNumFrames()){
                cout << "restarting?"<<endl;
                initEnds();
                return;
            }

            if (!thread.isThreadRunning()) {
                thread.lock();
                thread.setup(&loops, &loopIndeces, &displayLoops, &loopStartMats, &loopLengths, &loopPlayIdx, &loopQuality, loopWidth, loopHeight, minPeriod, maxPeriod, frameStart, &potentialLoopEnds, minMovementThresh, maxMovementThresh, loopThresh, minChangeRatio, minMovementBool, maxMovementBool, videoFilePath, &threadPlayer);
                thread.startThread(true,true);
                thread.unlock();
                thread.lock();
                populateLoopEnds();
                thread.unlock();
                timeVid.nextFrame();
                timeVid.update();
            }
        }
        else if (videoLoaded && needToInitEnds)
            initEnds();
        for (int i = 0; i < loopPlayIdx.size(); i++) {
            loopPlayIdx.at(i)++;
            if(loopPlayIdx.at(i) >= loopLengths.at(i))
                loopPlayIdx.at(i) = 0;
        }
    }
    else{
        needToInitEnds = false;
        if(timeline.getIsPlaying()){
            timeVid.nextFrame();
            timeVid.update();
        }
    }
    loopsFoundLabel->setTextString("                              Number of Loops Found: " + ofToString(displayLoops.size()));
    if(thread.isThreadRunning())
        guiLoops->getCanvasTitle()->setLabel("                                  Processing...");
    else
        guiLoops->getCanvasTitle()->setLabel("                                      Idle");
    guiLoops->update();
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
        guiLoops->setPosition(guiMatch->getGlobalCanvasWidth(), timeline.getBottomLeft().y+vidPlayer.getHeight());
        guiLoops->setWidth(ofGetWidth()-guiMatch->getGlobalCanvasWidth());
        guiLoops->setHeight(ofGetHeight()-loopHeight - vidPlayer.getHeight() - timeline.getBottomLeft().y);
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
                string changeString;
                instructions = "Press 's' to save loop as a GIF. Press 'r' to refine the loop";
                setGuiInstructions();
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
    }
    
    if (timeWasPlaying) {
        timeline.play();
    }
    savingGif = false;
}


//-------------------------------------------------------------------
void ofApp::onGifSaved(string &fileName) {
    cout << "gif saved as " << fileName << endl;
    (*gifses.at(gifses.size() -1 )).exit();
}


//-------------------------------------------------------------------
void ofApp::exit() {
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
                refineLoop();
            }
            else if(refiningLoop){
                timeline.stop();
                timeline.clearInOut();
                timeline.getZoomer()->setViewRange(oldRange);
                refiningLoop = false;
                updateRefinedLoop();
                needToInitEnds = true;
            }
            break;
        case 'n':
            if (refiningLoop){
                timeline.stop();
                //ofRange inOutTotal = ofRange(0.0,1.0);
                //timeline.setInOutRange(inOutTotal);
                timeline.clearInOut();
                timeline.getZoomer()->setViewRange(oldRange);
                instructions = "Press 's' to save loop as a GIF. Press 'r' to keep your changes to the loop.";
                setGuiInstructions();
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
                tempLoopIndeces[1] --;
                int end = tempLoopIndeces[1];
                timeline.setOutPointAtFrame(end);
                //videoTrack->setOutFrame(end);
                timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                if (!timeline.getIsPlaying()) {
                    timeVid.setFrame(end);
                    timeVid.update();
                }
            }
            break;
        case 'p':
            if(refiningLoop){
                tempLoopIndeces[1] ++;
                int end = tempLoopIndeces[1];
                timeline.setOutPointAtFrame(end);
                //videoTrack->setOutFrame(end);
                timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                if (!timeline.getIsPlaying()) {
                    timeVid.setFrame(end);
                    timeVid.update();
                }
            }
            break;
        case 'k':
            if(refiningLoop){
                tempLoopIndeces[0] --;
                int start = tempLoopIndeces[0];
                timeline.setInPointAtFrame(start);
                //videoTrack->setInFrame(start);
                timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                if (!timeline.getIsPlaying()) {
                    timeVid.setFrame(start);
                    timeVid.update();
                }
            }
            break;
        case 'l':
            if(refiningLoop){
                tempLoopIndeces[0] ++;
                int start = tempLoopIndeces[0];
                timeline.setInPointAtFrame(start);
                //videoTrack->setInFrame(start);
                timeline.getZoomer()->setViewRange(timeline.getInOutRange());
                if (!timeline.getIsPlaying()) {
                    timeVid.setFrame(start);
                    timeVid.update();
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
    //timeline.stop();
    timeline.setInPointAtFrame(start);
    timeline.setOutPointAtFrame(end);
    //videoTrack->setInFrame(start);
    //videoTrack->setOutFrame(end);
    timeline.getZoomer()->setViewRange(timeline.getInOutRange());
    instructions = "Press 'r' to keep your changes to the loop. Press 'n' to cancel and go back. Press 'o' and 'p' to move the end one frame back or forward. Press 'k' and 'l' to move the beginning one frame back or forward.";
    setGuiInstructions();
}


//------------------------------------------------------------------------------------
void ofApp::updateRefinedLoop(){
    int prevFrame = vidPlayer.getCurrentFrame();
    vector<ofImage*> display;
    //vidPlayer.idleMovie();
    vidPlayer.setPaused(true);
    //std::copy(tempLoopIndeces, tempLoopIndeces + 2, loopIndeces.at(loopSelected));
    vector<int> indices;
    indices.push_back(tempLoopIndeces[0]);
    indices.push_back(tempLoopIndeces[1]);
    //loopIndeces.at(loopSelected) = indices;
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
        //cout << "Updating Vid Player" << endl;
        vidPlayer.update();
        //potentialLoopEnds.at(i).copyTo(currEnd);
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


//------------------------------------------------------------------------------------
void ofApp::keyReleased(int key){
}


//------------------------------------------------------------------------------------
//GUI STUFF
void ofApp::guiEvent(ofxUIEventArgs &e)
{
	string name = e.getName();
    if(name == "Load Video"){
        videoLoaded = false;
		ofxUIButton *button = (ofxUIButton *) e.getButton();
        if (button->getValue()){
            timeline.stop();
            //pausePlayback = true;
            ofFileDialogResult result = ofSystemLoadDialog("Choose a Video File",false);
            if(result.bSuccess){
                //need to clear old data
                //timeline.removeTrack("Video");
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
            loopSelected = -1;
            loopPage--;
            loopsOnDisplay.at(0) = loopPage*3 + 1;
            loopsOnDisplay.at(1) = loopPage*3 + 2;
            loopsOnDisplay.at(2) = loopPage*3 + 3;
            loopsIndexLabel->setTextString("                            Current Loops Displayed: " + ofToString(loopsOnDisplay.at(0)) + " - " + ofToString(loopsOnDisplay.at(2)));
        }
    }
    else if(name == ">>Page Right>>"){
        ofxUIButton *button = (ofxUIButton *) e.getButton();
        if (button->getValue() && loopPage*3 + 3 < displayLoops.size()) {
            loopSelected = -1;
            loopPage++;
            loopsOnDisplay.at(0) = loopPage*3 + 1;
            loopsOnDisplay.at(1) = loopPage*3 + 2;
            loopsOnDisplay.at(2) = loopPage*3 + 3;
            loopsIndexLabel->setTextString("                            Current Loops Displayed: " + ofToString(loopsOnDisplay.at(0)) + " - " + ofToString(loopsOnDisplay.at(2)));
        }
    }
    
}


//------------------------------------------------------------------------------------
void ofApp::setGuiInstructions(){
    instructLabel->setTextString(pauseInstruct + instructions);
    instructLabel->stateChange();
    instructLabel->update();
    instructLabel->setVisible(true);
    guiMatch->update();
    //guiMatch->autoSizeToFitWidgets();
}


//------------------------------------------------------------------------------------
void ofApp::setGuiMatch(){
	
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
    
    guiMatch->setHeight(ofGetHeight());
    //guiMatch->autoSizeToFitWidgets();
    maxMove->setVisible(maxMovementBool);
	ofAddListener(guiMatch->newGUIEvent,this,&ofApp::guiEvent);
}

//------------------------------------------------------------------------------------
void ofApp::setGuiLoops(){
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
    //guiLoops->setWidth(ofGetWidth()-guiMatch->getGlobalCanvasWidth());
    //guiLoops->autoSizeToFitWidgets();
    guiLoops->setHeight(ofGetHeight()-loopHeight - vidPlayer.getHeight() - timeline.getBottomLeft().y);
    ofAddListener(guiLoops->newGUIEvent, this, &ofApp::guiEvent);
}


//--------------------------------------------------------------
void ofApp::loadVideo(string videoPath, string videoName){
    int lastindex = videoName.find_last_of(".");
    outputPrefix = videoName.substr(0, lastindex);
    
    
    //TIMELINE STUFF
    timeline.clear();
    //timeline.removeTrack(videoTrack);
    timeline.setup();
    timeline.removeFromThread();
    timeline.setAutosave(false);
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
        int vidWidth = videoTrack->getPlayer()->getWidth();
        int vidHeight = videoTrack->getPlayer()->getHeight();
        vidRect = ofRectangle(guiMatch->getGlobalCanvasWidth() + (ofGetWidth() - guiMatch->getGlobalCanvasWidth())/2 - vidWidth/2, 0, vidWidth, videoTrack->getPlayer()->getHeight());
        frameBuffer.allocate(vidRect.getWidth(),vidRect.getHeight(),GL_RGB);
        
        timeline.setDurationInFrames(videoTrack->getPlayer()->getTotalNumFrames());
        cout << "totes frames: " << videoTrack->getPlayer()->getTotalNumFrames() << endl;
        cout << timeline.getDurationInFrames() << endl;
        timeline.setTimecontrolTrack(videoTrack); //video playback will control the time
		//timeline.bringTrackToTop(videoTrack);
        videoTrack->getPlayer()->setVolume(0);
        videoTrack->getPlayer()->stop();
        timeVid = *(videoTrack->getPlayer());
        timeVid.setPaused(true);
        timeVid.setFrame((int)(timeVid.getTotalNumFrames()/2));
        timeVid.update();
        if (!timeVid.isFrameNew()){
            videoGood = false;
            videoLoaded = false;
            videoTrack->clear();
            timeVid.close();
            timeVid.closeMovie();
            //timeline.removeTrack(videoName);
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
        
        scale = 10;
        //imResize = cv::Size(videoTrack->getPlayer()->getWidth()/scale,videoTrack->getPlayer()->getHeight()/scale);
        imResize = cv::Size(64,64);
        guiMatch->setPosition(0, timeline.getBottomLeft().y);
        ofQTKitDecodeMode decodeMode = OF_QTKIT_DECODE_PIXELS_ONLY;
        
        vidPlayer.loadMovie(videoPath,decodeMode);
        threadPlayer.loadMovie(videoPath,decodeMode);
        vidPlayer.setSynchronousSeeking(true);
        threadPlayer.setSynchronousSeeking(true);
        cout << "Pix Format: " << vidPlayer.getPixelFormat() << endl;
        vidPlayer.play();
        threadPlayer.play();
        vidPlayer.setPaused(true);
        threadPlayer.setPaused(true);
        videoFilePath = videoPath;
        //pausePlayback = true;
        fps = (float)vidPlayer.getTotalNumFrames()/vidPlayer.getDuration();
        minPeriod = (int)(fps*minPeriodSecs);
        maxPeriod = (int)(fps*maxPeriodSecs);
        loopWidth = (ofGetWidth()-guiMatch->getGlobalCanvasWidth())/numLoopsInRow;
        loopHeight = vidPlayer.getHeight()*(loopWidth/vidPlayer.getWidth());
        
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
void ofApp::foundLoop(loopFoundEventArgs& loopArgs){
    loopFound = true;
    //addLoopFrames();
}


//--------------------------------------------------------------
void ofApp::addLoopFrames(){
    thread.waitForThread();
    thread.lock();
    ditchSimilarLoop();
    //if(!ditchSimilarLoop()){
        vector<ofImage*> lastLoop = displayLoops.at(displayLoops.size() - 1);
        for (int i = 0; i < lastLoop.size(); i++) {
            (*displayLoops.at(displayLoops.size() - 1).at(i)).setUseTexture(true);
            (*displayLoops.at(displayLoops.size() - 1).at(i)).reloadTexture();
        }
    //}
    loopFound = false;
    thread.unlock();
        
}

