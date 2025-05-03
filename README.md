<p>
    An alternative javascript module for the Freeswitch based on <a href="https://bellard.org/quickjs/">quickjs</a>. <br>
    This module uses in the <a href="https://akstel.org/" target="_blank">SmartIVR</a> with <a href="https://demo1.akstel.org/" target="_blank">capabilities demo here</a>. 
</p>

## version 1.7
 - added configuration option 'use_std' for enabling functions from std/os modules <br>
 - added 'import' function for laoding so/js modules (see examples/dyn_module) <br> 
 - added DBH class for interaction with  freeswitch DBH <br>
 - many changes in Session (speech detection, etc) <br>
 - IVS class was removed <br>
 - odbc was removed <br>
    
## version 1.6
Was an experimental version for testing some ideas
 - added IVS class that helps to capture media streams and work with them <br>
   see: v16_echo.js (and other examples with v16_ prefix)
 - new features in the Curl class that allows to work in asynchronous mode <br>
   see: curl_async_test.js
 - new examples: <br>
    - [Stream capturing and working with chunks](https://github.com/akscf/mod_quickjs/blob/main/examples/v16_echo.js)
    - [Simple transcription through the whispered](https://github.com/akscf/mod_quickjs/blob/main/examples/v16_whisperd.js)
    - [Interaction with OpenAI (chatGPT+whisper)](https://github.com/akscf/mod_quickjs/blob/main/examples/v16_chatgpt.js)
    - [Asynchronous requests in the Curl](https://github.com/akscf/mod_quickjs/blob/main/examples/curl_async_test.js)
 
## version 1.0
 Quite old version, developed as a replacement for mod_spidermonkey (with capabilities to launch its scripts without changes)
 - [Build and installation guide](https://github.com/akscf/mod_quickjs/blob/main/docs/installation_guide.pdf)
 - [Functions](https://github.com/akscf/mod_quickjs/blob/main/docs/builtin_functions_v10.pdf)
 - [Classes](https://github.com/akscf/mod_quickjs/blob/main/docs/builtin_classes_v10.pdf)
 - [Examples](examples/)

