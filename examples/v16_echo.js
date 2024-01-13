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
ivs.silenceTimeout = 5; // sec
ivs.ttsEngine = "piper";
ivs.language = "en";

if(!ivs.captureStart('audio', 'file', 'mp3')) {
    consoleLog('err', "Couldn't start cepture!");
    exit();
}

ivs.say("Hello, How can I help you?");

while(!script.isInterrupted()) {
    if(!session.isReady) { break; }

    var ivsEvent = ivs.getEvent();
    if(ivsEvent) {
        if(ivsEvent.type == 'silence-timeout') {
            ivs.say("Please, ask your question.", true);
        } else if(ivsEvent.type == 'audio-chunk-ready') {
            ivs.playback(ivsEvent.data.file, true, true);
        }
    }

    msleep(100);
}
