
console_log('notice', "CoreDB test");
var db = new CoreDB('test');
var res = 0;
// ---------------------------------------------------------------------------------------------------------------
if(!db.tableExists('t1')) {
    db.exec("CREATE TABLE t1 (name VARCHAR(255), value VARCHAR(255))");
    for(var i=0; i< 10; i++) {
        if(!db.exec("INSERT INTO t1 (name, value) VALUES('var_" + i + "', 'val_" + i + "')")) {
            console_log('err', "insert faild");
            exit();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
console_log('notice', "SELECT #1");

res = db.exec("SELECT * FROM t1", function(rowData) {
            console_log('notice', "ROW: " + rowData['name'] + " ==> " + rowData['value']);
            return true;
        });


// ---------------------------------------------------------------------------------------------------------------
console_log('notice', "SELECT #2");

res = db.prepare("SELECT * FROM t1");
if(res) {
    while(db.next()) {
        var rowData = db.fetch();
        console_log('notice', "ROW: " + rowData['name'] + " ==> " + rowData['value']);
    }
}
