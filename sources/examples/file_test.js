/*
 * examples of use File object
 */

//
// directory i/o
//
var tmpDir = new File('/tmp');

console_log('notice', "dir.name..........: [" + tmpDir.name + "]");
console_log('notice', "dir.path..........: [" + tmpDir.path + "]");
console_log('notice', "dir.exists........: [" + tmpDir.exists() + "]");
console_log('notice', "dir.isFile........: [" + tmpDir.isFile + "]");
console_log('notice', "dir.isDirectory...: [" + tmpDir.isDirectory + "]");

if(!tmpDir.exists()) {
    if(!tmpDir.mkdir()) {
        console_log('error', "Couldn't create directory: [" + tmpDir.path + "]");
        exit();
    }
}

if(!tmpDir.open()) {
    console_log('error', "Couldn't opne directory: [" + tmpDir.path + "]");
    exit();
}

console_log('notice', "directory list: [" + tmpDir.path + "]");
tmpDir.list(function(path, name) {
    console_log('notice', "===> " + name + " (" + path + ")");
});

tmpDir.close();


//
// file string i/o
//
console_log('notice', "TEST1: /tmp/file1.txt");

var testf1 = new File('/tmp/file1.txt');

console_log('notice', "file1.name..........: [" + testf1.name + "]");
console_log('notice', "file1.path..........: [" + testf1.path + "]");
console_log('notice', "file1.exists........: [" + testf1.exists() + "]");
console_log('notice', "file1.isFile........: [" + testf1.isFile + "]");
console_log('notice', "file1.isDirectory...: [" + testf1.isDirectory + "]");
console_log('notice', "file1.size..........: [" + testf1.size + "]");

if(testf1.exists()) {
    testf1.rename('/tmp/file1.txt.old')
}
if(testf1.open("crwb")) {
    testf1.writeString("Line #1 - 1234567890\n");
    testf1.writeString("Line #2 - 0987654321\n");

    console_log('notice', "file1.position...: [" + testf1.position + "]");
    console_log('notice', "file1.size.......: [" + testf1.size + "]");

    testf1.seek(0);

    var data = testf1.readString(testf1.size);
    console_log('notice', "FILE1 CONTENT:\n" + data + "\n");

    testf1.close();
}

//
// file bytes i/o
//
console_log('notice', "TEST2: /tmp/file1.bin");

var buf1 = new ArrayBuffer(16);
var buf2 = new ArrayBuffer(16);
var buf1i8  = new Int8Array(buf1);
var buf2i8  = new Int8Array(buf2);

var testf2 = new File('/tmp/file1.bin');

console_log('notice', "file2.name..........: [" + testf2.name + "]");
console_log('notice', "file2.path..........: [" + testf2.path + "]");
console_log('notice', "file2.exists........: [" + testf2.exists() + "]");
console_log('notice', "file2.isFile........: [" + testf2.isFile + "]");
console_log('notice', "file2.isDirectory...: [" + testf2.isDirectory + "]");
console_log('notice', "file2.size..........: [" + testf2.size + "]");

if(testf2.exists()) {
    testf2.rename('/tmp/file1.bin.old')
}
if(testf2.open("crwb")) {
    buf1i8[0] = 0x0;
    buf1i8[1] = 0x1;
    buf1i8[2] = 0x2;
    buf1i8[3] = 0x3;
    buf1i8[4] = 0xA;
    buf1i8[5] = 0xB;
    buf1i8[6] = 0xC;
    buf1i8[7] = 0xD;

    var iolen = testf2.write(buf1, buf1.byteLength);

    console_log('notice', "write bytes......: [" + iolen + "]");
    console_log('notice', "file2.position...: [" + testf2.position + "]");
    console_log('notice', "file2.size.......: [" + testf2.size + "]");

    testf2.seek(0);

    iolen = testf2.read(buf2, buf2.byteLength);
    console_log('notice', "read bytes.......: [" + iolen + "]");

    var equal = true;
    for(var i = 0; i < buf1i8.byteLength; i++) {
        if(buf1i8[i] != buf2i8[i]) {
            equal = false;
            break;
        }
    }

    if(!equal) {
        console_log('warning', "buffers not equals!");
    } else {
        console_log('notice', "bytes i/o success!");
    }

    testf2.close();
}
