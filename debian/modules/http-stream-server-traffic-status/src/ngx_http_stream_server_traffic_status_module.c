
/*
 * Copyright (C) YoungJoo Kim (vozlt)
 */


#include <ngx_config.h>

#include "ngx_http_stream_server_traffic_status_module.h"
#include "ngx_http_stream_server_traffic_status_display.h"


static char *ngx_http_stream_server_traffic_status_zone(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_http_stream_server_traffic_status_average_method(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static void *ngx_http_stream_server_traffic_status_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_stream_server_traffic_status_init_main_conf(ngx_conf_t *cf,
    void *conf);
static void *ngx_http_stream_server_traffic_status_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_stream_server_traffic_status_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);


static ngx_conf_enum_t  ngx_http_stream_server_traffic_status_display_format[] = {
    { ngx_string("json"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_JSON },
    { ngx_string("html"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_HTML },
    { ngx_string("jsonp"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_JSONP },
    { ngx_string("prometheus"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_PROMETHEUS },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_stream_server_traffic_status_average_method_post[] = {
    { ngx_string("AMM"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_AMM },
    { ngx_string("WMA"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_WMA },
    { ngx_null_string, 0 }
};


static ngx_command_t ngx_http_stream_server_traffic_status_commands[] = {

    { ngx_string("stream_server_traffic_status"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, enable),
      NULL },

    { ngx_string("stream_server_traffic_status_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
      ngx_http_stream_server_traffic_status_zone,
      0,
      0,
      NULL },

    { ngx_string("stream_server_traffic_status_display"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
      ngx_http_stream_server_traffic_status_display,
      0,
      0,
      NULL },

    { ngx_string("stream_server_traffic_status_display_format"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, format),
      &ngx_http_stream_server_traffic_status_display_format },

    { ngx_string("stream_server_traffic_status_display_jsonp"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, jsonp),
      NULL },

    { ngx_string("stream_server_traffic_status_average_method"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_stream_server_traffic_status_average_method,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_http_module_t ngx_http_stream_server_traffic_status_module_ctx = {
    NULL,                                                    /* preconfiguration */
    NULL,                                                    /* postconfiguration */

    ngx_http_stream_server_traffic_status_create_main_conf,  /* create main configuration */
    ngx_http_stream_server_traffic_status_init_main_conf,    /* init main configuration */

    NULL,                                                    /* create server configuration */
    NULL,                                                    /* merge server configuration */

    ngx_http_stream_server_traffic_status_create_loc_conf,   /* create location configuration */
    ngx_http_stream_server_traffic_status_merge_loc_conf,    /* merge location configuration */
};


ngx_module_t ngx_http_stream_server_traffic_status_module = {
    NGX_MODULE_V1,
    &ngx_http_stream_server_traffic_status_module_ctx,       /* module context */
    ngx_http_stream_server_traffic_status_commands,          /* module directives */
    NGX_HTTP_MODULE,                                         /* module type */
    NULL,                                                    /* init master */
    NULL,                                                    /* init module */
    NULL,                                                    /* init process */
    NULL,                                                    /* init thread */
    NULL,                                                    /* exit thread */
    NULL,                                                    /* exit process */
    NULL,                                                    /* exit master */
    NGX_MODULE_V1_PADDING
};


static char *
ngx_http_stream_server_traffic_status_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                    *value, name;
    ngx_uint_t                                    i;
    ngx_http_stream_server_traffic_status_ctx_t  *ctx;

    value = cf->args->elts;

    ctx = ngx_http_conf_get_module_main_conf(cf, ngx_http_stream_server_traffic_status_module);
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    ctx->enable = 1;

    ngx_str_set(&name, NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_SHM_NAME);

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "shared:", 7) == 0) {
            name.data = value[i].data + 7;
            name.len = value[i].len - 7;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    ctx->shm_name = name;

    return NGX_CONF_OK;
}


static char *
ngx_http_stream_server_traffic_status_average_method(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_stream_server_traffic_status_loc_conf_t *stscf = conf;

    char       *rv;
    ngx_int_t   rc;
    ngx_str_t  *value;

    value = cf->args->elts;

    cmd->offset = offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, average_method);
    cmd->post = &ngx_http_stream_server_traffic_status_average_method_post;

    rv = ngx_conf_set_enum_slot(cf, cmd, conf);
    if (rv != NGX_CONF_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[1]);
        goto invalid;
    }

    /* second argument process */
    if (cf->args->nelts == 3) {
        rc = ngx_parse_time(&value[2], 0);
        if (rc == NGX_ERROR) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[2]);
            goto invalid;
        }
        stscf->average_period = (ngx_msec_t) rc;
    }

    return NGX_CONF_OK;

invalid:

    return NGX_CONF_ERROR;
}


static void *
ngx_http_stream_server_traffic_status_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_stream_server_traffic_status_ctx_t  *ctx;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_stream_server_traffic_status_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->enable = NGX_CONF_UNSET;

    return ctx;
}


static char *
ngx_http_stream_server_traffic_status_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_stream_server_traffic_status_ctx_t  *ctx = conf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0,
                   "http stream sts init main conf");

    ngx_conf_init_value(ctx->enable, 0);

    return NGX_CONF_OK;
}


static void *
ngx_http_stream_server_traffic_status_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_stream_server_traffic_status_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_stream_server_traffic_status_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->shm_zone = { NULL, ... };
     *     conf->enable = 0;
     *     conf->shm_name = { 0, NULL };
     *     conf->stats = { 0, ... };
     *     conf->start_msec = 0;
     *     conf->format = 0;
     *     conf->jsonp = { 1, NULL };
     *     conf->average_method = 0;
     *     conf->average_period = 0;
     */

    conf->start_msec = ngx_http_stream_server_traffic_status_current_msec();
    conf->enable = NGX_CONF_UNSET;
    conf->shm_zone = NGX_CONF_UNSET_PTR;
    conf->format = NGX_CONF_UNSET;
    conf->average_method = NGX_CONF_UNSET;
    conf->average_period = NGX_CONF_UNSET_MSEC;

    conf->node_caches = ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t *)
                                    * (NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_FG + 1));
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_NO] = NULL;
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_UA] = NULL;
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_UG] = NULL;
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_FG] = NULL;

    return conf;
}


static char *
ngx_http_stream_server_traffic_status_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_stream_server_traffic_status_loc_conf_t *prev = parent;
    ngx_http_stream_server_traffic_status_loc_conf_t *conf = child;

    ngx_http_stream_server_traffic_status_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0,
                   "http stream sts merge loc conf");

    ctx = ngx_http_conf_get_module_main_conf(cf, ngx_http_stream_server_traffic_status_module);

    if (!ctx->enable) {
        return NGX_CONF_OK;
    }

    ngx_conf_merge_value(conf->enable, prev->enable, 1);
    ngx_conf_merge_ptr_value(conf->shm_zone, prev->shm_zone, NULL);
    ngx_conf_merge_value(conf->format, prev->format,
                         NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_JSON);
    ngx_conf_merge_str_value(conf->jsonp, prev->jsonp,
                             NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_JSONP);
    ngx_conf_merge_value(conf->average_method, prev->average_method,
                         NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_AMM);
    ngx_conf_merge_msec_value(conf->average_period, prev->average_period,
                              NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_AVG_PERIOD * 1000);

    conf->shm_name = ctx->shm_name;

    return NGX_CONF_OK;
}


ngx_msec_t
ngx_http_stream_server_traffic_status_current_msec(void)
{
    time_t           sec;
    ngx_uint_t       msec;
    struct timeval   tv;

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;

    return (ngx_msec_t) sec * 1000 + msec;
}

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
