/**
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

static struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts;
    switch_inthash_t        *scripts_map;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
    int                     active_threads;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown);
SWITCH_MODULE_DEFINITION(mod_quickjs, mod_quickjs_load, mod_quickjs_shutdown, NULL);

static void *SWITCH_THREAD_FUNC script_instance_thread(switch_thread_t *thread, void *obj);

static uint32_t script_sem_take(script_t *script);
static void script_sem_release(script_t *script);

// ---------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t name2id(char *name, uint32_t len) {
    return switch_crc32_8bytes((char *)name, len);
}

static uint32_t rand_id() {
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
    memset((char *)uuid_str, '\0' , sizeof(uuid_str));
    switch_uuid_str(uuid_str, sizeof(uuid_str));
    return name2id((char *)uuid_str, strlen(uuid_str));
}

void launch_thread(switch_memory_pool_t *pool, switch_thread_start_t fun, void *data) {
    switch_threadattr_t *attr = NULL;
    switch_thread_t *thread = NULL;

    switch_threadattr_create(&attr, pool);
    switch_threadattr_detach_set(attr, 1);
    switch_threadattr_stacksize_set(attr, SWITCH_THREAD_STACKSIZE);
    switch_thread_create(&thread, attr, fun, data, pool);

    switch_mutex_lock(globals.mutex);
    globals.active_threads++;
    switch_mutex_unlock(globals.mutex);

    return;
}

static script_t *script_lookup(char *name) {
    script_t *script = NULL;
    int32_t id = 0;

    if(zstr(name) || globals.fl_shutdown) {
        return NULL;
    }
    id = name2id(name, strlen(name));

    switch_mutex_lock(globals.mutex_scripts);
    script = switch_core_inthash_find(globals.scripts_map, id);
    switch_mutex_unlock(globals.mutex_scripts);

    return script;
}

static script_instance_t *instance_lookup(script_t *script, uint32_t id) {
    script_instance_t *instance = NULL;

    if(!script || globals.fl_shutdown) {
        return NULL;
    }
    if(script_sem_take(script)) {
        switch_mutex_lock(script->mutex);
        instance = switch_core_inthash_find(script->instances_map, id);
        switch_mutex_unlock(script->mutex);
        script_sem_release(script);
    }
    return instance;
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

static uint32_t script_instance_sem_take(script_instance_t *instance) {
    uint32_t status = SWITCH_FALSE;

    if(!instance) { return SWITCH_FALSE; }

    switch_mutex_lock(instance->mutex);
    if(instance->fl_ready) {
        status = SWITCH_TRUE;
        instance->tx_sem++;
    }
    switch_mutex_unlock(instance->mutex);
    return status;
}

static void script_instance_sem_release(script_instance_t *instance) {
    switch_assert(instance);

    switch_mutex_lock(instance->mutex);
    if(instance->tx_sem) {
        instance->tx_sem--;
    }
    switch_mutex_unlock(instance->mutex);
}

static switch_status_t script_destroy(script_t *script) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    if(script_sem_take(script)) {
        script->fl_ready = SWITCH_FALSE;
        script->fl_destroyed = SWITCH_TRUE;

        switch_mutex_lock(globals.mutex_scripts);
        switch_core_inthash_delete(globals.scripts_map, script->id);
        switch_mutex_unlock(globals.mutex_scripts);

        while(script->tx_sem > 1) {
            switch_yield(100000);
        }

        switch_core_destroy_memory_pool(&script->pool);
    }
    return status;
}

static switch_status_t script_launch(switch_core_session_t *session, char *script_name, char *script_args, uint8_t async) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *script_path_local = NULL;
    char *script_args_local = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_memory_pool_t *ipool = NULL;
    script_t *script = NULL;
    script_instance_t *script_instance = NULL;
    uint32_t id = 0;

    if(zstr(script_name)) {
        status = SWITCH_STATUS_FALSE;
        goto out;
    }

    id = name2id(script_name, strlen(script_name));
    if(!zstr(script_args)) {
        script_args_local = strdup(script_args);
    }

    if(switch_file_exists(script_name, NULL) == SWITCH_STATUS_SUCCESS) {
        script_path_local = strdup(script_name);
    } else {
        script_path_local = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, script_name);
        if(switch_file_exists(script_path_local, NULL) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File not found: %s\n", script_path_local);
            status = SWITCH_STATUS_FALSE;
            goto out;
        }
    }

    script = script_lookup(script_name);
    if(!script) {
        if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_GENERR;
            goto out;
        }
        if((script = switch_core_alloc(pool, sizeof(script_t))) == NULL)  {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_GENERR;
            goto out;
        }
        script->id = id;
        script->pool = pool;
        script->path = switch_core_strdup(pool, script_path_local);
        script->name = basename(script->path);

        switch_core_inthash_init(&script->instances_map);
        switch_mutex_init(&script->mutex, SWITCH_MUTEX_NESTED, pool);

        switch_mutex_lock(globals.mutex_scripts);
        switch_core_inthash_insert(globals.scripts_map, script->id, script);
        switch_mutex_unlock(globals.mutex_scripts);

        script->fl_ready = SWITCH_TRUE;
    }

    if(script_sem_take(script)) {
        if(switch_core_new_memory_pool(&ipool) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_GENERR;
            goto out;
        }
        if((script_instance = switch_core_alloc(ipool, sizeof(script_instance_t))) == NULL)  {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_GENERR;
            goto out;
        }
        script_instance->pool = ipool;
        script_instance->script = script;
        script_instance->session = session;
        script_instance->session_id = (session ? switch_core_session_get_uuid(session) : NULL);
        script_instance->args = (zstr(script_args_local) ? NULL : switch_core_strdup(ipool, script_args_local));
        script_instance->id = (zstr(script_instance->session_id) ? rand_id() : name2id((char *)script_instance->session_id, strlen(script_instance->session_id)));
        script_instance->fl_async = async;
        switch_mutex_init(&script_instance->mutex, SWITCH_MUTEX_NESTED, ipool);
        //
        // todo: init js
        //
        switch_mutex_lock(script->mutex);
        switch_core_inthash_insert(script->instances_map, script_instance->id, script_instance);
        script->instances++;
        switch_mutex_unlock(script->mutex);

        script_sem_take(script);
        launch_thread(ipool, script_instance_thread, script_instance);

        if(!async) {
            while(SWITCH_TRUE) {
                if(script->fl_do_kill || script_instance->fl_do_kill || script->fl_destroyed || script_instance->fl_destroyed || globals.fl_shutdown) {
                    break;
                }
                switch_yield(10000);
            }
            script_instance->fl_ready = SWITCH_FALSE;
            script_instance->fl_destroyed = SWITCH_TRUE;

            switch_mutex_lock(script->mutex);
            switch_core_inthash_delete(script->instances_map, script_instance->id);
            script->instances--;
            switch_mutex_unlock(script->mutex);

            while(script_instance->tx_sem > 0) {
                switch_yield(100000);
            }

            switch_core_destroy_memory_pool(&script_instance->pool);
        }
        script_sem_release(script);

        if(!script->instances) {
            script_destroy(script);
        }
    } else {
        status = SWITCH_STATUS_FALSE;
    }
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(pool) { switch_core_destroy_memory_pool(&pool); }
        if(ipool) { switch_core_destroy_memory_pool(&ipool); }
    }
    switch_safe_free(script_path_local);
    switch_safe_free(script_args_local);
    return status;
}

static void *SWITCH_THREAD_FUNC script_instance_thread(switch_thread_t *thread, void *obj) {
    volatile script_instance_t *_ref = (script_instance_t *) obj;
    script_instance_t *script_instance = (script_instance_t *) _ref;
    script_t *script = (script_t *)script_instance->script;
    uint8_t fl_async = script_instance->fl_async;
    uint32_t t_cnt = 100;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "STATED: %s (%X) [args=%s]\n", script->name, script_instance->id, script_instance->args);

    script_instance->fl_ready = SWITCH_TRUE;
    while(1) {
        if(script->fl_do_kill || script_instance->fl_do_kill || script->fl_destroyed || script_instance->fl_destroyed || globals.fl_shutdown) {
            break;
        }
        // test
        switch_yield(100000);
        if(t_cnt == 0) { break; }
        else { t_cnt--; }
    }
out:
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "FINISHED: %s (%X)\n", script->name, script_instance->id);

    script_instance->fl_ready = SWITCH_FALSE;
    script_instance->fl_destroyed = SWITCH_TRUE;

    if(fl_async) {
        switch_mutex_lock(script->mutex);
        switch_core_inthash_delete(script->instances_map, script_instance->id);
        script->instances--;
        switch_mutex_unlock(script->mutex);

        while(script_instance->tx_sem > 0) {
            switch_yield(100000);
        }

        switch_core_destroy_memory_pool(&script_instance->pool);
    }
    script_sem_release(script);

    if(fl_async && !script->instances) {
        script_destroy(script);
    }

    switch_mutex_lock(globals.mutex);
    if(globals.active_threads > 0) globals.active_threads--;
    switch_mutex_unlock(globals.mutex);

    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void event_handler_shutdown(switch_event_t *event) {
    globals.fl_shutdown = SWITCH_TRUE;
}

#define CMD_SYNTAX "\n" \
    "list - show running scripts\n" \
    "run scriptName [args] - launch a new script instance\n" \
    "kill scriptName [instanceId] - kill script/instance\n"

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
            switch_hash_index_t *hidx2 = NULL;

            switch_mutex_lock(globals.mutex_scripts);
            for(hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
                void *hval = NULL;
                script_t *script = NULL;

                switch_core_hash_this(hidx, NULL, NULL, &hval);
                script = (script_t *)hval;

                if(script_sem_take(script)) {
                    stream->write_function(stream, "%s [%i] (%s)\n", script->name, script->instances, script->path);
                    for(hidx2 = switch_core_hash_first_iter(script->instances_map, hidx2); hidx2; hidx2 = switch_core_hash_next(&hidx2)) {
                        script_instance_t *script_instance = NULL;
                        void *hval2 = NULL;

                        switch_core_hash_this(hidx2, NULL, NULL, &hval2);
                        script_instance = (script_instance_t *)hval2;

                        if(script_instance_sem_take(script_instance)) {
                            stream->write_function(stream, " - %X (session: %s)\n", script_instance->id, (script_instance->session_id ? script_instance->session_id : "none"));
                            script_instance_sem_release(script_instance);
                        }
                    }
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

        if((status = script_launch(session, argv[1], script_args, SWITCH_TRUE)) == SWITCH_STATUS_SUCCESS) {
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
            if(argc > 2) {
                script_instance_t *instance = NULL;
                uint32_t id = name2id(argv[2], strlen(argv[2]));
                instance = instance_lookup(script, id);
                if(script_instance_sem_take(instance)) {
                    instance->fl_do_kill = SWITCH_TRUE;
                    script_instance_sem_release(instance);
                    stream->write_function(stream, "+OK\n");
                } else {
                    stream->write_function(stream, "-ERR: Instance not found\n");
                }
            } else {
                script->fl_do_kill = SWITCH_TRUE;
                stream->write_function(stream, "+OK\n");
            }
            script_sem_release(script);
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

#define APP_SYNTAX "[run] script.js [args]"
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
    script_args = (argc > 2 ? ((char *)data + (strlen(argv[0]) + strlen(argv[1]) + 2)) : NULL);

    if(strcasecmp(argv[0], "run") == 0) {
        if((status = script_launch(NULL, script_name, script_args, SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't launch: %s\n", script_name);
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
    switch_core_inthash_init(&globals.scripts_map);
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&globals.mutex_scripts, SWITCH_MUTEX_NESTED, pool);

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
    switch_hash_index_t *hidx = NULL;
    void *hval = NULL;

    switch_event_unbind_callback(event_handler_shutdown);

    globals.fl_shutdown = SWITCH_TRUE;
    while(globals.active_threads > 0) {
        switch_yield(100000);
    }

    switch_mutex_lock(globals.mutex_scripts);
    for(hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
        script_t *script = NULL;

        switch_core_hash_this(hidx, NULL, NULL, &hval);
        script = (script_t *) hval;

        if(script_sem_take(script)) {
            script->fl_do_kill = SWITCH_TRUE;
            script_sem_release(script);
        }
    }

    switch_safe_free(hidx);
    switch_core_inthash_destroy(&globals.scripts_map);
    switch_mutex_unlock(globals.mutex_scripts);

    return SWITCH_STATUS_SUCCESS;
}

