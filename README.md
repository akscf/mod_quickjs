<p>
    This is an alternative javascript engine for the Freeswitch based on <a href="https://bellard.org/quickjs/">quickjs engine</a>.<br>
    <br>
    What's that for ? <br>
    Freeswitch is a main tool which I (and my clients) use in various projects and I often need the features than absent there now. <br>
    To be honest the situation with javascript there leaves a lot to be desired, but unfortunately I'm not a fan of C++ and V8 therefore I had no motivation to improve the existing mod_v8 (i prefer pure C). 
    And as a result, several years ago, I've already tried the attempt to write a similar module but it left in a half finished state... But recently one of my client offered me to have a look at the quickjs engine and try to make the module based on one. I did it and it seemed a good idea and the work has begun...<br>
    <br>
    What I'm particularly lacked in the existing modules:
    <ul>
     <li>more closely integration with javascript (using objects in functions and so on)</li>
     <li>access to audio/video data in a session and codecs (not always comfortable to use C for it)</li>
     <li>asynchronous execution for the functions (threading)</li>
     <li>file object, like in mod_spidermonkey (I haven't seen it in freeswitch 1.8.7)</li>
     <li>possibility to interrupt the script at any time</li>
    </ul>
    <br>
    In whole, all the things which I wanted have been done (and even more [expect async.execution], see Builtin functions and Builtin classes). <br>
    I think this functionality is enough to make quite developed applications and I'm planning to continue developing it as needed. <br>
    But if you have any suggestions or ideas write me and we can discuss it. <br>
</p>

### Related links
 - [Build and installation guide](https://akscf.org/files/mod_quickjs/installation_guide.pdf)
 - [Builitin functions (version 1.0 / ready)](https://akscf.org/files/mod_quickjs/builtin_functions_v10.pdf)
 - [Builitin classes (version 1.0 / ready)](https://akscf.org/files/mod_quickjs/builtin_classes_v10.pdf)
 - [Scripts examples](examples/)

