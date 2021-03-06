//
//  ngx_http_google_response.c
//  nginx
//
//  Created by Cube on 14/12/15.
//  Copyright (c) 2014年 Cube. All rights reserved.
//

#include "ngx_http_google_util.h"
#include "ngx_http_google_response.h"

static ngx_int_t
ngx_http_google_response_header_location(ngx_http_request_t    * r,
                                         ngx_http_google_ctx_t * ctx,
                                         ngx_str_t             * v)
{
  ngx_http_google_loc_conf_t * glcf;
  glcf = ngx_http_get_module_loc_conf(r, ngx_http_google_filter_module);
  
  u_char *  last = v->data + v->len;
  ngx_uint_t add = 0;
  
  if        (!ngx_strncasecmp(v->data, (u_char *)"http://", 7)) {
    add = 7;
  } else if (!ngx_strncasecmp(v->data, (u_char *)"https://", 8)) {
    add = 8;
  } else {
    return NGX_OK;
  }
  
  ngx_str_t host;
  host.data = v->data + add;
  host.len  = last - host.data;
  
  ngx_str_t uri;
  uri.data = ngx_strlchr(host.data, last, '/');
  if (uri.data) {
    uri .len = last - uri.data;
    host.len = uri.data - host.data;
  } else {
    uri.len = 0;
  }
  
  if (!ngx_strlcasestrn(host.data, host.data + host.len,
                        (u_char *)"google", 6 - 1))
  {
    // none google domains
    // just return back
    return NGX_OK;
  }
  
  if (!ngx_strncasecmp(host.data, (u_char *)"ipv", 3)) {
    ngx_str_t nuri;
    nuri.len  = uri.len + 5;
    nuri.data = ngx_pcalloc(r->pool, nuri.len);
    if (!nuri.data) return NGX_ERROR;
    ngx_snprintf(nuri.data, nuri.len, "/ipv%c%V", host.data[3], &uri);
    uri = nuri;
  } else if (ctx->enable.scholar &&
             !ngx_strncasecmp(host.data, (u_char *)"scholar", 7))
  {
    if (uri.len &&
        ngx_strncasecmp(uri.data, (u_char *)"/scholar", 8))
    {
      ngx_str_t nuri;
      nuri.len  = uri.len + 8;
      nuri.data = ngx_pcalloc(r->pool, nuri.len);
      if (!nuri.data) return NGX_ERROR;
      ngx_snprintf(nuri.data, nuri.len, "/scholar%V", &uri);
      uri = nuri;
    }
  }
  
  ngx_str_t nv;
  nv.len  = 8 + ctx->host->len + uri.len;
  nv.data = ngx_pcalloc(r->pool, nv.len);
  
  if (!nv.data) return NGX_ERROR;
  
  ngx_snprintf(nv.data, nv.len, "%s%V%V",
               glcf->ssl ? "https://" : "http://",
               ctx->host, &uri);
  *v = nv;
  
  return NGX_OK;
}


static ngx_int_t
ngx_http_google_response_header_set_cookie_exempt(ngx_http_request_t    * r,
                                                  ngx_http_google_ctx_t * ctx,
                                                  ngx_array_t           * kvs)
{
  ngx_uid_t i;
  ngx_keyval_t * kv, * hd = kvs->elts;
  
  for (i = 0; i < kvs->nelts; i++) {
    kv = hd + i;
    
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"GOOGLE_ABUSE_EXEMPTION", 22)) {
      if (!kv->value.len) {
        kvs->nelts = 0;
        return NGX_OK;
      }
    }
    
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"expires", 7)) {
      ngx_str_set(&kv->value, "Fri, 01-Jan-2016 00:00:00 GMT");
    }
  }
  
  return NGX_OK;
}

static ngx_int_t
ngx_http_google_response_header_set_cookie_pref(ngx_http_request_t    * r,
                                                ngx_http_google_ctx_t * ctx,
                                                ngx_str_t             * v)
{
  if (ctx->type != ngx_http_google_type_main) return NGX_OK;
  
  ngx_uid_t i;
  ngx_array_t * kvs  = ngx_http_google_explode_kv(r, v, ":");
  if (!kvs) return NGX_ERROR;
  
  ngx_array_t * nkvs = ngx_array_create(r->pool, 4, sizeof(ngx_keyval_t));
  if (!nkvs) return NGX_ERROR;
  
  ngx_keyval_t * kv, * hd;

  hd = kvs->elts;
  for (i = 0; i < kvs->nelts; i++) {
    kv = hd + i;
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"LD", 2)) continue;
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"CR", 2)) continue;
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"NW", 2)) continue;
    kv = ngx_array_push(nkvs);
    if (!kv) return NGX_ERROR;
    *kv = hd[i];
  }

  kv = ngx_array_push(nkvs);
  if (!kv) return NGX_ERROR;
  
  ngx_str_set(&kv->key,   "LD");
  ngx_str_set(&kv->value, "zh-CN");
  
  kv = ngx_array_push(nkvs);
  if (!kv) return NGX_ERROR;
  
  ngx_str_set(&kv->key,   "NW");
  ngx_str_set(&kv->value, "1");
  
  kv = ngx_array_push(nkvs);
  if (!kv) return NGX_ERROR;
  
  ngx_str_set(&kv->key,   "CR");
  ngx_str_set(&kv->value, "2");

  ngx_str_t * nv = ngx_http_google_implode_kv(r, nkvs, ":");
  if (!nv) return NGX_ERROR;
  
  *v = *nv;
  
  return NGX_OK;
}

static ngx_int_t
ngx_http_google_response_header_set_cookie(ngx_http_request_t    * r,
                                           ngx_http_google_ctx_t * ctx,
                                           ngx_table_elt_t       * tb)
{
  ngx_array_t * kvs = ngx_http_google_explode_kv(r, &tb->value, ";");
  if (!kvs) return NGX_ERROR;
  
  ngx_uint_t i;
  ngx_keyval_t * kv, * hd = kvs->elts;
  
  for (i = 0; i < kvs->nelts; i++)
  {
    kv = hd + i;
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"PREF", 4))
    {
      if (ngx_http_google_response_header_set_cookie_pref(r, ctx, &kv->value)) {
        return NGX_ERROR;
      }
    }
    
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"GOOGLE_ABUSE_EXEMPTION", 22))
    {
      if (ngx_http_google_response_header_set_cookie_exempt(r, ctx, kvs)) {
        return NGX_ERROR;
      }
    }
    
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"domain", 6)) {
      kv->value.len  = 1 + ctx->host->len;
      kv->value.data = ngx_pcalloc(r->pool, kv->value.len);
      ngx_snprintf(kv->value.data, kv->value.len, ".%V", ctx->host);
    }
    
    if (!ngx_strncasecmp(kv->key.data, (u_char *)"path", 4)) {
      ngx_str_set(&kv->value, "/");
    }
  }
  
  // unset this key
  if (!kvs->nelts) {
    tb->key.len = 0; return NGX_OK;
  }
  
  ngx_str_t * set_cookie = ngx_http_google_implode_kv(r, kvs, "; ");
  if (!set_cookie) return NGX_ERROR;
  
  // reset set cookie
  tb->value = *set_cookie;
  
  return NGX_OK;
}

ngx_int_t
ngx_http_google_response_header_filter(ngx_http_request_t * r)
{
  ngx_http_google_main_conf_t * gmcf;
  gmcf = ngx_http_get_module_main_conf(r, ngx_http_google_filter_module);
  
  ngx_http_google_loc_conf_t * glcf;
  glcf = ngx_http_get_module_loc_conf(r, ngx_http_google_filter_module);
  if (glcf->enable != 1) return gmcf->next_header_filter(r);
  
  ngx_http_google_ctx_t * ctx;
  ctx = ngx_http_get_module_ctx(r, ngx_http_google_filter_module);
  
  ngx_list_t * hds = ngx_list_create(r->pool, 4, sizeof(ngx_table_elt_t));
  if (!hds) return NGX_ERROR;
  
  ngx_uint_t i;
  ngx_list_part_t * pt = &r->headers_out.headers.part;
  ngx_table_elt_t * hd = pt->elts, * tb;
  
  for (i = 0; /* void */; i++)
  {
    if (i >= pt->nelts) {
      
      if (pt->next == NULL) break;
      
      pt = pt->next;
      hd = pt->elts;
      i  = 0;
    }
    
    tb = hd + i;
    
    if (!ngx_strncasecmp(tb->key.data, (u_char *)"Location", 8)) {
      if (ngx_http_google_response_header_location(r, ctx, &tb->value)) {
        return NGX_ERROR;
      }
    }
    
    if (!ngx_strncasecmp(tb->key.data, (u_char *)"Set-Cookie", 10)) {
      if (ngx_http_google_response_header_set_cookie(r, ctx, tb)) {
        return NGX_ERROR;
      }
    }
    
    if (!tb->key.len) continue;
    tb = ngx_list_push(hds);
    if (!tb) return NGX_ERROR;
    
    *tb = hd[i];
  }
  
  tb = ngx_list_push(hds);
  if (!tb) return NGX_ERROR;
  
  // add server header
  ngx_str_set(&tb->key, "Server");
  tb->value = *ctx->host;
  tb->hash  = ngx_hash_key_lc(tb->key.data, tb->key.len);
  
  // replace with new headers
  r->headers_out.server  = tb;
  r->headers_out.headers = *hds;
  
  return gmcf->next_header_filter(r);
}
