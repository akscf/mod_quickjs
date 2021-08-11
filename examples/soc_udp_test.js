
console_log('notice', "UDP test");

var soc = new Socket('udp', '127.0.0.10', true);

console_log('notice', "socket.type.......: [" + soc.type + "]");
console_log('notice', "socket.nonblock...: [" + soc.nonblock + "]");
console_log('notice', "socket.timeout....: [" + soc.timeout + "]");

if(soc.connect('127.0.0.1', 5555)) {

    console_log('notice', "sending data to client...");
    if(!soc.writeString("Test 123-456-789\n")) {
        console_log('err', "writeString failed!");
        exit();
    }

    console_log('notice', "waiting for a client response...");
    var wait = 5;
    while(wait > 0) {
        var data = soc.readString();
        if(!data) {
            wait--; msleep(1000);
            continue;
        }
        console_log('notice', "readString: " + data);
        break;
    }

    soc.close();
}


console_log('notice', "***************** script finished *****************");
