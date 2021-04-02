// This file defines some simple handler functions
// The main purpose is to keep the FS checks and error handling DRY

#ifndef __HANLDERS__
#define __HANLDERS__

#include "utils.h"

const char HTTP_GET_NAME[] PROGMEM      = "GET";
const char HTTP_POST_NAME[] PROGMEM     = "POST";
const char HTTP_DELETE_NAME[] PROGMEM   = "DELETE";
const char HTTP_PUT_NAME[] PROGMEM      = "PUT";
const char HTTP_PATCH_NAME[] PROGMEM    = "PATCH";
const char HTTP_HEAD_NAME[] PROGMEM     = "HEAD";
const char HTTP_OPTIONS_NAME[] PROGMEM  = "OPTIONS";
const char HTTP_ANY_NAME[] PROGMEM      = "ANY";
const char* const HTTP_NAMES[] PROGMEM  = {
    HTTP_GET_NAME,
    HTTP_POST_NAME,
    HTTP_DELETE_NAME,
    HTTP_PUT_NAME,
    HTTP_PATCH_NAME,
    HTTP_HEAD_NAME,
    HTTP_OPTIONS_NAME,
};

AsyncCallbackWebHandler& register_fs_handler(
    AsyncWebServer *server,
    const char* uri,
    WebRequestMethodComposite method,
    const char* content_type,
    const char* content_encoding,
    const char* content_disposition,
    const char* fs_path
) {
    const char* method_name = HTTP_ANY_NAME;
    if (method < HTTP_ANY) {
        method_name = HTTP_NAMES[method - 1];
    }

    serial_printf("ROUTE: %s %s\n", method_name, uri);
    return server->on(uri, method, [=](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response;
        int code = 200;
        if (SPIFFS.exists(fs_path)) {
            response = request->beginResponse(SPIFFS, fs_path, content_type);
            response->addHeader("Content-Encoding", content_encoding);
            // Can't do this because async response adds a duplicate which is invalid
            // response->addHeader("Content-Disposition", content_disposition);
        } else {
            //TODO: better error?
            code = 404;
            response = request->beginResponse(code, "text/plain", fs_path);
        }
        serial_printf("%s %s %d\n", method_name, uri, code);
        request->send(response);
    });
}

#endif
