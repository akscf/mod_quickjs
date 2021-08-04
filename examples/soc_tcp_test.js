console_log('notice', "TCP test");

var soc = new Socket('tcp', 5000);

console_log('notice', "socket.type.......: [" + soc.type + "]");
console_log('notice', "socket.nonblock...: [" + soc.nonblock + "]");
console_log('notice', "socket.timeout....: [" + soc.timeout + "]");

console_log('notice', "Connecting to 127.0.0.1:5555...");
if(soc.connect('127.0.0.1', 5555)) {

    console_log('notice', "sending data to client...");
    if(!soc.writeString("Test 123-456-789\n")) {
        console_log('err', "writeString failed!");
        exit();
    }

    console_log('notice', "waiting for a client response...");
    var data = soc.readString();
    if(data) {
        console_log('notice', "readString: " + data);
    }

    soc.close();
}

console_log('notice', "***************** script finished *****************");
