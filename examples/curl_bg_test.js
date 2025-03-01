// -----------------------------------------------------------------------------------------------------------------------------
//
// updated: 27.10.2023
// required mod_quickjs 1.4 or higher 
//
// -----------------------------------------------------------------------------------------------------------------------------
var curl = new CURL('http://127.0.0.1/', 'GET', 10);

var send_cnt = 0;
var recv_cnt = 0;

while(!script.isInterrupted()) {
    if(send_cnt < 3) {
        var jid = curl.performBg();
        if(jid) {
            consoleLog('notice', "job-id: " + jid);
            send_cnt++;
        } else {
            consoleLog('error', "unable to perform request");
        }
        continue;
    }

    var res = curl.getResult();
    if(res) {
        //consoleLog('notice', "RESULT: " + JSON.stringify(res));
        consoleLog('notice', "job-done: " + res.jid + " - done (code=" + res.code + ")");
        recv_cnt++;
    }

    if(send_cnt == recv_cnt) {
        break;
    }

    msleep(1000);
}

consoleLog('notice', "***************** script finished *****************");