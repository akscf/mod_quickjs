console_log('notice', "script name...: [" + script.name + "]");
console_log('notice', "script id.....: [" + script.instance_id + "]");

console_log('notice', "script argc...: [" + argc + "]");
console_log('notice', "script argv...: [" + argv + "]");

for(var i = 0; i < argc; i++) {
    console_log('notice', "arg [" + i + "] => [" + argv[i] + "]");
}

