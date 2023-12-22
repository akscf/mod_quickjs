//
// playback captured fragments
//

if(typeof(session) == 'undefined') {
    consoleLog('err', "session not found!");
    exit();
}

if(!session.isAnswered) {
    session.answer();
}
if(!session.isReady) {
    consoleLog('err', "session isn't ready");
    exit();
}


var err = session.ivsCaptureStart('audio', 'file', 'mp3');
if(!err) {
    consoleLog('notice', "Couldn't start cepture!");
    exit();
}

var flSayHello = false;

while(!script.isInterrupted()) {
    if(!session.isReady) {
        break;
    }

    if(!flSayHello) {
        session.ivsPlayback('/opt/freeswitch/share/freeswitch/sounds/en/us/callie/conference/8000/conf-welcome.wav', false, true);
        flSayHello = true;
    }

    var ivsEvent = session.ivsGetEvent();
    if(ivsEvent && ivsEvent.type == 'audio-chunk-ready') {
        session.ivsPlayback(ivsEvent.data.file, true, true);
    }

    msleep(10);
}

