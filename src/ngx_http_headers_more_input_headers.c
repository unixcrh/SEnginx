/* Copyright (C) agentzh */

#define DDEBUG 0

#include "ddebug.h"

#include "ngx_http_headers_more_input_headers.h"
#include "ngx_http_headers_more_util.h"
#include <ctype.h>

/* config time */

static char *
ngx_http_headers_more_parse_directive(ngx_conf_t *cf, ngx_command_t *ngx_cmd,
        void *conf, ngx_http_headers_more_opcode_t opcode);

/* request time */

static ngx_flag_t ngx_http_headers_more_check_type(ngx_http_request_t *r, ngx_array_t *types);

static ngx_int_t ngx_http_set_header(ngx_http_request_t *r,
    ngx_http_headers_more_header_val_t *hv, ngx_str_t *value);

static ngx_int_t ngx_http_set_header_helper(ngx_http_request_t *r,
    ngx_http_headers_more_header_val_t *hv, ngx_str_t *value,
    ngx_table_elt_t **output_header);

static ngx_int_t ngx_http_set_builtin_header(ngx_http_request_t *r,
    ngx_http_headers_more_header_val_t *hv, ngx_str_t *value);

static ngx_int_t ngx_http_set_content_length_header(ngx_http_request_t *r,
    ngx_http_headers_more_header_val_t *hv, ngx_str_t *value);

static ngx_int_t ngx_http_clear_builtin_header(ngx_http_request_t *r,
    ngx_http_headers_more_header_val_t *hv, ngx_str_t *value);

static ngx_int_t ngx_http_clear_content_length_header(ngx_http_request_t *r,
        ngx_http_headers_more_header_val_t *hv, ngx_str_t *value);

static ngx_int_t ngx_http_set_host_header(ngx_http_request_t *r,
        ngx_http_headers_more_header_val_t *hv, ngx_str_t *value);

static ngx_http_headers_more_set_header_t ngx_http_headers_more_set_handlers[] = {

    { ngx_string("Host"),
                 offsetof(ngx_http_headers_in_t, host),
                 ngx_http_set_host_header },

    { ngx_string("Connection"),
                 offsetof(ngx_http_headers_in_t, connection),
                 ngx_http_set_builtin_header },

    { ngx_string("If-Modified-Since"),
                 offsetof(ngx_http_headers_in_t, if_modified_since),
                 ngx_http_set_builtin_header },

    { ngx_string("User-Agent"),
                 offsetof(ngx_http_headers_in_t, user_agent),
                 ngx_http_set_builtin_header },

    { ngx_string("Referer"),
                 offsetof(ngx_http_headers_in_t, referer),
                 ngx_http_set_builtin_header },

    { ngx_string("Content-Type"),
                 offsetof(ngx_http_headers_in_t, content_type),
                 ngx_http_set_builtin_header },

    { ngx_string("Range"),
                 offsetof(ngx_http_headers_in_t, range),
                 ngx_http_set_builtin_header },

    { ngx_string("If-Range"),
                 offsetof(ngx_http_headers_in_t, if_range),
                 ngx_http_set_builtin_header },

    { ngx_string("Transfer-Encoding"),
                 offsetof(ngx_http_headers_in_t, transfer_encoding),
                 ngx_http_set_builtin_header },

    { ngx_string("Expect"),
                 offsetof(ngx_http_headers_in_t, expect),
                 ngx_http_set_builtin_header },

    { ngx_string("Authorization"),
                 offsetof(ngx_http_headers_in_t, authorization),
                 ngx_http_set_builtin_header },

    { ngx_string("Keep-Alive"),
                 offsetof(ngx_http_headers_in_t, keep_alive),
                 ngx_http_set_builtin_header },

    { ngx_string("Content-Length"),
                 offsetof(ngx_http_headers_in_t, content_length),
                 ngx_http_set_content_length_header },

    { ngx_null_string, 0, ngx_http_set_header }
};

/* request time implementation */

ngx_int_t
ngx_http_headers_more_exec_input_cmd(ngx_http_request_t *r,
        ngx_http_headers_more_cmd_t *cmd)
{
    ngx_str_t                                   value;
    ngx_http_headers_more_header_val_t          *h;
    ngx_uint_t                                  i;

    if (!cmd->headers) {
        return NGX_OK;
    }

    if (cmd->types) {
        if ( ! ngx_http_headers_more_check_type(r, cmd->types) ) {
            return NGX_OK;
        }
    }

    h = cmd->headers->elts;
    for (i = 0; i < cmd->headers->nelts; i++) {

        if (ngx_http_complex_value(r, &h[i].value, &value) != NGX_OK) {
            return NGX_ERROR;
        }

        if (h[i].handler(r, &h[i], &value) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_set_header(ngx_http_request_t *r, ngx_http_headers_more_header_val_t *hv,
        ngx_str_t *value)
{
    return ngx_http_set_header_helper(r, hv, value, NULL);
}

static ngx_int_t
ngx_http_set_header_helper(ngx_http_request_t *r, ngx_http_headers_more_header_val_t *hv,
        ngx_str_t *value, ngx_table_elt_t **output_header)
{
    ngx_table_elt_t             *h;
    ngx_list_part_t             *part;
    ngx_uint_t                  i;

    dd("entered set_header (input)");

    part = &r->headers_in.headers.part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (h[i].key.len == hv->key.len
                && ngx_strncasecmp(h[i].key.data,
                    hv->key.data,
                    h[i].key.len) == 0)
        {
            if (value->len == 0) {
                h[i].hash = 0;
            }

            h[i].value = *value;

            if (output_header) {
                *output_header = &h[i];
            }

            return NGX_OK;
        }
    }

    if (value->len == 0 || hv->replace){
      return NGX_OK;
    }

    h = ngx_list_push(&r->headers_in.headers);

    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    h->hash = hv->hash;
    h->key = hv->key;
    h->value = *value;

    if (output_header) {
        *output_header = h;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_set_builtin_header(ngx_http_request_t *r, ngx_http_headers_more_header_val_t *hv,
        ngx_str_t *value)
{
    ngx_table_elt_t  *h, **old;

    dd("entered set_builtin_header (input)");

    if (hv->offset) {
        old = (ngx_table_elt_t **) ((char *) &r->headers_in + hv->offset);

    } else {
        old = NULL;
    }

    if (old == NULL || *old == NULL) {
        return ngx_http_set_header_helper(r, hv, value, old);
    }

    h = *old;

    if (value->len == 0) {
        h->hash = 0;
        h->value = *value;
        return NGX_OK;
    }

    h->hash = hv->hash;
    h->key = hv->key;
    h->value = *value;

    return NGX_OK;
}

static ngx_int_t
ngx_http_set_host_header(ngx_http_request_t *r, ngx_http_headers_more_header_val_t *hv,
        ngx_str_t *value)
{
    dd("server new value len: %d", value->len);

    r->headers_in.server = *value;

    return ngx_http_set_builtin_header(r, hv, value);
}

static ngx_int_t
ngx_http_set_content_length_header(ngx_http_request_t *r, ngx_http_headers_more_header_val_t *hv,
        ngx_str_t *value)
{
    off_t           len;

    if (value->len == 0) {
        return ngx_http_clear_content_length_header(r, hv, value);
    }

    len = ngx_atosz(value->data, value->len);
    if (len == NGX_ERROR) {
        return NGX_ERROR;
    }

    dd("reset headers_in.content_length_n to %d", (int)len);

    r->headers_in.content_length_n = len;

    return ngx_http_set_builtin_header(r, hv, value);
}

static ngx_int_t
ngx_http_clear_content_length_header(ngx_http_request_t *r, ngx_http_headers_more_header_val_t *hv,
        ngx_str_t *value)
{
    r->headers_in.content_length_n = -1;

    return ngx_http_clear_builtin_header(r, hv, value);
}

static ngx_int_t
ngx_http_clear_builtin_header(ngx_http_request_t *r, ngx_http_headers_more_header_val_t *hv,
        ngx_str_t *value)
{
    value->len = 0;
    return ngx_http_set_builtin_header(r, hv, value);
}

char *
ngx_http_headers_more_set_input_headers(ngx_conf_t *cf,
        ngx_command_t *cmd, void *conf)
{
    return ngx_http_headers_more_parse_directive(cf, cmd, conf,
            ngx_http_headers_more_opcode_set);
}

char *
ngx_http_headers_more_clear_input_headers(ngx_conf_t *cf,
        ngx_command_t *cmd, void *conf)
{
    return ngx_http_headers_more_parse_directive(cf, cmd, conf,
            ngx_http_headers_more_opcode_clear);
}

static ngx_flag_t
ngx_http_headers_more_check_type(ngx_http_request_t *r, ngx_array_t *types)
{
    ngx_uint_t          i;
    ngx_str_t           *t;
    ngx_str_t           actual_type;

    if (r->headers_in.content_type == NULL) {
        return 0;
    }

    actual_type = r->headers_in.content_type->value;
    if (actual_type.len == 0) {
        return 0;
    }

    dd("headers_in->content_type: %s (len %d)",
            actual_type.data,
            actual_type.len);

    t = types->elts;
    for (i = 0; i < types->nelts; i++) {
        dd("...comparing with type [%s] (len %d)", t[i].data, t[i].len);
        if (actual_type.len == t[i].len
                && ngx_strncmp(actual_type.data,
                    t[i].data, t[i].len) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/* config time implementation */

static char *
ngx_http_headers_more_parse_directive(ngx_conf_t *cf, ngx_command_t *ngx_cmd,
        void *conf, ngx_http_headers_more_opcode_t opcode)
{
    ngx_http_headers_more_conf_t        *hcf = conf;

    ngx_uint_t                          i;
    ngx_http_headers_more_cmd_t         *cmd;
    ngx_str_t                           *arg;
    ngx_flag_t                          ignore_next_arg;
    ngx_str_t                           *cmd_name;
    ngx_int_t                           rc;
    ngx_flag_t                          replace = 0;
    ngx_http_headers_more_header_val_t  *h;

    if (hcf->cmds == NULL) {
        hcf->cmds = ngx_array_create(cf->pool, 1,
                                        sizeof(ngx_http_headers_more_cmd_t));
    }

    cmd = ngx_array_push(hcf->cmds);

    if (cmd == NULL) {
        return NGX_CONF_ERROR;
    }

    cmd->headers = ngx_array_create(cf->pool, 1,
                            sizeof(ngx_http_headers_more_header_val_t));
    if (cmd->headers == NULL) {
        return NGX_CONF_ERROR;
    }

    cmd->types = ngx_array_create(cf->pool, 1,
                            sizeof(ngx_str_t));
    if (cmd->types == NULL) {
        return NGX_CONF_ERROR;
    }

    cmd->statuses = NULL;

    arg = cf->args->elts;

    cmd_name = &arg[0];

    ignore_next_arg = 0;

    for (i = 1; i < cf->args->nelts; i++) {
        if (ignore_next_arg) {
            ignore_next_arg = 0;
            continue;
        }

        if (arg[i].len == 0) {
            continue;
        }

        if (arg[i].data[0] != '-') {
            rc = ngx_http_headers_more_parse_header(cf, cmd_name,
                    &arg[i], cmd->headers, opcode,
                    ngx_http_headers_more_set_handlers);

            if (rc != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (arg[i].len == 2) {
            if (arg[i].data[1] == 't') {
                if (i == cf->args->nelts - 1) {
                    ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                          "%V: option -t takes an argument.",
                          cmd_name);

                    return NGX_CONF_ERROR;
                }

                rc = ngx_http_headers_more_parse_types(cf->log, cmd_name,
                        &arg[i + 1], cmd->types);

                if (rc != NGX_OK) {
                    return NGX_CONF_ERROR;
                }

                ignore_next_arg = 1;

                continue;
            } else if (arg[i].data[1] == 'r') {
              dd("Found replace flag");
              replace = 1;
              continue;
            }
        }

        ngx_log_error(NGX_LOG_ERR, cf->log, 0,
              "%V: invalid option name: \"%V\"",
              cmd_name, &arg[i]);

        return NGX_CONF_ERROR;
    }

    dd("Found %d types, and %d headers",
            cmd->types->nelts,
            cmd->headers->nelts);

    if (cmd->headers->nelts == 0) {
        ngx_pfree(cf->pool, cmd->headers);
        cmd->headers = NULL;
    } else if (replace) {
          h = cmd->headers->elts;
          for (i = 0; i < cmd->headers->nelts; i++) {
            h[i].replace = 1;
          }
    }

    if (cmd->types->nelts == 0) {
        ngx_pfree(cf->pool, cmd->types);
        cmd->types = NULL;
    }

    cmd->is_input = 1;

    ngx_http_headers_more_access_input_headers = 1;

    return NGX_CONF_OK;
}


