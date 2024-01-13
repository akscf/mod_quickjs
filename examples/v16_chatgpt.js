//
// interaction with openai services
//
if(typeof(session) == 'undefined') {
    consoleLog('error', "session not found!");
    exit();
}
if(!session.isAnswered) {
    session.answer();
}
if(!session.isReady) {
    consoleLog('err', "session isn't ready");
    exit();
}

var wspCurl = new CURL('https://api.openai.com/v1/audio/transcriptions', 'POST', 30);
wspCurl.authType = 'bearer';
wspCurl.credentials = '---YOUR-API-KEY---';
wspCurl.contentType = 'multipart/form-data';

var gptCurl = new CURL('https://api.openai.com/v1/chat/completions', 'POST', 30);
gptCurl.authType = 'bearer';
gptCurl.credentials = '---YOUR-API-KEY---';
gptCurl.contentType = 'application/json';


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
    if(!session.isReady) { 
	break; 
    }

    if(!flSayHello) {
	ivs.say("Hello, How can I help you?");
	flSayHello = true;
    }

    // whisper responses
    var wspCurlEvent = wspCurl.getAsyncResult();
    if(wspCurlEvent) {
	var oid = 'asr' + wspCurlEvent['jid'];
	if(asrFiles[oid]) { unlink(asrFiles[oid]); delete(asrFiles[oid]); }

	if(wspCurlEvent.code == 200) {
    	    var wobj = JSON.parse(wspCurlEvent.body);
            if(wobj) {
		if(wobj['text']) {
		    consoleLog('notice', "TEXT: [" + wobj.text + "]");

		    // send this to the openai-chat
		    var gptData = {"model": "gpt-3.5-turbo-16k", "messages": [{"role": "user", "content": wobj.text}]};
		    var jid = gptCurl.performAsync(JSON.stringify(gptData));

		} else if(wobj['error']) {
		    consoleLog('err', "TRANSCRIPTION-FAILED: [" + wobj.error.message + "]");
		}		
    	    }
	} else {
	    consoleLog('err', "HTTP-ERROR (" + wspCurlEvent.code + ") - [" + wspCurlEvent.body + "]");
	}
    }

    // chat responses
    var gptCurlEvent = gptCurl.getAsyncResult();
    if(gptCurlEvent) {
	if(gptCurlEvent.code == 7) {
	    var rspObj = JSON.parse(gptCurlEvent.body);
	    if(rspObj['choices']) {
		var tobj = rspObj['choices'][0];
		if(tobj && tobj['message']) {
    		    var txt = tobj['message']['content'];
    		    if(txt) {
			consoleLog("notice", "GPT-TXT: [" + txt + "]");
			ivs.ivsSay(txt, true);
		    }
		}
	    }
	} else {
	    consoleLog('err', "HTTP-ERROR (" + gptCurlEvent.code + ") - [" + gptCurlEvent.body + "]");
	}
    }

    // audio chunks
    var ivsEvent = ivs.getEvent();
    if(ivsEvent && ivsEvent.type == 'audio-chunk-ready') {
	var jid = wspCurl.performAsync({type: 'simple', name: 'model', value: 'whisper-1'}, {type: 'file', name: 'file', value: ivsEvent.data.file} );
	if(jid) { asrFiles['asr' + jid] = ivsEvent.data.file; }

    }

    msleep(100);
}

