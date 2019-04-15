
/*
 * Copyright (C) YoungJoo Kim (vozlt)
 */


#include <ngx_config.h>

#include "ngx_stream_server_traffic_status_module.h"
#include "ngx_stream_server_traffic_status_variables.h"
#include "ngx_stream_server_traffic_status_shm.h"
#include "ngx_stream_server_traffic_status_filter.h"
#include "ngx_stream_server_traffic_status_limit.h"


static ngx_int_t ngx_stream_server_traffic_status_handler(ngx_stream_session_t *s);

static void ngx_stream_server_traffic_status_rbtree_insert_value(
    ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel);
static ngx_int_t ngx_stream_server_traffic_status_init_zone(
    ngx_shm_zone_t *shm_zone, void *data);
static char *ngx_stream_server_traffic_status_zone(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_stream_server_traffic_status_average_method(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_stream_server_traffic_status_histogram_buckets(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_stream_server_traffic_status_preconfiguration(ngx_conf_t *cf);
static void *ngx_stream_server_traffic_status_create_main_conf(ngx_conf_t *cf);
static char *ngx_stream_server_traffic_status_init_main_conf(ngx_conf_t *cf,
    void *conf);
static void *ngx_stream_server_traffic_status_create_loc_conf(ngx_conf_t *cf);
static char *ngx_stream_server_traffic_status_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_stream_server_traffic_status_init(ngx_conf_t *cf);


static ngx_conf_enum_t  ngx_stream_server_traffic_status_average_method_post[] = {
    { ngx_string("AMM"), NGX_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_AMM },
    { ngx_string("WMA"), NGX_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_WMA },
    { ngx_null_string, 0 }
};


static ngx_command_t ngx_stream_server_traffic_status_commands[] = {

    { ngx_string("server_traffic_status"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_server_traffic_status_conf_t, enable),
      NULL },

    { ngx_string("server_traffic_status_filter"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_server_traffic_status_conf_t, filter),
      NULL },

    { ngx_string("server_traffic_status_filter_check_duplicate"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_server_traffic_status_conf_t, filter_check_duplicate),
      NULL },

    { ngx_string("server_traffic_status_filter_by_set_key"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE12,
      ngx_stream_server_traffic_status_filter_by_set_key,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("server_traffic_status_limit"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_server_traffic_status_conf_t, limit),
      NULL },

    { ngx_string("server_traffic_status_limit_check_duplicate"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_server_traffic_status_conf_t, limit_check_duplicate),
      NULL },

    { ngx_string("server_traffic_status_limit_traffic"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE12,
      ngx_stream_server_traffic_status_limit_traffic,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("server_traffic_status_limit_traffic_by_set_key"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE23,
      ngx_stream_server_traffic_status_limit_traffic_by_set_key,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("server_traffic_status_zone"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
      ngx_stream_server_traffic_status_zone,
      0,
      0,
      NULL },

    { ngx_string("server_traffic_status_average_method"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE12,
      ngx_stream_server_traffic_status_average_method,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("server_traffic_status_histogram_buckets"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_1MORE,
      ngx_stream_server_traffic_status_histogram_buckets,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_stream_module_t ngx_stream_server_traffic_status_module_ctx = {
    ngx_stream_server_traffic_status_preconfiguration, /* preconfiguration */
    ngx_stream_server_traffic_status_init,             /* postconfiguration */

    ngx_stream_server_traffic_status_create_main_conf, /* create main configuration */
    ngx_stream_server_traffic_status_init_main_conf,   /* init main configuration */

    ngx_stream_server_traffic_status_create_loc_conf,  /* create server configuration */
    ngx_stream_server_traffic_status_merge_loc_conf,   /* merge server configuration */
};


ngx_module_t ngx_stream_server_traffic_status_module = {
    NGX_MODULE_V1,
    &ngx_stream_server_traffic_status_module_ctx,      /* module context */
    ngx_stream_server_traffic_status_commands,         /* module directives */
    NGX_STREAM_MODULE,                                 /* module type */
    NULL,                                              /* init master */
    NULL,                                              /* init module */
    NULL,                                              /* init process */
    NULL,                                              /* init thread */
    NULL,                                              /* exit thread */
    NULL,                                              /* exit process */
    NULL,                                              /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_stream_server_traffic_status_handler(ngx_stream_session_t *s)
{
    ngx_int_t                                 rc;
    ngx_stream_server_traffic_status_ctx_t   *ctx;
    ngx_stream_server_traffic_status_conf_t  *stscf;

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "stream sts handler");

    ctx = ngx_stream_get_module_main_conf(s, ngx_stream_server_traffic_status_module);
    stscf = ngx_stream_get_module_srv_conf(s, ngx_stream_server_traffic_status_module);

    if (!ctx->enable || !stscf->enable) {
        return NGX_DECLINED;
    }
    if (stscf->shm_zone == NULL) {
        return NGX_DECLINED;
    }

    rc = ngx_stream_server_traffic_status_shm_add_server(s);
    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "handler::shm_add_server() failed");
    }

    rc = ngx_stream_server_traffic_status_shm_add_upstream(s);
    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "handler::shm_add_upstream() failed");
    }

    rc = ngx_stream_server_traffic_status_shm_add_filter(s);
    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "handler::shm_add_filter() failed");
    }

    return NGX_DECLINED;
}


ngx_msec_t
ngx_stream_server_traffic_status_current_msec(void)
{
    time_t           sec;
    ngx_uint_t       msec;
    struct timeval   tv;

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;

    return (ngx_msec_t) sec * 1000 + msec;
}


ngx_msec_int_t
ngx_stream_server_traffic_status_session_time(ngx_stream_session_t *s)
{
    ngx_time_t      *tp;
    ngx_msec_int_t   ms;

    tp = ngx_timeofday();

    ms = (ngx_msec_int_t)
             ((tp->sec - s->start_sec) * 1000 + (tp->msec - s->start_msec));
    return ngx_max(ms, 0);
}


ngx_msec_int_t
ngx_stream_server_traffic_status_upstream_response_time(ngx_stream_session_t *s, uintptr_t data)
{
    ngx_uint_t                    i;
    ngx_msec_int_t                ms;
    ngx_stream_upstream_state_t  *state;

    i = 0;
    ms = 0;
    state = s->upstream_states->elts;

    for ( ;; ) {

        if (data == 1) {
            if (state[i].first_byte_time == (ngx_msec_t) -1) {
                goto next;
            }

            ms += state[i].first_byte_time;

        } else if (data == 2 && state[i].connect_time != (ngx_msec_t) -1) {
            ms += state[i].connect_time;

        } else {
            ms += state[i].response_time;
        }

    next:

        if (++i == s->upstream_states->nelts) {
            break;
        }
    }

    return ngx_max(ms, 0);
}


static void
ngx_stream_server_traffic_status_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t                        **p;
    ngx_stream_server_traffic_status_node_t   *stsn, *stsnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            stsn = (ngx_stream_server_traffic_status_node_t *) &node->color;
            stsnt = (ngx_stream_server_traffic_status_node_t *) &temp->color;

            p = (ngx_memn2cmp(stsn->data, stsnt->data, stsn->len, stsnt->len) < 0)
                ? &temp->left
                : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_int_t
ngx_stream_server_traffic_status_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_stream_server_traffic_status_ctx_t  *octx = data;

    size_t                                   len;
    ngx_slab_pool_t                         *shpool;
    ngx_rbtree_node_t                       *sentinel;
    ngx_stream_server_traffic_status_ctx_t  *ctx;

    ctx = shm_zone->data;

    if (octx) {
        ctx->rbtree = octx->rbtree;
        return NGX_OK;
    }

    shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        ctx->rbtree = shpool->data;
        return NGX_OK;
    }

    ctx->rbtree = ngx_slab_alloc(shpool, sizeof(ngx_rbtree_t));
    if (ctx->rbtree == NULL) {
        return NGX_ERROR;
    }

    shpool->data = ctx->rbtree;

    sentinel = ngx_slab_alloc(shpool, sizeof(ngx_rbtree_node_t));
    if (sentinel == NULL) {
        return NGX_ERROR;
    }

    ngx_rbtree_init(ctx->rbtree, sentinel,
                    ngx_stream_server_traffic_status_rbtree_insert_value);

    len = sizeof(" in server_traffic_status_zone \"\"") + shm_zone->shm.name.len;

    shpool->log_ctx = ngx_slab_alloc(shpool, len);
    if (shpool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(shpool->log_ctx, " in server_traffic_status_zone \"%V\"%Z",
                &shm_zone->shm.name);

    return NGX_OK;
}


static char *
ngx_stream_server_traffic_status_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    u_char                                  *p;
    ssize_t                                  size;
    ngx_str_t                               *value, name, s;
    ngx_uint_t                               i;
    ngx_shm_zone_t                          *shm_zone;
    ngx_stream_server_traffic_status_ctx_t  *ctx;

    value = cf->args->elts;

    ctx = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_server_traffic_status_module);
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    ctx->enable = 1;

    ngx_str_set(&name, NGX_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_SHM_NAME);

    size = NGX_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_SHM_SIZE;

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "shared:", 7) == 0) {

            name.data = value[i].data + 7;

            p = (u_char *) ngx_strchr(name.data, ':');
            if (p == NULL) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid shared size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            name.len = p - name.data;

            s.data = p + 1;
            s.len = value[i].data + value[i].len - s.data;

            size = ngx_parse_size(&s);
            if (size == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid shared size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            if (size < (ssize_t) (8 * ngx_pagesize)) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "shared \"%V\" is too small", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    shm_zone = ngx_shared_memory_add(cf, &name, size,
                                     &ngx_stream_server_traffic_status_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (shm_zone->data) {
        ctx = shm_zone->data;

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "server_traffic_status: \"%V\" is already bound to key",
                           &name);

        return NGX_CONF_ERROR;
    }

    ctx->shm_name = name;
    ctx->shm_size = size;
    shm_zone->init = ngx_stream_server_traffic_status_init_zone;
    shm_zone->data = ctx;

    return NGX_CONF_OK;
}


static char *
ngx_stream_server_traffic_status_average_method(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_stream_server_traffic_status_conf_t *stscf = conf;

    char       *rv;
    ngx_int_t   rc;
    ngx_str_t  *value;

    value = cf->args->elts;

    cmd->offset = offsetof(ngx_stream_server_traffic_status_conf_t, average_method);
    cmd->post = &ngx_stream_server_traffic_status_average_method_post;

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


static char *
ngx_stream_server_traffic_status_histogram_buckets(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_stream_server_traffic_status_conf_t *stscf = conf;

    ngx_str_t                                          *value;
    ngx_int_t                                           n;
    ngx_uint_t                                          i;
    ngx_array_t                                        *histogram_buckets;
    ngx_stream_server_traffic_status_node_histogram_t  *buckets;

    histogram_buckets = ngx_array_create(cf->pool, 1,
                            sizeof(ngx_stream_server_traffic_status_node_histogram_t));
    if (histogram_buckets == NULL) {
        goto invalid;
    }

    value = cf->args->elts;

    /* arguments process */
    for (i = 1; i < cf->args->nelts; i++) {
        if (i > NGX_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_BUCKET_LEN) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "histogram bucket size exceeds %d",
                               NGX_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_BUCKET_LEN);
            break;
        }

        n = ngx_atofp(value[i].data, value[i].len, 3);
        if (n == NGX_ERROR || n == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[i]);
            goto invalid;
        }

        buckets = ngx_array_push(histogram_buckets);
        if (buckets == NULL) {
            goto invalid;
        }

        buckets->msec = (ngx_msec_int_t) n;
    }

    stscf->histogram_buckets = histogram_buckets;

    return NGX_CONF_OK;

invalid:

    return NGX_CONF_ERROR;
}


static ngx_int_t
ngx_stream_server_traffic_status_preconfiguration(ngx_conf_t *cf)
{
    return ngx_stream_server_traffic_status_add_variables(cf);
}


static void *
ngx_stream_server_traffic_status_create_main_conf(ngx_conf_t *cf)
{
    ngx_stream_server_traffic_status_ctx_t  *ctx;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_stream_server_traffic_status_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->enable = NGX_CONF_UNSET;
    ctx->filter_check_duplicate = NGX_CONF_UNSET;
    ctx->limit_check_duplicate = NGX_CONF_UNSET;
    ctx->upstream = NGX_CONF_UNSET_PTR;

    return ctx;
}


static char *
ngx_stream_server_traffic_status_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_stream_server_traffic_status_ctx_t  *ctx = conf;

    ngx_int_t                                 rc;
    ngx_stream_server_traffic_status_conf_t  *stscf;

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, cf->log, 0,
                   "stream sts init main conf");

    stscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_server_traffic_status_module);

    if (stscf->filter_check_duplicate != 0) {
        rc = ngx_stream_server_traffic_status_filter_unique(cf->pool, &ctx->filter_keys);
        if (rc != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "init_main_conf::filter_unique() failed");
            return NGX_CONF_ERROR;
        }
    }

    if (stscf->limit_check_duplicate != 0) {
        rc = ngx_stream_server_traffic_status_limit_traffic_unique(cf->pool, &ctx->limit_traffics);
        if (rc != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "init_main_conf::limit_traffic_unique(server) failed");
            return NGX_CONF_ERROR;
        }

        rc = ngx_stream_server_traffic_status_limit_traffic_unique(cf->pool,
                                                                &ctx->limit_filter_traffics);
        if (rc != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "init_main_conf::limit_traffic_unique(filter) failed");
            return NGX_CONF_ERROR;
        }
    }

    ngx_conf_init_value(ctx->enable, 0);
    ngx_conf_init_value(ctx->filter_check_duplicate, stscf->filter_check_duplicate);
    ngx_conf_init_value(ctx->limit_check_duplicate, stscf->limit_check_duplicate);
    ngx_conf_init_ptr_value(ctx->upstream, ngx_stream_conf_get_module_main_conf(cf,
                                           ngx_stream_upstream_module));

    return NGX_CONF_OK;
}


static void *
ngx_stream_server_traffic_status_create_loc_conf(ngx_conf_t *cf)
{
    ngx_stream_server_traffic_status_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_server_traffic_status_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->shm_zone = { NULL, ... };
     *     conf->shm_name = { 0, NULL };
     *     conf->enable = 0;
     *     conf->filter = 0;
     *     conf->filter_check_duplicate = 0;
     *     conf->filter_keys = { NULL, ... };
     *
     *     conf->limit = 0;
     *     conf->limit_check_duplicate = 0;
     *     conf->limit_traffics = { NULL, ... };
     *     conf->limit_filter_traffics = { NULL, ... };
     *
     *     conf->stats = { 0, ... };
     *     conf->start_msec = 0;
     *
     *     conf->average_method = 0;
     *     conf->average_period = 0;
     *     conf->histogram_buckets = { NULL, ... };
     */

    conf->shm_zone = NGX_CONF_UNSET_PTR;
    conf->enable = NGX_CONF_UNSET;
    conf->filter = NGX_CONF_UNSET;
    conf->filter_check_duplicate = NGX_CONF_UNSET;
    conf->limit = NGX_CONF_UNSET;
    conf->limit_check_duplicate = NGX_CONF_UNSET;
    conf->start_msec = ngx_stream_server_traffic_status_current_msec();

    conf->average_method = NGX_CONF_UNSET;
    conf->average_period = NGX_CONF_UNSET_MSEC;
    conf->histogram_buckets = NGX_CONF_UNSET_PTR;

    conf->node_caches = ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t *)
                                    * (NGX_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_FG + 1));
    conf->node_caches[NGX_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_NO] = NULL;
    conf->node_caches[NGX_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_UA] = NULL;
    conf->node_caches[NGX_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_UG] = NULL;
    conf->node_caches[NGX_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_FG] = NULL;

    return conf;
}


static char *
ngx_stream_server_traffic_status_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_stream_server_traffic_status_conf_t *prev = parent;
    ngx_stream_server_traffic_status_conf_t *conf = child;

    ngx_int_t                                rc;
    ngx_str_t                                name;
    ngx_shm_zone_t                          *shm_zone;
    ngx_stream_server_traffic_status_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, cf->log, 0,
                   "stream sts merge loc conf");

    ctx = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_server_traffic_status_module);

    if (!ctx->enable) {
        return NGX_CONF_OK;
    }

    if (conf->filter_keys == NULL) {
        conf->filter_keys = prev->filter_keys;

    } else {
        if (conf->filter_check_duplicate == NGX_CONF_UNSET) {
            conf->filter_check_duplicate = ctx->filter_check_duplicate;
        }
        if (conf->filter_check_duplicate != 0) {
            rc = ngx_stream_server_traffic_status_filter_unique(cf->pool, &conf->filter_keys);
            if (rc != NGX_OK) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "mere_loc_conf::filter_unique() failed");
                return NGX_CONF_ERROR;
            }
        }
    }

    if (conf->limit_traffics == NULL) {
        conf->limit_traffics = prev->limit_traffics;

    } else {
        if (conf->limit_check_duplicate == NGX_CONF_UNSET) {
            conf->limit_check_duplicate = ctx->limit_check_duplicate;
        }

        if (conf->limit_check_duplicate != 0) {
            rc = ngx_stream_server_traffic_status_limit_traffic_unique(cf->pool,
                                                                    &conf->limit_traffics);
            if (rc != NGX_OK) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "mere_loc_conf::limit_traffic_unique(server) failed");
                return NGX_CONF_ERROR;
            }
        }
    }

    if (conf->limit_filter_traffics == NULL) {
        conf->limit_filter_traffics = prev->limit_filter_traffics;

    } else {
        if (conf->limit_check_duplicate == NGX_CONF_UNSET) {
            conf->limit_check_duplicate = ctx->limit_check_duplicate;
        }

        if (conf->limit_check_duplicate != 0) {
            rc = ngx_stream_server_traffic_status_limit_traffic_unique(cf->pool,
                                                                    &conf->limit_filter_traffics);
            if (rc != NGX_OK) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "mere_loc_conf::limit_traffic_unique(filter) failed");
                return NGX_CONF_ERROR;
            }
        }
    }

    ngx_conf_merge_ptr_value(conf->shm_zone, prev->shm_zone, NULL);
    ngx_conf_merge_value(conf->enable, prev->enable, 1);
    ngx_conf_merge_value(conf->filter, prev->filter, 1);
    ngx_conf_merge_value(conf->filter_check_duplicate, prev->filter_check_duplicate, 1);
    ngx_conf_merge_value(conf->limit, prev->limit, 1);
    ngx_conf_merge_value(conf->limit_check_duplicate, prev->limit_check_duplicate, 1);

    ngx_conf_merge_value(conf->average_method, prev->average_method,
                         NGX_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_AMM);
    ngx_conf_merge_msec_value(conf->average_period, prev->average_period,
                              NGX_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_AVG_PERIOD * 1000);
    ngx_conf_merge_ptr_value(conf->histogram_buckets, prev->histogram_buckets, NULL);

    name = ctx->shm_name;

    shm_zone = ngx_shared_memory_add(cf, &name, 0,
                                     &ngx_stream_server_traffic_status_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->shm_zone = shm_zone;
    conf->shm_name = name;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_stream_server_traffic_status_init(ngx_conf_t *cf)
{
    ngx_stream_handler_pt        *h;
    ngx_stream_core_main_conf_t  *cmcf;

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, cf->log, 0,
                   "stream sts init");

    cmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_STREAM_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_stream_server_traffic_status_limit_handler;

    h = ngx_array_push(&cmcf->phases[NGX_STREAM_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_stream_server_traffic_status_handler;

    return NGX_OK;
}

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
