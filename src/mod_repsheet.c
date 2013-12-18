/*
  Copyright 2013 Aaron Bedra

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <stdio.h>

#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_config.h"
#include "http_log.h"

#include "hiredis/hiredis.h"

#include "mod_repsheet.h"
#include "proxy.h"
#include "mod_security.h"
#include "repsheet.h"

const char *repsheet_set_enabled(cmd_parms *cmd, void *cfg, const char *arg)
{
  if (strcasecmp(arg, "on") == 0) {
    config.repsheet_enabled = 1;
    return NULL;
  } else if (strcasecmp(arg, "off") == 0) {
    config.repsheet_enabled = 0;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetEnabled directive must be set to On or Off";
  }
}

const char *repsheet_set_recorder_enabled(cmd_parms *cmd, void *cfg, const char *arg)
{
  if (strcasecmp(arg, "on") == 0) {
    config.recorder_enabled = 1;
    return NULL;
  } else if (strcasecmp(arg, "off") == 0) {
    config.recorder_enabled = 0;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetRecorder directive must be set to On or Off";
  }
}

const char *repsheet_set_filter_enabled(cmd_parms *cmd, void *cfg, const char *arg)
{
  if (strcasecmp(arg, "on") == 0) {
    config.filter_enabled = 1;
    return NULL;
  } else if (strcasecmp(arg, "off") == 0) {
    config.filter_enabled = 0;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetFilter directive must be set to On or Off";
  }
}

const char *repsheet_set_geoip_enabled(cmd_parms *cmd, void *cfg, const char *arg)
{
  if (strcasecmp(arg, "on") == 0) {
    config.geoip_enabled = 1;
    return NULL;
  } else if (strcasecmp(arg, "off") == 0) {
    config.geoip_enabled = 0;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetGeoIP directive must be set to On or Off";
  }
}

const char *repsheet_set_proxy_headers_enabled(cmd_parms *cmd, void *cfg, const char *arg)
{
  if (strcasecmp(arg, "on") == 0) {
    config.proxy_headers_enabled = 1;
    return NULL;
  } else if (strcasecmp(arg, "off") == 0) {
    config.proxy_headers_enabled = 0;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetProxyHeaders directive must be set to On or Off";
  }
}

const char *repsheet_set_timeout(cmd_parms *cmd, void *cfg, const char *arg)
{
  int timeout = atoi(arg) * 1000;

  if (timeout > 0) {
    config.redis_timeout = timeout;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetRedisTimeout directive must be a number";
  }
}

const char *repsheet_set_host(cmd_parms *cmd, void *cfg, const char *arg)
{
  config.redis_host = arg;
  return NULL;
}

const char *repsheet_set_port(cmd_parms *cmd, void *cfg, const char *arg)
{
  int port = atoi(arg);

  if (port > 0) {
    config.redis_port = port;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetRedisPort directive must be a number";
  }
}

const char *repsheet_set_length(cmd_parms *cmd, void *cfg, const char *arg)
{
  int length = atoi(arg);

  if (length > 0) {
    config.redis_max_length = length;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetRedisMaxLength directive must be a number";
  }
}

const char *repsheet_set_expiry(cmd_parms *cmd, void *cfg, const char *arg)
{
  config.redis_expiry = atoi(arg) * 60 * 60;
  return NULL;
}

const char *repsheet_set_prefix(cmd_parms *cmd, void *cfg, const char *arg)
{
  config.prefix = arg;
  return NULL;
}

const char *repsheet_set_action(cmd_parms *cmd, void *cfg, const char *arg)
{
  if (strcasecmp(arg, "notify") == 0) {
    config.action = NOTIFY;
    return NULL;
  } else if (strcasecmp(arg, "block") == 0) {
    config.action = BLOCK;
    return NULL;
  } else if (strcasecmp(arg, "allow") == 0) {
    config.action = ALLOW;
    return NULL;
  } else {
    return "[ModRepsheet] - The RepsheetAction directive must be set to Block, Notify, or Allow";
  }
}

const char *repsheet_set_modsecurity_anomaly_threshold(cmd_parms *cmd, void *cfg, const char *arg)
{
  int threshold = strtol(arg, 0, 10);

  if (threshold > 0) {
    config.modsecurity_anomaly_threshold = threshold;
    return NULL;
  } else {
    return "[ModRepsheet] - The ModSecurity anomaly threshold directive must be a number";
  }
}

static const command_rec repsheet_directives[] =
  {
    AP_INIT_TAKE1("repsheetEnabled",         repsheet_set_enabled,               NULL, RSRC_CONF, "Enable or disable mod_repsheet"),
    AP_INIT_TAKE1("repsheetRecorder",        repsheet_set_recorder_enabled,      NULL, RSRC_CONF, "Enable or disable repsheet recorder"),
    AP_INIT_TAKE1("repsheetFilter",          repsheet_set_filter_enabled,        NULL, RSRC_CONF, "Enable or disable repsheet ModSecurity filter"),
    AP_INIT_TAKE1("repsheetGeoIP",           repsheet_set_geoip_enabled,         NULL, RSRC_CONF, "Enable or disable repsheet GeoIP filter"),
    AP_INIT_TAKE1("repsheetAnomalyThreshold",repsheet_set_modsecurity_anomaly_threshold, NULL, RSRC_CONF, "Set block threshold"),
    AP_INIT_TAKE1("repsheetProxyHeaders",    repsheet_set_proxy_headers_enabled, NULL, RSRC_CONF, "Enable or disable proxy header scanning"),
    AP_INIT_TAKE1("repsheetAction",          repsheet_set_action,                NULL, RSRC_CONF, "Set the action"),
    AP_INIT_TAKE1("repsheetPrefix",          repsheet_set_prefix,                NULL, RSRC_CONF, "Set the log prefix"),
    AP_INIT_TAKE1("repsheetRedisTimeout",    repsheet_set_timeout,               NULL, RSRC_CONF, "Set the Redis timeout"),
    AP_INIT_TAKE1("repsheetRedisHost",       repsheet_set_host,                  NULL, RSRC_CONF, "Set the Redis host"),
    AP_INIT_TAKE1("repsheetRedisPort",       repsheet_set_port,                  NULL, RSRC_CONF, "Set the Redis port"),
    AP_INIT_TAKE1("repsheetRedisMaxLength",  repsheet_set_length,                NULL, RSRC_CONF, "Last n requests kept per IP"),
    AP_INIT_TAKE1("repsheetRedisExpiry",     repsheet_set_expiry,                NULL, RSRC_CONF, "Number of hours before records expire"),
    { NULL }
  };

static redisContext *get_redis_context(request_rec *r)
{
  redisContext *context;
  struct timeval timeout = {0, (config.redis_timeout > 0) ? config.redis_timeout : 10000};

  context = redisConnectWithTimeout((char*) config.redis_host, config.redis_port, timeout);
  if (context == NULL || context->err) {
    if (context) {
      ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "%s Redis Connection Error: %s", config.prefix, context->errstr);
      redisFree(context);
    } else {
      ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "%s Connection Error: can't allocate redis context", config.prefix);
    }
    return NULL;
  } else {
    return context;
  }
}

static char *remote_address(request_rec *r)
{
# if AP_SERVER_MAJORVERSION_NUMBER == 2 && AP_SERVER_MINORVERSION_NUMBER == 4
  char *connected_ip = r->useragent_ip;
# else
  char *connected_ip = r->connection->remote_ip;
#endif

  if (config.proxy_headers_enabled) {
    char *address = process_headers((char*)apr_table_get(r->headers_in, "X-Forwarded-For"));

    if (address == NULL) {
      return connected_ip;
    }

    return address;
  } else {
    return connected_ip;
  }
}

static int repsheet_mod_security_filter(request_rec *r)
{
  if (!config.repsheet_enabled || !config.filter_enabled || !ap_is_initial_req(r)) {
    return DECLINED;
  }

  char *x_waf_score = (char *)apr_table_get(r->headers_in, "X-WAF-Score");
  int anomaly_score = modsecurity_total(x_waf_score);
  if (anomaly_score >= config.modsecurity_anomaly_threshold) {
    //TODO: Blacklist when this is hit
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "%s %s was blocked by Repsheet. ModSecurity anomaly score was %d", config.prefix, remote_address(r), anomaly_score);
    return HTTP_FORBIDDEN;
  }

  char *waf_events = (char *)apr_table_get(r->headers_in, "X-WAF-Events");

  if (!waf_events) {
    return DECLINED;
  }

  redisContext *context = get_redis_context(r);

  if (context == NULL) {
    return DECLINED;
  }

  int i, m;
  char **events;

  m = matches(waf_events);

  if (m > 0) {
    events = malloc(m * sizeof(char*));
    for(i = 0; i < m; i++) {
      events[i] = malloc(i * sizeof(char));
    }

    process_mod_security_headers(waf_events, events);

    char *ip = remote_address(r);

    for(i = 0; i < m; i++) {
      freeReplyObject(redisCommand(context, "ZINCRBY %s:detected 1 %s", ip, events[i]));
      freeReplyObject(redisCommand(context, "SET %s:repsheet true", ip));
      if (config.redis_expiry > 0) {
        freeReplyObject(redisCommand(context, "EXPIRE %s:detected %d", ip, config.redis_expiry));
        freeReplyObject(redisCommand(context, "EXPIRE %s:repsheet %d", ip, config.redis_expiry));
      }
    }
    free(events);
  }

  redisFree(context);

  return DECLINED;
}

static int hook_post_config(apr_pool_t *mp, apr_pool_t *mp_log, apr_pool_t *mp_temp, server_rec *s) {
  void *init_flag = NULL;
  int first_time = 0;

  apr_pool_userdata_get(&init_flag, "mod_repsheet-init-flag", s->process->pool);

  if (init_flag == NULL) {
    first_time = 1;
    apr_pool_userdata_set((const void *)1, "mod_repsheet-init-flag", apr_pool_cleanup_null, s->process->pool);
    ap_log_error(APLOG_MARK, APLOG_NOTICE | APLOG_NOERRNO, 0, s, "ModRepsheet for Apache %s (%s) loaded", REPSHEET_VERSION, REPSHEET_URL);
    return OK;
  }

  return OK;
}

static int repsheet_action(actor_t *actor)
{
  if (actor->whitelisted) {
    return ALLOW;
  }

  if (actor->blacklisted) {
    return BLOCK;
  } else if (actor->offender) {
    if (config.action == BLOCK) {
      return BLOCK;
    } else {
      return NOTIFY;
    }
  }

  return ALLOW;
}

static void record(redisContext *context, request_rec *r, actor_t *actor)
{
  apr_time_exp_t start;
  char           timestamp[50];

  apr_time_exp_gmt(&start, r->request_time);
  sprintf(timestamp, "%d/%d/%d %d:%d:%d.%d", (start.tm_mon + 1), start.tm_mday, (1900 + start.tm_year), start.tm_hour, start.tm_min, start.tm_sec, start.tm_usec);

  repsheet_record(context, timestamp, apr_table_get(r->headers_in, "User-Agent"), r->method, r->uri, r->args, actor->address, config.redis_max_length, config.redis_expiry);
}

static int act_and_record(request_rec *r)
{
  if (!config.repsheet_enabled || !ap_is_initial_req(r)) {
    return DECLINED;
  }

  redisContext *context = get_redis_context(r);

  if (context == NULL) {
    return DECLINED;
  }

  actor_t actor;
  repsheet_init_actor(&actor);

  actor.address = remote_address(r);

  repsheet_score_actor(context, &actor);

  int action;

  action = repsheet_action(&actor);
  if (action == BLOCK) {
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "%s %s was blocked by the repsheet", config.prefix, actor.address);
    redisFree(context);
    return HTTP_FORBIDDEN;
  } else if (action == NOTIFY) {
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "%s IP Address %s was found on the repsheet. No action taken", config.prefix, actor.address);
    apr_table_set(r->headers_in, "X-Repsheet", "true");
  }

  if (config.recorder_enabled) {
    record(context, r, &actor);
  }

  redisFree(context);

  return DECLINED;
}

static void register_hooks(apr_pool_t *pool)
{
  ap_hook_post_config(hook_post_config, NULL, NULL, APR_HOOK_REALLY_LAST);
  ap_hook_post_read_request(act_and_record, NULL, NULL, APR_HOOK_LAST);
  ap_hook_fixups(repsheet_mod_security_filter, NULL, NULL, APR_HOOK_REALLY_LAST);
}

module AP_MODULE_DECLARE_DATA repsheet_module = {
  STANDARD20_MODULE_STUFF,
  NULL,                /* Per-directory configuration handler */
  NULL,                /* Merge handler for per-directory configurations */
  NULL,                /* Per-server configuration handler */
  NULL,                /* Merge handler for per-server configurations */
  repsheet_directives, /* Any directives we may have for httpd */
  register_hooks       /* Our hook registering function */
};
