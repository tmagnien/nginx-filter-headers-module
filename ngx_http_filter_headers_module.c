/* Header filtering module for G-Core Labs */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_event.h>

/*
 * ngx_http_filter_headers_main_conf_t
 */
typedef struct {
	ngx_int_t	requires_filter;
	/*ngx_int_t	postponed_to_phase_end;
	ngx_int_t	requires_handler;*/
} ngx_http_filter_headers_main_conf_t;

/*
 * ngx_http_filter_headers_loc_conf_t
 */
typedef struct {
	/*ngx_array_t	*input_headers;*/
	ngx_hash_t	*output_headers_hash;
} ngx_http_filter_headers_loc_conf_t;

static void* ngx_http_filter_headers_create_main_conf(ngx_conf_t *cf);
static void* ngx_http_filter_headers_create_loc_conf(ngx_conf_t *cf);
static ngx_int_t ngx_http_filter_headers_filter(ngx_http_request_t *r);
/*static ngx_int_t ngx_http_filter_headers_handler(ngx_http_request_t *r);*/

static ngx_int_t ngx_http_filter_headers_post_config(ngx_conf_t *cf);

static char* ngx_http_filter_headers_config_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

/*
 * Module commands
 */
static ngx_command_t ngx_http_filter_headers_commands[] = {
	/*{ ngx_string("filter_headers_input_whitelist"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_1MORE,
	  ngx_http_filter_headers_config_headers,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_filter_headers_loc_conf_t, input_headers),
	  NULL },*/
	{ ngx_string("filter_headers_output_whitelist"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_1MORE,
	  ngx_http_filter_headers_config_headers,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_filter_headers_loc_conf_t, output_headers_hash),
	  NULL },
	ngx_null_command
};

/*
 * Module context
 */
static ngx_http_module_t ngx_http_filter_headers_module_ctx = {
	NULL,						/* preconfiguration */
	ngx_http_filter_headers_post_config,		/* postconfiguration */

	ngx_http_filter_headers_create_main_conf,	/* create main configuration */
	NULL,						/* init main configuration */

	NULL,						/* create server configuration */
	NULL,						/* merge server configuration */

	ngx_http_filter_headers_create_loc_conf,	/* create location configuration */
	NULL						/* merge location configuration */
};

/*
 * Module definition
 * ngx_http_filter_headers_module
 */
ngx_module_t ngx_http_filter_headers_module = {
	NGX_MODULE_V1,
	&ngx_http_filter_headers_module_ctx,	/* module context */
	ngx_http_filter_headers_commands,	/* module directives */
	NGX_HTTP_MODULE,			/* module type */
	NULL,					/* init master */
	NULL,					/* init module */
	NULL,					/* init process */
	NULL,					/* init thread */
	NULL,					/* exit thread */
	NULL,					/* exit process */
	NULL,					/* exit master */
	NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static volatile ngx_cycle_t *ngx_http_filter_headers_prev_cycle = NULL;

/*
 * Post configuration
 */
static ngx_int_t
ngx_http_filter_headers_post_config(ngx_conf_t *cf)
{
	int					multi_http_blocks;
	ngx_http_filter_headers_main_conf_t	*fhmcf;
	/*ngx_http_handler_pt			*h;
	ngx_http_core_main_conf_t		*cmcf;*/

	fhmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_filter_headers_module);

	if (ngx_http_filter_headers_prev_cycle != ngx_cycle) {
		ngx_http_filter_headers_prev_cycle = ngx_cycle;
		multi_http_blocks = 0;
	} else {
		multi_http_blocks = 1;
	}

	if (multi_http_blocks || fhmcf->requires_filter) {
		ngx_http_next_header_filter = ngx_http_top_header_filter;
		ngx_http_top_header_filter = ngx_http_filter_headers_filter;
	}

	/*if (fhmcf->requires_handler) {
		cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
		h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
		if (h == NULL) {
			return NGX_ERROR;
		}
		*h = ngx_http_filter_headers_handler;
	}*/

	return NGX_OK;
}

/*
 * Configuration creation and initialization
 */
static void *
ngx_http_filter_headers_create_main_conf(ngx_conf_t *cf)
{
	ngx_http_filter_headers_main_conf_t  *fhmcf;

	fhmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_filter_headers_main_conf_t));
	if (fhmcf == NULL) {
		return NGX_CONF_ERROR;
	}

	/* No need to set everything to zero, ngx_pcalloc does it for us */

	return fhmcf;
}

static void *
ngx_http_filter_headers_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_filter_headers_loc_conf_t  *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_filter_headers_loc_conf_t));
	if (conf == NULL) {
		return NGX_CONF_ERROR;
	}

	/* No need to set everything to NULL, ngx_pcalloc does it for us */

	return conf;
}

static char *
ngx_http_filter_headers_config_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_filter_headers_main_conf_t	*fhmcf;
	ngx_uint_t				i;
	ngx_hash_t				**headers_hash;
	ngx_array_t				*headers;
	ngx_str_t				*arg;
	ngx_hash_init_t				hash;
	ngx_hash_key_t				*hk;
	ngx_int_t				rc;

	ngx_http_filter_headers_loc_conf_t	*fhlcf = conf;

	arg = cf->args->elts;

	if (ngx_strcmp(arg[0].data, "filter_headers_output_whitelist") == 0) {
		headers_hash = &fhlcf->output_headers_hash;
	}
	/*else if (ngx_strcmp(arg[0].data, "filter_headers_input_whitelist") == 0) {
		headers = &fhlcf->input_headers;
	}*/
	else {
		/* Why the hell did we get here ??? */
		ngx_log_error(NGX_LOG_ERR, cf->log, 0, "Got unknown directive %V in ngx_http_filter_headers_config_headers", &arg[0]);
		return NGX_CONF_ERROR;

	}

	if (*headers_hash == NULL) {
		*headers_hash = ngx_pcalloc(cf->pool, sizeof(ngx_hash_t));
		if (*headers_hash == NULL) {
			return NGX_CONF_ERROR;
		}
	}

	headers = ngx_array_create(cf->pool, 1, sizeof(ngx_hash_key_t));
	if (headers == NULL) {
		return NGX_CONF_ERROR;
	}

	for (i = 1; i < cf->args->nelts; i++) {
		if (arg[i].len == 0) {
			continue;
		}
		hk = ngx_array_push(headers);
		if (hk == NULL) {
			return NGX_CONF_ERROR;
		}
		hk->key.data = ngx_pcalloc(cf->pool, arg[i].len);
		hk->key.len = arg[i].len;
		ngx_memcpy(hk->key.data, arg[i].data, arg[i].len);
		hk->key_hash = ngx_hash_key_lc(arg[i].data, arg[i].len);
		hk->value = (void *) 1;
	}

	if (headers->nelts == 0) {
		ngx_pfree(cf->pool, headers);
		ngx_pfree(cf->pool, headers_hash);
		headers = NULL;
		headers_hash = NULL;
	}

	/*else if (fhlcf->input_headers && fhlcf->input_headers->nelts > 0) {
		fhmcf->requires_handler = 1;
	}*/

	hash.max_size = 512;
	hash.bucket_size = 64;
	hash.name = "filter_headers_hash";
	hash.hash = *headers_hash;
	hash.key = ngx_hash_key_lc;
	hash.pool = cf->pool;
	hash.temp_pool = NULL;

	rc = ngx_hash_init(&hash, headers->elts, headers->nelts);

	if (rc != NGX_OK) {
		return NGX_CONF_ERROR;
	}

	fhmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_filter_headers_module);
	if (fhlcf->output_headers_hash && (fhlcf->output_headers_hash->size > 0)) {
		fhmcf->requires_filter = 1;
	}

	return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_filter_headers_filter(ngx_http_request_t *r)
{
	ngx_http_filter_headers_loc_conf_t	*conf;
	ngx_uint_t				i;
	ngx_list_part_t				*part;
	ngx_table_elt_t				*header;

	conf = ngx_http_get_module_loc_conf(r, ngx_http_filter_headers_module);

	part = &r->headers_out.headers.part;
	header = part->elts;

	for (i = 0; /* void */; i++) {
		if (i >= part->nelts) {
			if (part->next == NULL) {
				break;
			}
			part = part->next;
			header = part->elts;
			i = 0;
		}
		if (header[i].hash == 0) {
			continue;
		}
		/* Check if header is in accepted output headers */
		if (conf->output_headers_hash && (conf->output_headers_hash->size > 0)) {
			if (!ngx_hash_find(conf->output_headers_hash, header[i].hash, header[i].lowcase_key, header[i].key.len)) {
				ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Clearing output header %V", &header[i].key);
				header[i].hash = 0;
				header[i].value.len = 0;
			}
		}
	}

	return ngx_http_next_header_filter(r);
}

/*static ngx_int_t
ngx_http_filter_headers_handler(ngx_http_request_t *r)
{
	return NGX_DECLINED;
}*/
