if(flags.curlEnabled != "true") {
    console_log('err', "CURL disabled");
    exit();
}

console_log('notice', "TEST #1 (simple http get)");
var curl = new CURL('http://127.0.0.1/index.html', 'GET', 10);

// with ssl
// var curl = new CURL('https://127.0.0.1/index.html', 'GET', 10);
// curl.sslCAcert='catest.pem';

// with proxy
// curl.proxy='http://192.168.0.1:3128';
// curl.proxy='https://192.168.0.1:3128';
// curl.proxyCAcert='catest.pem';

console_log('notice', "curl.url................: [" + curl.url + "]");
console_log('notice', "curl.method.............: [" + curl.method + "]");
console_log('notice', "curl.timeout............: [" + curl.timeout + "]");
console_log('notice', "curl.credentials........: [" + curl.credentials + "]");
console_log('notice', "curl.contentType........: [" + curl.contentType + "]");
console_log('notice', "curl.sslCAcert..........: [" + curl.sslCAcert + "]");
console_log('notice', "curl.sslVerfyPeer.......: [" + curl.sslVerfyPeer + "]");
console_log('notice', "curl.sslVerfyHost.......: [" + curl.sslVerfyHost + "]");
console_log('notice', "curl.proxy..............: [" + curl.proxy + "]");
console_log('notice', "curl.proxyCAcert........: [" + curl.proxyCAcert + "]");
console_log('notice', "curl.proxyCredentials...: [" + curl.proxyCredentials + "]");

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
