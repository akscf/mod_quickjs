
console_log('notice', "TEST #1 (simple http get)");

var curl = new CURL('http://127.0.0.1/index.html', 'GET', 10);

console_log('notice', "curl.url...........: [" + curl.url + "]");
console_log('notice', "curl.method........: [" + curl.method + "]");
console_log('notice', "curl.timeout.......: [" + curl.timeout + "]");
console_log('notice', "curl.credentials...: [" + curl.credentials + "]");
console_log('notice', "curl.contentType...: [" + curl.contentType + "]");

var resp = curl.doRequest();
if(resp.error) {
    console_log('err', "response.error.............: [" + resp.error + "]");
} else {
    console_log('notice', "response.contentLength....: [" + resp.contentLength + "]");
    console_log('notice', "response.content..........: [" + resp.content + "]");
}

//
// -----------------------------------------------------------------------------------------------------------------------------
//
console_log('notice', "TEST #2 (json-rpc call)");

var rpcReq = {'id': 1, 'service': 'SwitchManagementService', 'method': 'switchGetStatus', 'params': null};
var jsonData = JSON.stringify(rpcReq);

curl = new CURL('http://127.0.0.1:8080/rpc/', 'POST', 10, "admin:secret");

console_log('notice', "curl.url...........: [" + curl.url + "]");
console_log('notice', "curl.method........: [" + curl.method + "]");
console_log('notice', "curl.timeout.......: [" + curl.timeout + "]");
console_log('notice', "curl.credentials...: [" + curl.credentials + "]");
console_log('notice', "curl.contentType...: [" + curl.contentType + "]");

resp = curl.doRequest(jsonData);
if(resp.error) {
    console_log('err', "transport error: " + resp.error);
} else {
    console_log('notice', "response.contentLength....: [" + resp.contentLength + "]");
    console_log('notice', "response.content..........: [" + resp.content + "]");
}


console_log('notice', "***************** script finished *****************");
