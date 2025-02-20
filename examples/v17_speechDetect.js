if(typeof(session) == 'undefined') {
    consoleLog('err', "Sessions is undefined");
    exit();
}

session.ttsEngine= 'piper';
session.asrEngine= 'openai';

//var txt = session.playAndDetectSpeech('/tmp/test.wav', 10);
var txt = session.sayAndDetectSpeech('Hello, how can I help you?', 10);
consoleLog('info', "TEXT: " + txt);

