var HANGUP = false;

function hangup_hook(state) {
    console_log('notice', "HANGUP-HOOK: state=" + state);
    HANGUP = true;
}

function input_callback(session, event, type, udata) {
    console_log('notice', "INPUT-CALLBACK: type: [" + type + "], user_data: [" + udata + "]");

    if(session) {
        console_log('notice', "INPUT-CALLBACK: session.uuid: [" + session.uuid + "]");
    }

    if(type == 'dtmf') {
        console_log('notice', "INPUT-CALLBACK: dtmf: [" + event.digit + "]");
    }

    return true;
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

//---------------------------------------------------------------------------------------------------
console_log('notice', "***************** test begin *****************");

console_log('notice', "script name...: [" + script.name + "]");
console_log('notice', "script id.....: [" + script.instance_id + "]");

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

    // REC-PLAY
    if(0) {
        console_log('notice', "****** REC-PLAY TEST:  session.recordFile() / session.streamFile()");

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

        console_log('notice', "****** REC-PLAY TEST:  DONE)");
    }

    // echo + simple vad
    if(1) {
        console_log('notice', "****** VAD TEST:  session.readFrame() / session.writeFrame()");

        var rcodec = session.getReadCodec();
        var wcodec = session.getWriteCodec();

        console_log('notice', "******** read codec.....: [" + rcodec.name + "] , canEncode: [" + rcodec.canEncode + "] , canDecode: [" + rcodec.canDecode + "] ");
        console_log('notice', "******** write codec....: [" + wcodec.name + "] , canEncode: [" + wcodec.canEncode + "] , canDecode: [" + wcodec.canDecode + "] ");

        var vad_hits = 0;
        var srcBuf = new ArrayBuffer(4096);
        var dstBuf = new ArrayBuffer(4096);
        var tmpBuf = new ArrayBuffer(4096);
        var tbuf16 = new Int16Array(tmpBuf);

        while(true) {
            var dlen = session.frameRead(srcBuf);
            if(dlen > 0) {
                dlen = rcodec.decode(srcBuf, rcodec.samplerate, dlen, tmpBuf, rcodec.samplerate);
                if(dlen > 0) {
                    if(vad_hits > 0) {
                        dlen = wcodec.encode(tmpBuf, rcodec.samplerate, dlen, dstBuf, wcodec.samplerate);
                        if(dlen > 0) {
                            session.frameWrite(dstBuf, dlen);
                        }
                        vad_hits--;
                    } else {
                        var j = 0, lvl = 0, samples = (dlen / 2);
                        for(var i = 0; i < samples; i++) {
                            j += Math.abs(tbuf16[i]);
                        }
                        lvl = Math.ceil(j / samples);
                        if(lvl > 160) {
                            vad_hits = 45;
                            console_log('notice', "VAD level: [" + lvl + "]");
                        }
                    }
                }
            }
            session.sleep(10);
        }
        console_log('notice', "****** VAD TEST:  DONE");
    }

}

console_log('notice', "***************** test end *****************");
