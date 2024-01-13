//
// interaction with the whisperd (or the OpenAI service)
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

// setup culr
var wdCurl = new CURL('http://127.0.0.1:8080/v1/audio/transcriptions', 'POST', 30);
wdCurl.authType = 'bearer';
wdCurl.credentials = 'secret123';
wdCurl.contentType = 'multipart/form-data';

// ivs
var ivs = new IVS(session);
//ivs.silenceTimeout = 5; // sec
ivs.ttsEngine = "piper";
ivs.language = "en";
ivs.cngEnable = true;
ivs.dtmfMaxDigits = 2;

if(!ivs.captureStart('audio', 'file', 'mp3')) {
    consoleLog('err', "Couldn't start cepture!");
    exit();
}

var flSayHello = false;
var asrFiles = {};

while(!script.isInterrupted()) {
    if(!session.isReady) { break; }

    if(!flSayHello) {
	ivs.say("Hello, How can I help you?");
	flSayHello = true;
    }

    // check responses from the whisperd
    var curlEvent = wdCurl.getAsyncResult();
    if(curlEvent) {
	var oid = 'asr' + curlEvent['jid'];
	if(asrFiles[oid]) { unlink(asrFiles[oid]); delete(asrFiles[oid]); }

	// parse the response
        if(curlEvent.code == 200) {
	    var wobj = JSON.parse(curlEvent.body);
    	    if(wobj) {
		if(wobj['text']) {
		    consoleLog('notice', "TRANSCRIPTION-RESULT: [" + wobj.text + "]");
		} else if(wobj['error']) {
		    consoleLog('err', "TRANSCRIPTION-ERROR: [" + wobj.error.message + "]");
		}
	    }
	} else {
    	    consoleLog('err', "HTTP-ERROR: [" + curlEvent.body + "]");
	}
    }

    // send chucnks to the whisperd
    var ivsEvent = ivs.getEvent();
    if(ivsEvent) {
	if(ivsEvent.type == 'audio-chunk-ready') {
    	    var jid = wdCurl.performAsync({type: 'simple', name: 'model', value: 'whisper-1'}, {type: 'file', name: 'file', value: ivsEvent.data.file} );
    	    if(jid) { asrFiles['asr' + jid] = ivsEvent.data.file; }
	} else if(ivsEvent.type == 'dtmf-buffer-ready') {
    	    consoleLog('notice', "DTMF-INPUT: [" + ivsEvent.data.digits + "]");
	}
    }

    msleep(100);
}

