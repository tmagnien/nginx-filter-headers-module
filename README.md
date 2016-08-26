Name
====

**ngx_filter_headers** - Filters output headers based on whitelist

Table of Contents
=================

* [Name] (#name)
* [Synopsis] (#synopsis)
* [Description] (#description)
* [Directives] (#directives)
    * [filter_headers_output_whitelist] (#filter_headers_output_whitelist)
* [Installation] (#installation)
* [TODO] (#todo)
* [Author] (#author)

Synopsis
========

```nginx
 # clear all headers not present in given list before sending response to client
 location /somewhere {
     filter_headers_output_whitelist vary content-length last-modified connection accept-ranges content-type content-encoding etag cache-control expires keep-alive accept-ranges server;
     proxy_pass http://origin_server;
 }
```

Description
===========

This module allows you to give the list of headers that will be sent to client. Any header not in this list will be cleared before response is sent.

Directives
==========

filter_headers_output_whitelist
-------------------------------
**syntax:** *filter_headers_output_whitelist &lt;header&gt; ... &lt;header&gt;*
**default:** *no*
**context:** *http, server, location, location if*
**phase:** *output-header-filter*

Clears any header not in given list from response to client.

Installation
============

Grab the nginx source code from [nginx.org](http://nginx.org/) and then build the source with this module:

```bash
 wget 'http://nginx.org/download/nginx-1.10.1.tar.gz'
 tar -xzvf nginx-1.10.1.tar.gz
 cd nginx-1.10.1/
 
 ./configure --add-module=/path/to/nginx-filter-headers-module

 make
 make install
```

TODO
====

* Support clearing headers from origin server response before the object gets cached

Author
======

* Thierry Magnien *&lt;thierry.magnien@gmail.com&gt;*
