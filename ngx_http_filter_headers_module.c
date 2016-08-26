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
	ngx_array_t	*output_headers;
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
	  offsetof(ngx_http_filter_headers_loc_conf_t, output_headers),
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
	ngx_array_t				**headers;
	ngx_str_t				*arg, *header_name;

	ngx_http_filter_headers_loc_conf_t	*fhlcf = conf;

	arg = cf->args->elts;

	if (ngx_strcmp(arg[0].data, "filter_headers_output_whitelist") == 0) {
		headers = &fhlcf->output_headers;
	}
	/*else if (ngx_strcmp(arg[0].data, "filter_headers_input_whitelist") == 0) {
		headers = &fhlcf->input_headers;
	}*/
	else {
		/* Why the hell did we get here ??? */
		ngx_log_error(NGX_LOG_ERR, cf->log, 0, "Got unknown directive %V in ngx_http_filter_headers_config_headers", &arg[0]);
		return NGX_CONF_ERROR;

	}

	if (*headers == NULL) {
		*headers = ngx_array_create(cf->pool, 1, sizeof(ngx_str_t));
		if (*headers == NULL) {
			return NGX_CONF_ERROR;
		}
	}

	for (i = 1; i < cf->args->nelts; i++) {
		if (arg[i].len == 0) {
			continue;
		}
		header_name = ngx_array_push(*headers);
		header_name->len = arg[i].len;
		header_name->data = ngx_pcalloc(cf->pool, header_name->len);
		ngx_memcpy(header_name->data, arg[i].data, arg[i].len);
	}

	if ((*headers)->nelts == 0) {
		ngx_pfree(cf->pool, *headers);
		*headers = NULL;
	}

	fhmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_filter_headers_module);
	if (fhlcf->output_headers && fhlcf->output_headers->nelts > 0) {
		fhmcf->requires_filter = 1;
	}
	/*else if (fhlcf->input_headers && fhlcf->input_headers->nelts > 0) {
		fhmcf->requires_handler = 1;
	}*/

	return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_filter_headers_filter(ngx_http_request_t *r)
{
	ngx_http_filter_headers_loc_conf_t	*conf;
	ngx_str_t				*header_name;
	ngx_uint_t				i, j, found;
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
		if (conf->output_headers) {
			header_name = conf->output_headers->elts;
			found = 0;
			for (j = 0; j < conf->output_headers->nelts; j++) {
				if (header[i].key.len == header_name[j].len && ngx_strncasecmp(header[i].key.data, header_name[j].data, header[i].key.len) == 0) {
					/* Found header */
					found = 1;
					break;
				}
			}
			if (!found) {
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
