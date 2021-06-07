/**
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

static struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts;
    switch_hash_t           *scripts_map;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
    int                     threads_active;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown);
SWITCH_MODULE_DEFINITION(mod_quickjs, mod_quickjs_load, mod_quickjs_shutdown, NULL);

// ---------------------------------------------------------------------------------------------------------------------------------------------
static script_t *script_lookup(char *name) {
    script_t *script = NULL;

    if(zstr(name)) { return NULL; }

    switch_mutex_lock(globals.mutex_scripts);
    script = switch_core_hash_find(globals.scripts_map, name);
    switch_mutex_unlock(globals.mutex_scripts);

    return script;
}

static uint32_t script_sem_take(script_t *script) {
    uint32_t status = SWITCH_FALSE;

    if(!script) { return SWITCH_FALSE; }

    switch_mutex_lock(script->mutex);
    if(script->fl_ready) {
        status = SWITCH_TRUE;
        script->tx_sem++;
    }
    switch_mutex_unlock(script->mutex);

    return status;
}

static void script_sem_release(script_t *script) {

    switch_assert(script);

    switch_mutex_lock(script->mutex);
    if(script->tx_sem) {
        script->tx_sem--;
    }
    switch_mutex_unlock(script->mutex);
}

static switch_status_t script_launch(char *script_name, char *script_args, uint8_t async, uint8_t invoke) {

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "script_launch: name=%s, args=%s, async=%i, invoke=%i\n", script_name, script_args, async, invoke);

    //
    // todo
    //

    return SWITCH_STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void event_handler_shutdown(switch_event_t *event) {
    globals.fl_shutdown = SWITCH_TRUE;
}

#define CMD_SYNTAX \
    "list - show rinning scripts\n" \
    "run script.js [args] - launch the script\n" \
    "invoke script.js [args] - launch or invoke exists script\n" \
    "kill script.js - stop the script\n"

SWITCH_STANDARD_API(quickjs_cmd) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *mycmd = NULL, *argv[5] = { 0 };
    int argc = 0;

    if(!zstr(cmd)) {
        mycmd = strdup(cmd);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(argc == 0) { goto usage; }
    if(globals.fl_shutdown) { goto out; }

    if(argc == 1) {
        if(strcasecmp(argv[0], "list") == 0) {
            switch_hash_index_t *hidx = NULL;

            switch_mutex_lock(globals.mutex_scripts);
            for (hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
                const void *hkey = NULL; void *hval = NULL;
                script_t *script = NULL;

                switch_core_hash_this(hidx, &hkey, NULL, &hval);
                script = (script_t *)hval;

                if(script_sem_take(script)) {
                    stream->write_function(stream, "%s\n", script->name);
                    script_sem_release(script);
                }
            }
            switch_mutex_unlock(globals.mutex_scripts);
            goto out;
        }
        goto usage;
    }
    if(strcasecmp(argv[0], "run") == 0) {
        char *script_args = (argc > 2 ? ((char *)cmd + (strlen(argv[0]) + strlen(argv[1]) + 2)) : NULL);

        if((status = script_launch(argv[1], script_args, SWITCH_TRUE, SWITCH_FALSE)) == SWITCH_STATUS_SUCCESS) {
            stream->write_function(stream, "+OK\n");
        } else {
            stream->write_function(stream, "-ERR: %i\n", status);
        }
        goto out;
    }
    if(strcasecmp(argv[0], "invoke") == 0) {
        char *script_args = (argc > 2 ? ((char *)cmd + (strlen(argv[0]) + strlen(argv[1]) + 2)) : NULL);

        if((status = script_launch(argv[1], script_args, SWITCH_TRUE, SWITCH_TRUE)) == SWITCH_STATUS_SUCCESS) {
            stream->write_function(stream, "+OK\n");
        } else {
            stream->write_function(stream, "-ERR: %i\n", status);
        }
        goto out;
    }
    if(strcasecmp(argv[0], "kill") == 0) {
        script_t *script = NULL;
        script = script_lookup(argv[1]);

        if(script_sem_take(script)) {
            script->fl_do_kill = SWITCH_TRUE;
            script_sem_release(script);
            stream->write_function(stream, "+OK\n");
        } else {
            stream->write_function(stream, "-ERR: Script not found\n");
        }
        goto out;
    }
    goto out;
usage:
    stream->write_function(stream, "-USAGE: %s\n", CMD_SYNTAX);
out:
    switch_safe_free(mycmd);
    return SWITCH_STATUS_SUCCESS;
}

#define APP_SYNTAX "[run|invoke] script.js [args]"
SWITCH_STANDARD_APP(quickjs_app) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *mycmd = NULL, *argv[5] = { 0 };
    char *script_name = NULL, *script_args = NULL;
    int argc = 0;

    if(!zstr(data)) {
        mycmd = strdup(data);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(globals.fl_shutdown) { goto out; }
    if(argc < 2) { goto usage; }

    script_name = argv[1];
    script_args = (argc > 2 ? (data + (strlen(argv[0]) + strlen(argv[1]) + 2)) : NULL);

    if(strcasecmp(argv[0], "run") == 0) {
        if((status = script_launch(script_name, script_args, SWITCH_FALSE, SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't launch script: %s\n", script_name);
        }
    } else if(strcasecmp(argv[0], "invoke") == 0) {
        if((status = script_launch(script_name, script_args, SWITCH_FALSE, SWITCH_TRUE)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't launch script: %s\n", script_name);
        }
    } else {
        goto usage;
    }
    goto out;
usage:
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", APP_SYNTAX);
out:
    switch_safe_free(mycmd);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
#define CONFIG_NAME "quickjs.conf"
SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_xml_t cfg, xml, settings, param;
    switch_api_interface_t *cmd_interface;
    switch_application_interface_t *app_interface;

    memset(&globals, 0, sizeof (globals));
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&globals.mutex_scripts, SWITCH_MUTEX_NESTED, pool);
    switch_core_hash_init(&globals.scripts_map);

    if((xml = switch_xml_open_cfg(CONFIG_NAME, &cfg, NULL)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open: %s\n", CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }
    if((settings = switch_xml_child(cfg, "settings"))) {
        for(param = switch_xml_child(settings, "param"); param; param = param->next) {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

        }
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(cmd_interface, "qjs", "manage quickjs scripts", quickjs_cmd, CMD_SYNTAX);
    SWITCH_ADD_APP(app_interface, "qjs", "manage quickjs scripts", "manage quickjs scripts", quickjs_app, APP_SYNTAX, SAF_NONE);

    if (switch_event_bind(modname, SWITCH_EVENT_SHUTDOWN, SWITCH_EVENT_SUBCLASS_ANY, event_handler_shutdown, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event handler!\n");
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    globals.fl_shutdown = SWITCH_FALSE;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "quckjs (%s)\n", MOD_VERSION);

done:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown) {
    switch_hash_index_t *hi = NULL;
    script_t *script = NULL;
    void *hval = NULL;

    switch_event_unbind_callback(event_handler_shutdown);

    globals.fl_shutdown = SWITCH_TRUE;
    while (globals.threads_active > 0) {
        switch_yield(100000);
    }

    switch_mutex_lock(globals.mutex_scripts);
    for(hi = switch_core_hash_first_iter(globals.scripts_map, hi); hi; hi = switch_core_hash_next(&hi)) {
        switch_core_hash_this(hi, NULL, NULL, &hval);
        script = (script_t *) hval;

        if(script_sem_take(script)) {
            script->fl_do_kill = SWITCH_TRUE;
            script_sem_release(script);
        }
    }
    switch_safe_free(hi);
    switch_core_hash_destroy(&globals.scripts_map);
    switch_mutex_unlock(globals.mutex_scripts);

    return SWITCH_STATUS_SUCCESS;
}

