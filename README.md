<p>
    An alternative javascript modules for the Freeswitch based on <a href="https://bellard.org/quickjs/">quickjs</a>. <br>
</p>

## version 1.0
 A quite stable version with the basic features and partial compatibility with the old spidermonkey scripts.
 Links relevant for this version:
 - [Build and installation guide](http://akscf.org/files/mod_quickjs/installation_guide.pdf)
 - [Builitin functions](http://akscf.org/files/mod_quickjs/builtin_functions_v10.pdf)
 - [Builitin classes](http://akscf.org/files/mod_quickjs/builtin_classes_v10.pdf)
 - [Scripts examples](examples/)

## version 1.6
This version focused on adding features to make interactive systems faster to develop.<br>
In particular: 
 - added the IVS object that helps to capture media streams and work with them <br>
   see: v16_echo.js (and other examples with v16_ prefix)
 - new features in the Curl object that allows to work in asynchronous mode <br>
   see: curl_async_test.js
 - new examples: <br>
    - [Stream capturing and working with chunks](https://github.com/akscf/mod_quickjs/blob/main/examples/v16_echo.js)
    - [Simple transcription through the whispered](https://github.com/akscf/mod_quickjs/blob/main/examples/v16_whisperd.js)
    - [Interaction with OpenAI (chatGPT+whisper)](https://github.com/akscf/mod_quickjs/blob/main/examples/v16_chatgpt.js)
    - [Asynchronous requests in the Curl](https://github.com/akscf/mod_quickjs/blob/main/examples/curl_async_test.js)
 
