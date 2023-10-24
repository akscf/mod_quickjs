var HANGUP = false;

function hangup_hook(state) {
    console_log('notice', "HANGUP-HOOK: state=" + state);
    HANGUP = true;
}

if(typeof(session) != 'undefined') {
    console_log('notice', "session.uuid..........: [" + session.uuid + "]");
    console_log('notice', "session.ready.........: [" + session.isReady + "]");
    console_log('notice', "session.answered......: [" + session.isAnswered + "]");
    console_log('notice', "session.mediaReady....: [" + session.isMediaReady + "]");
    console_log('notice', "session.samplerate....: [" + session.samplerate + "]");
    console_log('notice', "session.channels......: [" + session.channels + "]");
    console_log('notice', "session.ptime.........: [" + session.ptime + "]");

    session.setHangupHook(hangup_hook);

    if(!session.isAnswered) {
        session.answer();
    }

    if(!session.isReady) {
        console_log('error', "session isn't ready");
        exit();
    }

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
        if(!session.isReady) { break; }
        var dlen = session.frameRead(srcBuf);
        if(dlen > 0) {
            dlen = rcodec.decode(srcBuf, dlen, rcodec.samplerate, tmpBuf, rcodec.samplerate);
            if(dlen > 0) {
                if(vad_hits > 0) {
                    dlen = wcodec.encode(tmpBuf, dlen, rcodec.samplerate, dstBuf, wcodec.samplerate);
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
}

console_log('notice', "***************** script finished *****************");
