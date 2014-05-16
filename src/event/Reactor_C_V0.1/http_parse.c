
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "http_parse.h"
#include "Define_Macro.h"
#include <string.h>
#include "http_buffer.h"

static void do_method(const char *start, const char *end)
{
  char buff[BUFSIZ] = {0};
  strncpy(buff, start, end - start);

  printf("method = %s\n", buff);
}

static void do_path(const char *start, const char *end)
{
  char buff[BUFSIZ] = {0};
  strncpy(buff, start, end - start);

  printf("path = %s\n", buff);
}

static void do_version(const char *start, const char *end)
{
  char buff[BUFSIZ] = {0};
  strncpy(buff, start, end - start);

  printf("version = %s\n", buff);
}

static void do_kv(const char *key_start, const char *key_end, const char *value_start, const char *value_end)
{
  char buff[BUFSIZ] = {0};
  strncpy(buff, key_start, key_end - key_start);
  strncat(buff, ": ", strlen(": "));

  strncat(buff, value_start, value_end - value_start);
  
  printf("K/V -- >%s\n", buff);
}

int parse_http_request_line(http_request_t *r)
{
    char  ch;
    char  *p;
		char c;
		int count = 0;
    enum {
        sw_start = 0,
        sw_method,
        sw_spaces_before_uri,
				sw_uri,
				sw_spaces_after_uri,
				sw_spaces_before_version,
				sw_version,
				sw_version_H,
				sw_version_HT,
				sw_version_HTT,
				sw_version_HTTP,
				sw_version_first_http_major,
				sw_version_http_major,
				sw_version_first_http_minor,
				sw_version_http_minor,
				sw_request_line_parse_almost_done,
				sw_request_line_parse_done,
				sw_key,
				sw_key_end,
				sw_separator,
				sw_value,
				sw_kv_almost_done,
				sw_kv_done,
#if 0
				sw_request_header_almost_done,
				sw_request_header_done
#endif
				sw_almost_done,
				sw_done
    } state;

    state = (r->parse_state == -1)? sw_start: r->parse_state;

		for (p = r->c->buffer->pos; p != r->c->buffer->last; p++) {
			ch = *p;

			switch (state) {

				case sw_start:
					r->method_start = p;

					if (ch == '\r' || ch == '\n') {
						state = sw_request_line_parse_done;
						break;
					}

					if (ch < 'A' || ch > 'Z') {
						return -1;
					}

					state = sw_method;
					break;

				case sw_method:
					if (ch == ' ') {
						r->method_end = p;
						do_method(r->method_start, r->method_end);
						state = sw_spaces_before_uri;
						break;
					}

					if (ch < 'A' || ch > 'Z') {
						return -1;
					}

					break;

				case sw_spaces_before_uri:

					if (ch == '/') {
						r->path_start = p;
						state = sw_uri;
						break;
					}

				case sw_uri:
					if (ch == ' ') {
						state = sw_spaces_before_version;
						r->path_end = p;
						do_path(r->path_start, r->path_end);
					} 
					
					if (count > 1024) {
						printf("uri to large!\n");
						return -1;
					}

					count++;

					break;

				case sw_spaces_before_version:
					if (ch == 'H') {
						state = sw_version_H;
						r->version_start = p;
						break;
					}
				
				case sw_version_H:
					if (ch == 'T') {
						state = sw_version_HT;
						break;
					} else {
						fprintf(stderr ,"version parse error!\n");
						return -1;
					}
				case sw_version_HT:
					if (ch == 'T') {
						state = sw_version_HTT;
						break;
					}else {
						fprintf(stderr, "version parse error!\n");
						return -1;
					}
				case sw_version_HTT:
					if (ch == 'P') {
						state = sw_version_HTTP;
						break;
					} else {
						fprintf(stderr, "version parse error!\n");
						return -1;
					}
				case sw_version_HTTP:
					if (ch == '/') {
						state = sw_version_first_http_major;	
					}else {
						fprintf(stderr, "version parse error!\n");
					}
					break;
				case sw_version_first_http_major:
					if (ch < '1' || ch > '9') {
						fprintf(stderr, "version parse error!\n");
						return -1;
					}
					r->http_major = ch - '0';
					state = sw_version_http_major;
					break;

				case sw_version_http_major:
					if (ch == '.') {
						state = sw_version_first_http_minor;
						break;
					}
					
					if (!isdigit(ch)) {
						fprintf(stderr, "Invalid version!\n");
						return -1;
					}

					r->http_major *= 10;
					r->http_major += ch - '0';	
					break;
				case sw_version_first_http_minor:
					if (ch < '0' || ch > '9') {
						fprintf(stderr, "Invalid version!\n");
						return -1;
					}

					r->http_minor = ch - '0';
					state = sw_version_http_minor;
					break;

				case sw_version_http_minor:
					if (ch == '\r') {
						state = sw_request_line_parse_almost_done;
						r->version_end = p;
						do_version(r->version_start, r->version_end);
						break;
					}	

					if (ch < '0' || ch > '9') {
						fprintf(stderr, "Invalid version!\n");
						return -1;
					}

					r->http_minor = r->http_minor * 10 + ch - '0';
					break;
				case sw_request_line_parse_almost_done:
					if (ch == '\n') {
						state = sw_request_line_parse_done;
					}
					break;

				case sw_request_line_parse_done:
					if (ch == '\r') {
						r->tmp = p;
						state = sw_almost_done;
					} else {
						state = sw_key;
						r->key_start = p;
					}
					break;

				case sw_key:
					if (ch == ':') {
						state = sw_separator;
						r->key_end = p;
						break;
					}
					
					c = ch | 0x20;
					
					if (c >= 'a' && ch <= 'z') {
						state = sw_key;
					}

					break;
	
				case sw_separator:
					if (ch == ' ' || ch == '\t') {
						state = sw_separator;
						break;
					}

					state = sw_value;
					r->value_start = p;
					break;
				case sw_value:

					if (ch == '\r') {
						r->value_end = p;
						r->tmp = p;
						do_kv(r->key_start, r->key_end, r->value_start, r->value_end);
						state = sw_kv_almost_done;
						break;
					}
					state = sw_value;
					break;
				case sw_kv_almost_done:
					if (ch == '\n' && *r->tmp == '\r') {
						state = sw_kv_done;
					}
					break;

				case sw_kv_done:
					if (ch != '\r' && ch != '\n') {
						state = sw_key;
						r->key_start = p;
						p--;
						r->key_end = NULL;
						r->value_start = NULL;
						r->value_end = NULL;
						break;
					}else if (ch == '\r'){
						state = sw_almost_done; 
						r->tmp = p;
					}
					break;
#if 0
				case sw_request_header_almost_done:
					break;
				case sw_request_header_done:	
					break;
#endif
				case sw_almost_done:
					if (ch == '\n' && *r->tmp == '\r') {
						state = sw_done;
					} else {
						return -1;
					}
					break;
				case sw_done:
					break;

				default:
					break;

			}
			
		}

		r->parse_state = state;
		r->c->buffer->pos = p;
		
		if (r->parse_state != sw_done) {
			return EAGAIN;	
		}
		
		return 0;
}

