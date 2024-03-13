//
//
//
consoleLog('info', "*** Daling: user/1000");

var dialString = "{ignore_early_media=true,call_timeout=10,origination_caller_id_name=ivs}user/1000";
var ses = new Session(dialString);

//globals.amd_call_answer_time = microTime();

if(!ses.isReady) {
    consoleLog('err', "Couldn't make outboud call: " + ses.cause);
    exit();
}

var ivs = new IVS(ses);
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

var helloDone = false;
ivs.timerSetup(1, 3, 'once');

while(!script.isInterrupted()) {
    if(!session.isReady) { break; }

    var ivsEvent = ivs.getEvent();
    if(ivsEvent) {
        if(ivsEvent.type == 'silence-timeout') {
	    if(helloDone) { ivs.say("Hellooo"); }

        } else if(ivsEvent.type == 'audio-chunk-ready') {
            ivs.playback(ivsEvent.data.file, true, true);

        } else if(ivsEvent.type == 'timer-timeout') {
	    if(ivsEvent.data.timer == 1) {
		ivs.say("Hello, could you answer a couple of questions?");
		helloDone = true;
	    }
        }	
    }

    msleep(100);
}

if(ivs) {
    ivs.destroy();
}
