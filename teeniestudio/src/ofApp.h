// Photo Recorder & Sender for the Teenie Studio Projection Event
//
// Using Christopher Baker's ofxSMTP
//

#pragma once


#include "ofMain.h"
#include "ofxSMTP.h"
#include "ofxGui.h"
#include "ofxSyphon.h"


class ofApp: public ofBaseApp
{
public:
    void setup();
    void update();
    void draw();
    void exit();
    void keyPressed(int key);

    // ------- EMAIL -------
    
    void onSMTPDelivery(ofx::SMTP::Message::SharedPtr& message);
    void onSMTPException(const ofx::SMTP::ErrorArgs& evt);

    void onSSLClientVerificationError(Poco::Net::VerificationErrorArgs& args);
    void onSSLPrivateKeyPassphraseRequired(std::string& passphrase);

    // set emails
    string recipientEmail = "teeniestudio@gmail.com";
    string senderEmail = "teeniestudio@gmail.com";

    ofx::SMTP::Client smtp;

    // ------- SYPHON -------

    ofxSyphonServer mainServer; // of debug screen
    ofxSyphonServer projServer; // image projected on wall in front of users
    ofxSyphonServer deskServer; // screen on desktop users interact with
    
    ofTexture projTex;          // texture to project
    int projWidth = 1024;
    int projHeight = 768;
    ofTexture deskTex;          // texture to send to interface
    int deskWidth = 832;
    int deskHeight = 624;
    
    // ------- VIDEO -------
    
    ofVideoGrabber vidGrabber;
    int deviceIndex = 1;
    ofPixels videoPixels;
    ofTexture videoTexture;
    int camWidth = 640;//1080;
    int camHeight = 480;//720;
    
    // projection screen
    
    bool bFlagNewPhoto = false;
    uint64_t lastPhotoTime = 0;     // ms
    uint64_t debounceTime = 500;    // ms
    
    string newPhotoName = "~newphoto.jpg";
    
    bool bNewPhotoAnimate = false;
    
    uint64_t pFrameZero;
    int whiteFadeLength = 100; // frames
    
    float photoTilt;
    float maxTilt = 15.;
    int frameWidth = 8;
    ofColor frameColor = ofColor(255);
    
    ofImage img;
    float projImgScale = 0.7;
    
    int holdImgLength = 100;    // frames, in addition to fade
    int fallImgLength = 120;     // frames
    int blackFadeLength = 60;
    
    // desktop interface
    
    ofImage sendImg;
    float sendImgTilt;
    float tiltMult = 0.8;
    
    bool bFlagInterface = false;
    bool bInterfaceOn = false;
    
    uint64_t iFrameZero;
    int descendLength = 120;
    
    float deskImgScale = 0.5;
    
    int emailWaitLength = 100;
    int emailInLength = 30;
    
    ofTrueTypeFont lfont;
    int lFontHt = 35;
    ofTrueTypeFont sfont;
    int sFontHt = 15;
    
    bool bSendPhoto = false;
    
    bool bOutSeq = false;
    uint64_t startOutFrame;
    int outLength = 80;
    float outTilt;
    
    
    // -------- GUI --------
    
    ofxPanel panel;
    ofParameterGroup appCtrl;
    ofParameter<bool> bDebug;
    ofParameter<bool> bSmooth; // enable smoothing
    
    
};
