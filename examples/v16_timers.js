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

var ivs = new IVS(session);
ivs.silenceTimeout = 5;
ivs.ttsEngine = "piper";
ivs.language = "en";

if(!ivs.timersStart()) {
    consoleLog('err', "Couldn't start timers!");
    exit();
}

if(!ivs.captureStart('audio', 'file', 'mp3')) {
    consoleLog('err', "Couldn't start cepture!");
    exit();
}

ivs.say("Hello, How can I help you?");

ivs.timerSetup(1, 10, 'once');
ivs.timerSetup(2, 5);

while(!script.isInterrupted()) {
    if(!session.isReady) { break; }

    var ivsEvent = ivs.getEvent();
    if(ivsEvent) {
        if(ivsEvent.type == 'silence-timeout') {
            ivs.say("Please, ask your question.", true);
        } else if(ivsEvent.type == 'audio-chunk-ready') {
            ivs.playback(ivsEvent.data.file, true, true);
        } else if(ivsEvent.type == 'timer-timeout') {
	    consoleLog('notice', "timer-timeout: " + ivsEvent.data.timer);	    
        } 
	
    }

    msleep(100);
}

if(ivs) {
    ivs.destroy();
}
