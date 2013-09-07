#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "hiredis/hiredis.h"


#define REPSHEET_ACTION_NOTIFY  0
#define REPSHEET_ACTION_BLOCK   1


static ngx_conf_bitmask_t ngx_http_repsheet_action_mask[] = {
  { ngx_string("notify"), REPSHEET_ACTION_NOTIFY },
  { ngx_string("block"), REPSHEET_ACTION_BLOCK },
  { ngx_null_string, 0 }
};

typedef struct {
  ngx_str_t  host;
  ngx_uint_t port;
  ngx_uint_t timeout;
  ngx_uint_t max_length;
  ngx_uint_t expiry;

} repsheet_redis_t;

typedef struct {
  repsheet_redis_t redis;

} repsheet_main_conf_t;


typedef struct {
  ngx_flag_t enabled;
  ngx_flag_t record;
  ngx_flag_t proxy_headers;
  ngx_uint_t action;

} repsheet_loc_conf_t;


static redisContext*
get_redis_context(ngx_http_request_t *r)
{
  redisContext *context;
  struct timeval timeout = {0, 5000};

  context = redisConnectWithTimeout("localhost", 6379, timeout);
  if (context == NULL || context->err) {
    if (context) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "%s Redis Connection Error: %s", "[repsheet]", context->errstr);
      redisFree(context);
    } else {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "%s %s Connection Error: can't allocate redis context", "[repsheet]");
    }
    return NULL;
  } else {
    return context;
  }
}


static ngx_int_t
ngx_http_repsheet_handler(ngx_http_request_t *r)
{
  if (r->main->internal) {
    return NGX_DECLINED;
  }

  redisContext *context;
  redisReply *reply;

  context = get_redis_context(r);
  if (context == NULL) {
    return NGX_DECLINED;
  }

  ngx_str_t address = r->connection->addr_text;

  reply = redisCommand(context, "GET %s:repsheet:blacklist", address.data);
  if (reply->str && strcmp(reply->str, "true") == 0) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "%s %s was blocked by the repsheet", "[repsheet]", address.data);
    freeReplyObject(reply);
    redisFree(context);
    return NGX_HTTP_FORBIDDEN;
  }

  redisFree(context);

  r->main->internal = 1;
  return NGX_DECLINED;
}


static ngx_int_t
ngx_http_repsheet_init(ngx_conf_t *cf)
{
  ngx_http_handler_pt *h;
  ngx_http_core_main_conf_t *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_http_repsheet_handler;

  return NGX_OK;
}


static ngx_command_t ngx_http_repsheet_commands[] = {
  /* Location-specific commands */
  {
    ngx_string("repsheet"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(repsheet_loc_conf_t, enabled),
    NULL
  },
  {
    ngx_string("repsheet_recorder"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(repsheet_loc_conf_t, record),
    NULL
  },
  {
    ngx_string("repsheet_proxy_headers"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(repsheet_loc_conf_t, proxy_headers),
    NULL
  },
  {
    ngx_string("repsheet_action"),
    NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
    ngx_conf_set_bitmask_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(repsheet_loc_conf_t, action),
    &ngx_http_repsheet_action_mask
  },

  /* Main-specific commands */
  {
    ngx_string("repsheet_redis_host"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(repsheet_main_conf_t, redis.host),
    NULL
  },
  {
    ngx_string("repsheet_redis_port"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_num_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(repsheet_main_conf_t, redis.port),
    NULL
  },
  {
    ngx_string("repsheet_redis_timeout"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_num_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(repsheet_main_conf_t, redis.timeout),
    NULL
  },
  {
    ngx_string("repsheet_redis_max_length"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_num_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(repsheet_main_conf_t, redis.max_length),
    NULL
  },
  {
    ngx_string("repsheet_redis_expiry"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_num_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(repsheet_main_conf_t, redis.expiry),
    NULL
  },

  ngx_null_command
};


static void*
ngx_http_repsheet_create_main_conf(ngx_conf_t *cf)
{
  repsheet_main_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(repsheet_main_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->redis.port = NGX_CONF_UNSET_UINT;
  conf->redis.timeout = NGX_CONF_UNSET_UINT;
  conf->redis.max_length = NGX_CONF_UNSET_UINT;
  conf->redis.expiry = NGX_CONF_UNSET_UINT;

  return conf;
}


static void*
ngx_http_repsheet_create_loc_conf(ngx_conf_t *cf)
{
  repsheet_loc_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(repsheet_loc_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->enabled = NGX_CONF_UNSET;
  conf->record = NGX_CONF_UNSET;
  conf->proxy_headers = NGX_CONF_UNSET;
  conf->action = NGX_CONF_UNSET_UINT;

  return conf;
}


static char*
ngx_http_repsheet_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  repsheet_loc_conf_t *prev = (repsheet_loc_conf_t *)parent;
  repsheet_loc_conf_t *conf = (repsheet_loc_conf_t *)child;

  ngx_conf_merge_value(conf->enabled, prev->enabled, 0);
  ngx_conf_merge_value(conf->record, prev->record, 0);
  ngx_conf_merge_value(conf->proxy_headers, prev->proxy_headers, 0);
  ngx_conf_merge_bitmask_value(conf->action, prev->action, 0);

  return NGX_CONF_OK;
}


static ngx_http_module_t ngx_http_repsheet_module_ctx = {
  NULL,                               /* preconfiguration */
  ngx_http_repsheet_init,             /* postconfiguration */
  ngx_http_repsheet_create_main_conf, /* create main configuration */
  NULL,                               /* init main configuration */
  NULL,                               /* create server configuration */
  NULL,                               /* merge server configuration */
  ngx_http_repsheet_create_loc_conf,  /* create location configuration */
  ngx_http_repsheet_merge_loc_conf    /* merge location configuration */
};


ngx_module_t ngx_http_repsheet_module = {
  NGX_MODULE_V1,
  &ngx_http_repsheet_module_ctx,    /* module context */
  ngx_http_repsheet_commands,       /* module directives */
  NGX_HTTP_MODULE,               /* module type */
  NULL,                          /* init master */
  NULL,                          /* init module */
  NULL,                          /* init process */
  NULL,                          /* init thread */
  NULL,                          /* exit thread */
  NULL,                          /* exit process */
  NULL,                          /* exit master */
  NGX_MODULE_V1_PADDING
};
