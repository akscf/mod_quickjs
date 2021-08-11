var HANGUP = false;

function hangup_hook(state) {
    console_log('notice', "HANGUP-HOOK: state=" + state);
    HANGUP = true;
}

function playback_callback(filehandle, event, type, udata) {
    console_log('notice', "PLAYBACK-CALLBACK: type: [" + type + "], user_data: [" + udata + "]");

    if(!filehandle) {
        console_log('error', "PLAYBACK-CALLBACK: filehandle == NULL");
        return;
    }

    if(type == 'dtmf') {
        console_log('notice', "PLAYBACK-CALLBACK: dtmf: [" + event.digit + "]");

        if(event.digit == '1') {
            filehandle.volume = '+1';
        } else if(event.digit == '2') {
            filehandle.volume = '-1';
        } else if(event.digit == '3') {
            filehandle.volume = 0;
        } else if(event.digit == '4') {
            filehandle.speed = '+1';
        } else if(event.digit == '5') {
            filehandle.speed = '-1';
        } else if(event.digit == '6') {
            filehandle.speed = 0;
        } else if(event.digit == '*') {
            filehandle.pause();
        } else if(event.digit == '#') {
            return false;
        }
    }
    return true;
}

if(typeof(session) != 'undefined') {
    console_log('notice', "session.uuid..........: [" + session.uuid + "]");
    console_log('notice', "session.ready.........: [" + session.isReady + "]");
    console_log('notice', "session.answered......: [" + session.isAnswered + "]");
    console_log('notice', "session.mediaReady....: [" + session.isMediaReady + "]");

    session.setHangupHook(hangup_hook);

    if(!session.isAnswered) {
        session.answer();
    }

    if(!session.isReady) {
        console_log('error', "session isn't ready");
        exit();
    }

    var recFile = new File('/tmp/rec_XXXXXX.wav');
    if(!recFile.mktemp()) {
        console_log('error', "Couldn't create temporary file");
        exit();
    }
    console_log('notice', "TEST: file.name: [" + recFile.name + "]");
    console_log('notice', "TEST: file.path: [" + recFile.path + "]");

    console_log('notice', "TEST:  session.recordFile(), file: [" + recFile.path + "]");
    //session.recordFile(recFile.path, playback_callback, 'record');
    session.recordFile(recFile, playback_callback, 'record', 10);

    console_log('notice', "TEST: session.streamFile(), file: [" + recFile.path + "]");
    //session.streamFile(recFile.path, playback_callback, 'playback');
    session.streamFile(recFile, playback_callback, 'playback');

}

console_log('notice', "***************** script finished *****************");
