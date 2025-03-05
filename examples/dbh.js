
function myCallback(rowData) {
    console_log('notice', "---->  " + JSON.stringify(rowData));
    return true;
}

var dbh = new DBH("odbc://testdb:testusr:testpass");
if(dbh.isConnected) {
    console_log('notice', "DBH connected!");
} else {
    console_log('notice', "DBH error: " + dbh.error);
}

var res = dbh.execQuery("select * from test", myCallback);
if(!res) {
    console_log('notice', "error: " + dbh.error);
}

