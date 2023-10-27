// -----------------------------------------------------------------------------------------------------------------------------
//
// updated: 27.10.2023
// required mod_quickjs 1.4 or higher 
//
// -----------------------------------------------------------------------------------------------------------------------------
//
// GET request
var curl = new CURL('http://127.0.0.1/', 'GET', 10);

// with ssl
// var curl = new CURL('https://127.0.0.1/index.html', 'GET', 10);
// curl.sslCAcert='catest.pem';

// with proxy
// curl.proxy='http://192.168.0.1:3128';
// curl.proxy='https://192.168.0.1:3128';
// curl.proxyCAcert='catest.pem';

consoleLog('notice', "curl.url.................: " + curl.url);
consoleLog('notice', "curl.method..............: " + curl.method);
consoleLog('notice', "curl.connectTimeout......: " + curl.connectTimeout);
consoleLog('notice', "curl.requestTimeout......: " + curl.requestTimeout);
consoleLog('notice', "curl.credentials.........: " + curl.credentials);
consoleLog('notice', "curl.authType............: " + curl.authType);
consoleLog('notice', "curl.contentType.........: " + curl.contentType);
consoleLog('notice', "curl.sslVerfyPeer........: " + curl.sslVerfyPeer);
consoleLog('notice', "curl.sslVerfyHost........: " + curl.sslVerfyHost);
consoleLog('notice', "curl.sslCAcert...........: " + curl.sslCAcert);
consoleLog('notice', "curl.proxy...............: " + curl.proxy);
consoleLog('notice', "curl.proxyCredentials....: " + curl.proxyCredentials);
consoleLog('notice', "curl.proxyCAcert.........: " + curl.proxyCAcert);

var resp = curl.perform();
if(!resp) {
    consoleLog('err', "Couldn't perform the request");
} else {
    consoleLog('notice', "response.code: [" + resp.code + "]");
    consoleLog('notice', "response.body: [" + resp.body + "]");
}

// -----------------------------------------------------------------------------------------------------------------------------
// POST request 
curl = new CURL('http://127.0.0.1/rpc/', 'POST', 10);
curl.authType = "basic";
curl.credentials = "secret:password";

var jsonData = JSON.stringify({'id': 1, 'service': 'myServiceName', 'method': 'myMethodName', 'params': [1,2,3,'a','b','c']});
resp = curl.perform(jsonData);

if(!resp) {
    consoleLog('err', "Couldn't perform the request");
} else {
    consoleLog('notice', "response.code: [" + resp.code + "]");
    consoleLog('notice', "response.body: [" + resp.body + "]");
}


consoleLog('notice', "***************** script finished *****************");

