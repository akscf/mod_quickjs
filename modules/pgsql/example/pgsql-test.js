import { Pgsql } from "pgsql.so";

consoleLog('notice', "---------------- pgsql test start -----------------");

var tmap = {
    "bool" : "bool",    
    "int4" : function(type, val) { return parseInt(val); }
};

//var dbh = new Pgsql("dbh:pgsql://hostaddr=127.0.0.1 dbname=test user=test password=test", tmap);
var dbh = new Pgsql("host=127.0.0.1 dbname=test user=test password=test", tmap);
if(!dbh.isConnected) {
    console_log('err', "SQLError: " + dbh.error);
    exit();
}

var res = dbh.execQuery("select * from test", myCallback);
if(!res) {
    console_log('notice', "SQLEerror: " + dbh.error);
}

consoleLog('notice', "---------------- pgsql test done -----------------");


// --------------------------------------------------------------------------------------------------
function myCallback(rowData) {
    console_log('notice', "ROW: " + JSON.stringify(rowData));
    return true;
}
