//
// new properties for v1.5
//

if(typeof(session) == 'undefined') {
    consoleLog('err', "session not found!");
    exit();
}

consoleLog('notice', "session.isAudioCaptureActive........: " + session.isAudioCaptureActive);
consoleLog('notice', "session.isVideoCaptureActive........: " + session.isVideoCaptureActive);

consoleLog('notice', "session.ivsAudioChunkSec............: " + session.ivsAudioChunkSec);	// captured chunk max time in seconds
consoleLog('notice', "session.ivsAudioChunkType...........: " + session.ivsAudioChunkType);	// file or buffer
consoleLog('notice', "session.ivsAudioChunkEncoding.......: " + session.ivsAudioChunkEncoding);	// wav, mp3, b64, raw

consoleLog('notice', "session.ivsVadVoiceMs...............: " + session.ivsVadVoiceMs);		// VAD settings (see manual for the Freeswitch)
consoleLog('notice', "session.ivsVadSilenceMs.............: " + session.ivsVadSilenceMs);
consoleLog('notice', "session.ivsVadThreshold.............: " + session.ivsVadThreshold);
consoleLog('notice', "session.ivsVadDebug.................: " + session.ivsVadDebug);

consoleLog('notice', "session.ivsCng......................: " + session.ivsCng);		// comfort noises for the caller on some long-time operations
consoleLog('notice', "session.ivsCngLevel.................: " + session.ivsCngLevel);		// level

consoleLog('notice', "session.ivsDtmfMaxDigits............: " + session.ivsDtmfMaxDigits);	// DTMF grabber
consoleLog('notice', "session.ivsDtmMaxIdle...............: " + session.ivsDtmfMaxDigits);


