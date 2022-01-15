if(flags.odbcEnabled != "true") {
    console_log('err', "ODBC disabled");
    exit();
}

console_log('notice', "ODBC test");

var db = new ODBC("dsnName", "username", "password");
var sql = "select * from users";
 
db.connect();
 
if(!db.query(sql))
  session.hangup(); //might want to say something nice instead.
 
while (db.nextRow()) {
  row = db.getData();
  console_log("debug", "UserName: " + row["user_name"]);
}

