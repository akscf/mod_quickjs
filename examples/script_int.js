console_log('notice', "script id.....: [" + script.id + "]");
console_log('notice', "script name...: [" + script.name + "]");
console_log('notice', "script path...: [" + script.path + "]\n\n");

console_log('notice', "loop started (use: qjs int " + script.id + ")\n\n");
while(true) {
    if(script.isInterrupted()) {
	break;
    }
    msleep(1000);
}
console_log('notice', "loop finished");