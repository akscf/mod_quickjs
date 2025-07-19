import { Pgsql } from "pgsql.so";

consoleLog('notice', "---------------- pgsql test start -----------------");

var tmap = {
    "bool" : "bool",    
    "int4" : function(type, val) { return parseInt(val); }
};

var dbh = new Pgsql("host=127.0.0.1 dbname=test user=test password=test", tmap);
if(!dbh.isConnected) {
    console_log('err', "SQLError: " + dbh.error);
    exit();
}

var res = dbh.execQuery("select * from test");
if(!res) {
    console_log('notice', "SQLEerror: " + dbh.error);
} else {
    consoleLog('notice', "RESULT: " + JSON.stringify(res));
}

consoleLog('notice', "---------------- pgsql test done -----------------");
