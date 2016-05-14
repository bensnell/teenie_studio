// Photo Recorder & Sender for the Teenie Studio Projection Event
//
// Using Christopher Baker's ofxSMTP
//

#include "ofApp.h"


void ofApp::setup() {
    
    ofSetVerticalSync(true);
    ofSetWindowTitle("Teenie Studio Debug");
    
    ofEnableSmoothing();
//    ofDisableSmoothing();
    
    // ----------------- GUI -----------------

    appCtrl.setName("App Controls");
    appCtrl.add(bDebug.set("Debug", false));
    
    panel.setup();
    panel.add(appCtrl);
    
    
    // ------------- SETUP EMAIL -------------
    
    // Register for SSL Context events.
    ofSSLManager::registerClientEvents(this);

    // Load credentials and account settings from an xml file or element.
    ofx::SMTP::Settings settings = ofx::SMTP::Settings::loadFromXML("teeniestudio.xml");

    // Pass the settings to the client.
    smtp.setup(settings);

    // Register event callbacks for message delivery (or failure) events
    ofAddListener(smtp.events.onSMTPDelivery, this, &ofApp::onSMTPDelivery);
    ofAddListener(smtp.events.onSMTPException, this, &ofApp::onSMTPException);
    
    
    // ------------- SETUP VIDEO -------------
    
    //we can now get back a list of devices.
    vector<ofVideoDevice> devices = vidGrabber.listDevices();
    
    cout << "------- BEGIN DEVICE LIST -------" << endl;
    for(int i = 0; i < devices.size(); i++){
        if(devices[i].bAvailable){
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName;
        }else{
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName << " - unavailable ";
        }
    }
    cout << "------- END DEVICE LIST -------" << endl;
    
    vidGrabber.setDeviceID(deviceIndex); // 2 for webcam, 3 for facetime
    vidGrabber.setDesiredFrameRate(60);
    vidGrabber.initGrabber(camWidth, camHeight);
    
    videoPixels.allocate(camWidth, camHeight, OF_PIXELS_RGB);
    videoTexture.allocate(videoPixels);
    
    // ------------- SETUP SYPHON -------------

    mainServer.setName("Debug Screen");
    projServer.setName("Projection Screen");
    deskServer.setName("Desktop Screen");
    
    projTex.allocate(projWidth, projHeight, OF_PIXELS_RGBA);
    deskTex.allocate(deskWidth, deskHeight, OF_PIXELS_RGBA);
    
    lfont.load("fonts/CrimsonText-Semibold.ttf", lFontHt);
    sfont.load("fonts/CrimsonText-Semibold.ttf", sFontHt);
    
}


void ofApp::update() {
    
    if (bSmooth) {
        ofEnableSmoothing();
    } else {
        ofDisableSmoothing();
    }
    
    // ------- VIDEO -------
    
    // update video
    vidGrabber.update();
    if(vidGrabber.isFrameNew()) {
        // update pixels and textures
        videoPixels = vidGrabber.getPixels();
        videoTexture.loadData(videoPixels);
    }
    
    // save a new photo
    if (bFlagNewPhoto) {
        bFlagNewPhoto = false;
        
        // debounce the signal
        if (ofGetElapsedTimeMillis() - lastPhotoTime >= debounceTime) {
        
            // save photo to disk with timestamp
            img.clear();
            img.allocate(camWidth, camHeight, OF_IMAGE_COLOR);
            img.getPixels() = videoPixels;
            img.update();
            img.save("photos/teeniestudio_" + ofGetTimestampString() + ".png");
            
            // also save photo as the new photo name
            img.save("photos/" + newPhotoName);
            
            lastPhotoTime = ofGetElapsedTimeMillis();
            
            
            // what if a photo is taken while it's animating??
            
            
            // begin animating the photo
            bNewPhotoAnimate = true;
            pFrameZero = ofGetFrameNum();
            
            // set tilt of this photo
            photoTilt = ofRandom(-maxTilt, maxTilt); // degrees
        }
    }
    
    // ------ PROJECTION ------
    
    // project the live video feed continuously
    // when a photo is taken; make screen go white; then fade to the photo taken; animate it downwards on black background while turning; video fades in
    // make this texture available to millumin
    
    ofFbo projFbo;
    projFbo.allocate(projWidth, projHeight);
    projFbo.begin();
    ofBackground(0);
    
    // draw the projection
    if (bNewPhotoAnimate) {
        
        // calculate the frame number
        int thisFrame = ofGetFrameNum() - pFrameZero;
        
        // draw photo in frame, moving if applicable
        ofPushMatrix();
        ofTranslate(projWidth/2, projHeight/2);
        
        if (thisFrame > whiteFadeLength + holdImgLength) {
            // translate off screen
            float param = pow((float)(thisFrame - whiteFadeLength - holdImgLength)/float(fallImgLength) , 3);
            
            ofTranslate(0, param * projHeight * 1.25); // move it down
            
            ofRotate(- param * photoTilt * tiltMult); // center it a little
        }
        
        ofRotate(photoTilt);
        ofScale(projImgScale, projImgScale);
        ofSetColor(frameColor);
        ofDrawRectangle(-camWidth / 2 - frameWidth, -camHeight / 2 - frameWidth, camWidth + 2 * frameWidth, camHeight + 2 * frameWidth);
        img.draw(-camWidth / 2, -camHeight / 2, camWidth, camHeight);
        ofPopMatrix();
        
        // draw white fading overlay
        if (thisFrame < whiteFadeLength) {
            float fade = float(whiteFadeLength - thisFrame) / (float)whiteFadeLength * 255.;
            ofSetColor(255, int(fade));
            ofDrawRectangle(0, 0, projWidth, projHeight);
        }
        
        if (thisFrame > whiteFadeLength + holdImgLength + fallImgLength) {
            vidGrabber.draw(0, 0, projWidth, projHeight);
            
            float fade = float(blackFadeLength - (thisFrame - (whiteFadeLength + holdImgLength + fallImgLength)))/(float)blackFadeLength * 255.;
            ofSetColor(0, int(fade));
            ofDrawRectangle(0, 0, projWidth, projHeight);
        }
        
        if (thisFrame >= whiteFadeLength + holdImgLength + fallImgLength + blackFadeLength) {
            
            // reset animation
            bNewPhotoAnimate = false;
            
            // initialize the desktop interface animation sequence
            bFlagInterface = true;
        }
        
    } else {
        
        // just draw the live video (max size -- no margin)
        vidGrabber.draw(0, 0, projWidth, projHeight); // doesn't retain W:H ratio
    }
    
    projFbo.end();
    projTex = projFbo.getTexture();
    projServer.publishTexture(&projTex);
    
    
    // ------ DESKTOP INTERFACE ------
    
    // draw the last photo taken with a text box underneath to type an email (cleared each time) ("Email" written above) and a message to "Press Enter to Send"
    // make this texture available to millumin
    
    ofFbo deskFbo;
    deskFbo.allocate(deskWidth, deskHeight);
    deskFbo.begin();
    ofBackground(0);
    
    // draw the desktop interface
    if (bFlagInterface) {
        
        // initialize interface
        bFlagInterface = false;
        bInterfaceOn = true;
        iFrameZero = ofGetFrameNum();
        
        // store the image to be sent
        sendImg.clear();
        sendImg.allocate(camWidth, camHeight, OF_IMAGE_COLOR);
        sendImg.getPixels() = img.getPixels();
        sendImg.update();
        
        // store the tilt
        sendImgTilt = photoTilt * (1 - tiltMult);
        
        // reset recipient email
        recipientEmail = "";
        
    }
    
    if (bInterfaceOn) {
        
        int thisFrame = ofGetFrameNum() - iFrameZero;
        
        // if the out sequence, send everything off screen
        ofPushMatrix();
        if (bOutSeq) {
            // translate to the right
            float param = pow(float(ofGetFrameNum() - startOutFrame) / float(outLength), 2);
            
            ofTranslate(param * deskWidth * 1, 0);
            ofRotate(param * outTilt);
            
            // stop out
            if (thisFrame - startOutFrame == outLength) {
                bOutSeq = false;
                bInterfaceOn = false; // turn off interface
            }
        }
        
        // DRAW THE IMAGE
        
        ofPushMatrix();
        ofTranslate(deskWidth/2, deskHeight/2 * 0.8);
        if (thisFrame < descendLength) {
            
            // turn the photo and translate it down into place
            
            float param = pow(float(descendLength - thisFrame)/float(descendLength), 3);
            
            ofTranslate(0, - param * deskHeight * 1.25);
            
            ofRotate(2*(param - 0.5) * sendImgTilt);
            
            ofScale(deskImgScale, deskImgScale);
            ofSetColor(frameColor);
            ofDrawRectangle(-camWidth / 2 - frameWidth, -camHeight / 2 - frameWidth, camWidth + 2 * frameWidth, camHeight + 2 * frameWidth);
            sendImg.draw(-camWidth / 2, -camHeight / 2, camWidth, camHeight);
            
        } else {
            
            // draw slightly tilted still photo
            
            ofRotate(-sendImgTilt);
            ofScale(deskImgScale, deskImgScale);
            ofSetColor(frameColor);
            ofDrawRectangle(-camWidth / 2 - frameWidth, -camHeight / 2 - frameWidth, camWidth + 2 * frameWidth, camHeight + 2 * frameWidth);
            sendImg.draw(-camWidth / 2, -camHeight / 2, camWidth, camHeight);
            
        }
        ofPopMatrix();
        
        // DRAW THE TEXT FIELD
        
        if (thisFrame > emailWaitLength) {
            
            // draw email text field
            int px = deskWidth / 8;
            int py = deskHeight / 5 * 4 + 25;
            
            ofSetColor(255);
            
            // fade in
            if (thisFrame < emailWaitLength + emailInLength) {
                float fade = float(thisFrame - emailWaitLength) / float(emailInLength);
                ofSetColor(255, 255 * fade);
            }
            
            sfont.drawString("Type Your Email  (press enter to send)", px, py - 20);
            lfont.drawString(recipientEmail, px, py + 5 + lfont.getSize());
            
            // draw blinking line
            if (thisFrame / 30 % 2 == 0 && !bOutSeq) {
                ofRectangle rect = lfont.getStringBoundingBox(recipientEmail, 0, 0);
                ofDrawRectangle(px + rect.width + 5, py + 10, 2, lfont.getSize() + 15);
            }
        }
        
        // SEND THE EMAIL
        if (bSendPhoto) {
            bSendPhoto = false;
            
            // begin the out sequence
            bOutSeq = true;
            startOutFrame = ofGetFrameNum();
            outTilt = ofRandom(-15, 15);
            
            if (!recipientEmail.empty()) {
            
                // You can construct complex messages using poco's MailMessage object
                // See http://pocoproject.org/docs/Poco.Net.MailMessage.html
        
                /// SMTP::Message::SharedPtr simply wraps Poco::Net::MailMessage.
                ofx::SMTP::Message::SharedPtr message = ofx::SMTP::Message::makeShared();
        
                // Encode the sender and set it.
                message->setSender(Poco::Net::MailMessage::encodeWord(senderEmail, "UTF-8"));
                // Mark the primary recipient and add them.
                message->addRecipient(Poco::Net::MailRecipient(Poco::Net::MailRecipient::PRIMARY_RECIPIENT, recipientEmail));
        
                // Encode the subject and set it.
                message->setSubject(Poco::Net::MailMessage::encodeWord("Your Photo as Arrived!", "UTF-8"));
        
                // Poco::Net::MailMessage will take ownership of the *PartSource files,
                // so you don't have to worry about deleting the pointers.
                message->addContent(new Poco::Net::StringPartSource("Enjoy!\n-Teenie Studio"));
        
                // Poco::Net::MailMessage throws exceptions when a file is not found.
                // Thus, we need to add attachments in a try / catch block.
        
                try {
                    message->addAttachment(Poco::Net::MailMessage::encodeWord("photos/" + newPhotoName,"UTF-8"), new Poco::Net::FilePartSource(ofToDataPath("photos/" + newPhotoName)));
                }
                catch (const Poco::OpenFileException& exc)
                {
                    ofLogError("ofApp::keyPressed") << exc.name() << " : " << exc.displayText();
                }
        
                // Add an additional header, just because we can.
                message->add("X-Mailer", "ofxSMTP (https://github.com/bakercp/ofxSMTP)");
        
                // Add the message to our outbox.
                smtp.send(message);
                
                cout << "sending message..." << endl;
            }
        }
        
        ofPopMatrix();
        
    } else {
        
        // just draw black
        ofBackground(0);
    }
    
    deskFbo.end();
    deskTex = deskFbo.getTexture();
    deskServer.publishTexture(&deskTex);
    
    
    // caluclate size of windows
    
    
}

void ofApp::draw() {
    
    // clear with alpha to capture with syphon and composite elsewhere
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    ofBackground(30);
     
    // video
    ofSetColor(255);
    vidGrabber.draw(ofGetWidth() - camWidth / 3 - 20, 20, camWidth / 3, camHeight / 3);

    // projection
    projTex.draw(20, ofGetHeight() / 5 * 2, projWidth / 2, projHeight / 2);
    
    // desktop
    deskTex.draw(ofGetWidth() / 5 * 3 - 30, ofGetHeight() / 2, deskWidth / 2, deskHeight / 2);
    

    if (bDebug) {
        
        panel.draw();
        
        stringstream ss;
        ss << "Press '*' to toggle fullscreen\n";
        ss << ofToString(ofGetFrameRate()) << "\n";
        ss << "ofxSMTP: There are " + ofToString(smtp.getOutboxSize()) + " messages in your outbox.\n"; // outbox state
        ofDrawBitmapStringHighlight(ss.str(), 200, 20);
    }
    
    // publish screens
    mainServer.publishScreen();
    
}

void ofApp::exit() {
    
    // It is recommended to remove event callbacks, if the SMTP::Client will
    // outlast the listener class, so it is not absolutely required in this
    // case, but it is a good practice none-the-less.
    ofRemoveListener(smtp.events.onSMTPDelivery, this, &ofApp::onSMTPDelivery);
    ofRemoveListener(smtp.events.onSMTPException, this, &ofApp::onSMTPException);
}

void ofApp::keyPressed(int key) {
    
//    if (key == ' ') // Press spacebar for a simple send.
//    {
//        // simple send
//        smtp.send(recipientEmail, senderEmail, "Sent using ofxSMTP", "Hello world!");
//
//    }
    
    if (key == '*') ofToggleFullscreen();
    if (key == '&') bDebug = !bDebug;
    if (key == OF_KEY_UP) {
        bFlagNewPhoto = true; // take a new photo and begin the photo sequence
    }
    
    if (bInterfaceOn) {
        // add typed keys and delete last one with delete key
        if (key == 127) {
            if (recipientEmail.size() != 0) {
                recipientEmail.resize(recipientEmail.size() - 1);
            }
        } else {
            if (key >= 32 && key <= 126) { //  must be actual symbol
                recipientEmail = recipientEmail + ofToString((char)key);
            }
        }
        if (key == OF_KEY_RETURN) {
            bSendPhoto = true;
        }
    }
}

void ofApp::onSMTPDelivery(ofx::SMTP::Message::SharedPtr& message)
{
    ofLogNotice("ofApp::onSMTPDelivery") << "Message Sent: " << message->getSubject();
}


void ofApp::onSMTPException(const ofx::SMTP::ErrorArgs& evt)
{
    ofLogError("ofApp::onSMTPException") << evt.getError().displayText();

    if (evt.getMessage())
    {
        ofLogError("ofApp::onSMTPException") << evt.getMessage()->getSubject();
    }

}


void ofApp::onSSLClientVerificationError(Poco::Net::VerificationErrorArgs& args)
{
    ofLogNotice("ofApp::onClientVerificationError") << std::endl << ofToString(args);

    // If you want to proceed, you must allow the user to inspect the certificate
    // and set `args.setIgnoreError(true);` if they want to continue.

    // args.setIgnoreError(true);

}

void ofApp::onSSLPrivateKeyPassphraseRequired(std::string& passphrase)
{
    // If you want to proceed, you must allow the user to input the assign the private key's
    // passphrase to the `passphrase` argument.  For example:

    passphrase = ofSystemTextBoxDialog("Enter the Private Key Passphrase", "");

}
