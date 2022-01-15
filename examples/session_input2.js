console_log('notice', "session.playAndGetDigits()");

if(typeof(session) != 'undefined') {

    if(!session.isReady) {
	exit();
    }

    var digits = session.playAndGetDigits(0, 5, 3, 7000, "#", "/opt/freeswitch-master/share/freeswitch/sounds/en/us/callie/conference/8000/conf-pin.wav", "/opt/freeswitch-master/share/freeswitch/sounds/en/us/callie/conference/8000/invalid.wav");
    console_log('notice', "digits: " + digits);

}

