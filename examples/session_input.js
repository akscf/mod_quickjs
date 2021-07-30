/*
 * example of use session api's:
 *  collectInput, getDigits
 *
 */
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

        if(event.digit == '#') {
            return false;
        }
    }

    return true;
}


if(typeof(session) != 'undefined') {

    session.setHangupHook(hangup_hook);

    if(!session.isAnswered) {
        session.answer();
    }

    if(!session.isReady) {
        console_log('error', "session isn't ready");
        exit();
    }

    console_log('notice', "TEST: session.collectInput() [use '#' for terminate]");
    session.collectInput(input_callback, "my_Data_123", 5000);


    console_log('notice', "TEST: session.getDigits() [use '#' for terminate, waiting for 5 symbols]");

    var digits = session.getDigits(5);
    console_log('notice', "entered digits: " + digits);
}

console_log('notice', "***************** script finished *****************");
