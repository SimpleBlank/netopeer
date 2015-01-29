/**
 * \file netconf-server-transapi.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * @brief NETCONF device module to configure netconf server following
 * ietf-netconf-server data model
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

/*
 * This is automatically generated callbacks file
 * It contains 3 parts: Configuration callbacks, RPC callbacks and state data callbacks.
 * Do NOT alter function signatures or any structures unless you know exactly what you are doing.
 */

#ifdef __GNUC__
#	define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#	define UNUSED(x) UNUSED_ ## x
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

#include <libxml/tree.h>
#include <libnetconf_xml.h>
#include <libnetconf_ssh.h>

#include "netconf_server_transapi.h"
#include "server_tls.h"
#include "cfgnetopeer_transapi.h"

extern struct np_options netopeer_options;

#ifndef DISABLE_CALLHOME

static struct ch_app* callhome_apps = NULL;

#endif

static void free_all_bind_addr(struct np_bind_addr** list) {
	struct np_bind_addr* prev;

	if (list == NULL) {
		return;
	}

	while (*list != NULL) {
		prev = *list;
		*list = (*list)->next;
		free(prev->addr);
		free(prev);
	}
}

static struct np_bind_addr* find_bind_addr(struct np_bind_addr* root, int ssh, const char* addr, unsigned int port) {
	struct np_bind_addr* cur = NULL;

	if (root == NULL || addr == NULL) {
		return NULL;
	}

	for (cur = root; cur != NULL; cur = cur->next) {
		if (cur->ssh == ssh && strcmp(cur->addr, addr) == 0 && cur->port == port) {
			break;
		}
	}

	return cur;
}

static void add_bind_addr(struct np_bind_addr** root, int ssh, const char* addr, unsigned int port) {
	struct np_bind_addr* cur;
	unsigned int i;

	if (root == NULL) {
		return;
	}

	if (*root == NULL) {
		*root = malloc(sizeof(struct np_bind_addr));
		(*root)->ssh = ssh;
		(*root)->addr = strdup(addr);
		(*root)->port = port;
		(*root)->next = NULL;
		return;
	}

	for (cur = *root; cur->next != NULL; cur = cur->next);

	cur->next = malloc(sizeof(struct np_bind_addr));
	cur->next->ssh = ssh;
	cur->next->addr = strdup(addr);
	cur->next->port = port;
	cur->next->next = NULL;
}

static void del_bind_addr(struct np_bind_addr** root, int ssh, const char* addr, unsigned int port) {
	struct np_bind_addr* cur, *prev = NULL;

	if (root == NULL || addr == NULL) {
		return;
	}

	for (cur = *root; cur != NULL; cur = cur->next) {
		if (cur->ssh == ssh && strcmp(cur->addr, addr) == 0 && cur->port == port) {
			if (prev == NULL) {
				/* we're deleting the root */
				*root = cur->next;
				free(cur->addr);
				free(cur);
			} else {
				/* standard list member deletion */
				prev->next = cur->next;
				free(cur->addr);
				free(cur);
			}
			return;
		}
		prev = cur;
	}
}

/* Signal to libnetconf that configuration data were modified by any callback.
 * 0 - data not modified
 * 1 - data have been modified
 */
int server_config_modified = 0;

/*
 * Determines the callbacks order.
 * Set this variable before compilation and DO NOT modify it in runtime.
 * TRANSAPI_CLBCKS_LEAF_TO_ROOT (default)
 * TRANSAPI_CLBCKS_ROOT_TO_LEAF
 */
const TRANSAPI_CLBCKS_ORDER_TYPE callbacks_order = TRANSAPI_CLBCKS_ORDER_DEFAULT;

/* Do not modify or set! This variable is set by libnetconf to announce edit-config's error-option
Feel free to use it to distinguish module behavior for different error-option values.
 * Possible values:
 * NC_EDIT_ERROPT_STOP - Following callback after failure are not executed, all successful callbacks executed till
                         failure point must be applied to the device.
 * NC_EDIT_ERROPT_CONT - Failed callbacks are skipped, but all callbacks needed to apply configuration changes are executed
 * NC_EDIT_ERROPT_ROLLBACK - After failure, following callbacks are not executed, but previous successful callbacks are
                         executed again with previous configuration data to roll it back.
 */
NC_EDIT_ERROPT_TYPE server_erropt = NC_EDIT_ERROPT_NOTSET;

static char* get_node_content(xmlNodePtr node) {
	if (node == NULL || node->children == NULL) {
		return NULL;
	}
	return (char*)node->children->content;
}

xmlDocPtr server_get_state_data(xmlDocPtr UNUSED(model), xmlDocPtr UNUSED(running), struct nc_err **UNUSED(err)) {
	/* model doesn't contain any status data */
	return(NULL);
}
/*
 * Mapping prefixes with namespaces.
 * Do NOT modify this structure!
 */
struct ns_pair server_namespace_mapping[] = {{"srv", "urn:ietf:params:xml:ns:yang:ietf-netconf-server"}, {NULL, NULL}};

int callback_srv_netconf_srv_listen_srv_port(XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err** error, int ssh) {
	struct np_bind_addr* bind;
	unsigned int port, new_port;
	char* content;

	if (op & (XMLDIFF_REM | XMLDIFF_MOD)) {
		content = get_node_content(old_node);
		if (content == NULL) {
			nc_verb_error("%s: internal error at %s:%s", __func__, __FILE__, __LINE__);
			*error = nc_err_new(NC_ERR_OP_FAILED);
			nc_err_set(*error, NC_ERR_PARAM_MSG, "Check server logs.");
			return EXIT_FAILURE;
		}
		port = atoi(content);
	}

	if (op & (XMLDIFF_MOD | XMLDIFF_ADD)) {
		content = get_node_content(new_node);
		if (content == NULL) {
			nc_verb_error("%s: internal error at %s:%s", __func__, __FILE__, __LINE__);
			*error = nc_err_new(NC_ERR_OP_FAILED);
			nc_err_set(*error, NC_ERR_PARAM_MSG, "Check server logs.");
			return EXIT_FAILURE;
		}
		new_port = atoi(content);
	}

	/* BINDS LOCK */
	pthread_mutex_lock(&netopeer_options.binds_lock);

	if (op & (XMLDIFF_REM | XMLDIFF_MOD)) {
		del_bind_addr(&netopeer_options.binds, ssh, "::0", port);
		netopeer_options.binds_change_flag = 1;

		nc_verb_verbose("%s: " (ssh ? "SSH" : "TLS") " stopped listening on the port %d", __func__, port);
	}
	if (op & (XMLDIFF_MOD | XMLDIFF_ADD)) {
		add_bind_addr(&netopeer_options.binds, ssh, "::0", new_port);
		netopeer_options.binds_change_flag = 1;

		nc_verb_verbose("%s: " (ssh ? "SSH" : "TLS") " listening on the port %d", __func__, new_port);
	}

	/* BINDS UNLOCK */
	pthread_mutex_unlock(&netopeer_options.binds_lock);

	return EXIT_SUCCESS;
}

int callback_srv_netconf_srv_listen_srv_interface(XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err** error, int ssh) {
	xmlNodePtr cur;
	struct np_bind_addr* bind;
	char* addr = NULL, *new_addr = NULL, *content;
	unsigned int port = 0, new_port = 0;

	if (op & (XMLDIFF_REM | XMLDIFF_MOD)) {
		for (cur = old_node->children; cur != NULL; cur = cur->next) {
			if (cur->type != XML_ELEMENT_NODE) {
				continue;
			}

			if (xmlStrEqual(cur->name, BAD_CAST "address")) {
				addr = get_node_content(cur);
			}
			if (xmlStrEqual(cur->name, BAD_CAST "port")) {
				content = get_nodes_content(cur);
				if (content != NULL) {
					port = atoi(content);
				}
			}
		}

		if (addr == NULL || port == 0) {
			nc_verb_error("%s: missing either address or port at %s:%s", __func__, __FILE__, __LINE__);
			*error = nc_err_new(NC_ERR_OP_FAILED);
			nc_err_set(*error, NC_ERR_PARAM_MSG, "Check server logs.");
			return EXIT_FAILURE;
		}
	}

	if (op & (XMLDIFF_MOD | XMLDIFF_ADD)) {
		for (cur = new_node->children; cur != NULL; cur = cur->next) {
			if (cur->type != XML_ELEMENT_NODE) {
				continue;
			}

			if (xmlStrEqual(cur->name, BAD_CAST "address")) {
				new_addr = get_node_content(cur);
			}
			if (xmlStrEqual(cur->name, BAD_CAST "port")) {
				content = get_nodes_content(cur);
				if (content != NULL) {
					new_port = atoi(content);
				}
			}
		}

		if (new_addr == NULL || new_port == 0) {
			nc_verb_error("%s: missing either address or port at %s:%s", __func__, __FILE__, __LINE__);
			*error = nc_err_new(NC_ERR_OP_FAILED);
			nc_err_set(*error, NC_ERR_PARAM_MSG, "Check server logs.");
			return EXIT_FAILURE;
		}
	}

	/* BINDS LOCK */
	pthread_mutex_lock(&netopeer_options.binds_lock);

	if (op & (XMLDIFF_REM | XMLDIFF_MOD)) {
		del_bind_addr(&netopeer_options.binds, ssh, addr, port);
		netopeer_options.binds_change_flag = 1;

		nc_verb_verbose("%s: " (ssh ? "SSH" : "TLS") " stopped listening on the address %s:%d", __func__, addr, port);
	}
	if (op & (XMLDIFF_MOD | XMLDIFF_ADD)) {
		add_bind_addr(&netopeer_options.binds, ssh, new_addr, new_port);
		netopeer_options.binds_change_flag = 1;

		nc_verb_verbose("%s: " (ssh ? "SSH" : "TLS") " listening on the address %s:%d", __func__, new_addr, new_port);
	}

	/* BINDS UNLOCK */
	pthread_mutex_unlock(&netopeer_options.binds_lock);

	return EXIT_SUCCESS;
}

#ifndef DISABLE_CALLHOME

static xmlNodePtr find_node(xmlNodePtr parent, xmlChar* name) {
	xmlNodePtr child;

	for (child = parent->children; child != NULL; child= child->next) {
		if (child->type != XML_ELEMENT_NODE) {
			continue;
		}
		if (xmlStrcmp(name, child->name) == 0) {
			return (child);
		}
	}

	return (NULL);
}

static struct client_struct* sock_connect(const char* address, uint16_t port) {
	struct client_struct* ret;
	int is_ipv4;

	struct sockaddr_in* saddr4;
	struct sockaddr_in6* saddr6;

	ret = calloc(1, sizeof(struct client_struct));
	ret->sock = -1;

	if (strchr(address, ':') != NULL) {
		is_ipv4 = 0;
	} else {
		is_ipv4 = 1;
	}

	if ((ret->sock = socket((is_ipv4 ? AF_INET : AF_INET6), SOCK_STREAM, IPPROTO_TCP)) == -1) {
		nc_verb_error("%s: creating socket failed (%s)", __func__, strerror(errno));
		goto fail;
	}

	if (is_ipv4) {
		saddr4 = (struct sockaddr_in*)&ret->saddr;

		saddr4->sin_family = AF_INET;
		saddr4->sin_port = htons(port);

		if (inet_pton(AF_INET, address, &saddr4->sin_addr) != 1) {
			nc_verb_error("%s: failed to convert IPv4 address \"%s\"", __func__, address);
			goto fail;
		}

		if (connect(ret->sock, (struct sockaddr*)saddr4, sizeof(struct sockaddr_in)) == -1) {
			nc_verb_error("Call Home: could not connect to %s:%u (%s)", address, port, strerror(errno));
			goto fail;
		}

	} else {
		saddr6 = (struct sockaddr_in6*)&ret->saddr;

		saddr6->sin6_family = AF_INET6;
		saddr6->sin6_port = htons(port);

		if (inet_pton(AF_INET6, address, &saddr6->sin6_addr) != 1) {
			nc_verb_error("%s: failed to convert IPv6 address \"%s\"", __func__, address);
			goto fail;
		}

		if (connect(ret->sock, (struct sockaddr*)saddr6, sizeof(struct sockaddr_in6)) == -1) {
			nc_verb_error("Call Home: could not connect %s:%u (%s)",address, port, strerror(errno));
			goto fail;
		}
	}

	nc_verb_verbose("Call Home: connected to %s:%u", address, port);
	return ret;

fail:
	free(ret);
	return NULL;
}

pthread_mutex_t callhome_lock = PTHREAD_MUTEX_INITIALIZER;
volatile struct client_struct* callhome_client = NULL;

/* each app has its own thread */
__attribute__((noreturn))
static void* app_loop(void* app_v) {
	struct ch_app* app = (struct ch_app*)app_v;
	struct ch_server* cur_server = NULL;
	struct timeval cur_time;
	struct timespec ts;
	int i;

	/* TODO sigmask for the thread? */

	nc_verb_verbose("Starting Call Home thread (%s).", app->name);

	nc_session_transport(NC_TRANSPORT_SSH);

	for (;;) {
		pthread_testcancel();

		app->ch_st->freed = 0;

		/* get last connected server if any */
		for (cur_server = app->servers; cur_server != NULL && cur_server->active == 0; cur_server = cur_server->next);
		if (cur_server == NULL) {
			/*
			 * first-listed start-with's value is set in config or this is the
			 * first attempt to connect, so use the first listed server spec
			 */
			cur_server = app->servers;
		} else {
			/* remove active flag */
			cur_server->active = 0;
		}

		/* try to connect to a server indefinitely */
		for (;;) {
			for (i = 0; i < app->rec_count; ++i) {
				if ((app->client = sock_connect(cur_server->address, cur_server->port)) != NULL) {
					break;
				}
				sleep(app->rec_interval);
			}

			if (app->client != NULL) {
				break;
			}

			cur_server = cur_server->next;
			if (cur_server == NULL) {
				cur_server = app->servers;
			}
		}

		app->client->callhome_st = app->ch_st;

		/* publish the new client for the main application loop to create a new SSH session */
		while (1) {
			/* CALLHOME LOCK */
			pthread_mutex_lock(&callhome_lock);
			if (callhome_client == NULL) {
				callhome_client = app->client;
				/* CALLHOME UNLOCK */
				pthread_mutex_unlock(&callhome_lock);
				break;
			}
			/* CALLHOME UNLOCK */
			pthread_mutex_unlock(&callhome_lock);
		}
		cur_server->active = 1;

		if (app->connection) {
			/* periodic connection */
			pthread_mutex_lock(&app->ch_st->ch_lock);
			while (app->ch_st->freed == 0) {
				clock_gettime(CLOCK_REALTIME, &ts);
				ts.tv_sec += CALLHOME_PERIODIC_LINGER_CHECK;
				i = pthread_cond_timedwait(&app->ch_st->ch_cond, &app->ch_st->ch_lock, &ts);
				if (i == ETIMEDOUT) {
					if (app->client->tls == NULL) {
						/* very weird */
						app->client->to_free = 1;
					} else {
						gettimeofday(&cur_time, NULL);
						if (timeval_diff(cur_time, app->client->last_rpc_time) >= app->rep_linger) {

							/* no data flow for too long, disconnect the client, wait for the set timeout and reconnect */
							nc_verb_verbose("Call Home (app %s) did not communicate for too long, disconnecting.", app->name);
							app->client->callhome_st = NULL;
							app->client->to_free = 1;
							sleep(app->rep_timeout*60);
							break;
						}
					}
				}
			}
			pthread_mutex_unlock(&app->ch_st->ch_lock);
			if (app->ch_st->freed == 1) {
				nc_verb_verbose("Call Home (app %s) disconnected.", app->name);
			}

		} else {
			/* persistent connection */
			pthread_mutex_lock(&app->ch_st->ch_lock);
			while (app->ch_st->freed == 0) {
				pthread_cond_wait(&app->ch_st->ch_cond, &app->ch_st->ch_lock);
			}
			pthread_mutex_unlock(&app->ch_st->ch_lock);
			nc_verb_verbose("Call Home (app %s) disconnected.", app->name);
		}
	}
}

static int app_create(xmlNodePtr node, struct nc_err** error, int ssh) {
	struct ch_app* new;
	struct ch_server* srv, *del_srv;
	xmlNodePtr auxnode, servernode, childnode;
	xmlChar* auxstr;

	new = calloc(1, sizeof(struct ch_app));
	new->ssh = ssh;

	/* get name */
	auxnode = find_node(node, BAD_CAST "name");
	new->name = (char*)xmlNodeGetContent(auxnode);

	/* get servers list */
	auxnode = find_node(node, BAD_CAST "servers");
	for (servernode = auxnode->children; servernode != NULL; servernode = servernode->next) {
		if ((servernode->type != XML_ELEMENT_NODE) || (xmlStrcmp(servernode->name, BAD_CAST "server") != 0)) {
			continue;
		}

		/* alloc new server */
		if (new->servers == NULL) {
			new->servers = calloc(1, sizeof(struct ch_server));
			srv = new->servers;
		} else {
			/* srv is the last server */
			srv->next = calloc(1, sizeof(struct ch_server));
			srv->next->prev = srv;
			srv = srv->next;
		}

		for (childnode = servernode->children; childnode != NULL; childnode = childnode->next) {
			if (childnode->type != XML_ELEMENT_NODE) {
				continue;
			}
			if (xmlStrcmp(childnode->name, BAD_CAST "address") == 0) {
				if (!srv->address) {
					srv->address = strdup((char*)xmlNodeGetContent(childnode));
				} else {
					nc_verb_error("%s: duplicated address element", __func__);
					*error = nc_err_new(NC_ERR_BAD_ELEM);
					nc_err_set(*error, NC_ERR_PARAM_INFO_BADELEM, "/netconf/" (ssh ? "ssh" : "tls") "/call-home/applications/application/servers/address");
					nc_err_set(*error, NC_ERR_PARAM_MSG, "Duplicated address element");
					goto fail;
				}
			} else if (xmlStrcmp(childnode->name, BAD_CAST "port") == 0) {
				srv->port = atoi((char*)xmlNodeGetContent(childnode));
			}
		}
		if (srv->address == NULL || srv->port == 0) {
			nc_verb_error("%s: invalid address specification (host: %s, port: %s)", __func__, srv->address, srv->port);
			*error = nc_err_new(NC_ERR_BAD_ELEM);
			nc_err_set(*error, NC_ERR_PARAM_INFO_BADELEM, "/netconf/" (ssh ? "ssh" : "tls") "/call-home/applications/application/servers/address");
			goto fail;
		}
	}

	if (new->servers == NULL) {
		nc_verb_error("%s: no server to connect to from %s app", __func__, new->name);
		goto fail;
	}

	/* get reconnect settings */
	auxnode = find_node(node, BAD_CAST "reconnect-strategy");
	for (childnode = auxnode->children; childnode != NULL; childnode = childnode->next) {
		if (childnode->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(childnode->name, BAD_CAST "start-with") == 0) {
			auxstr = xmlNodeGetContent(childnode);
			if (xmlStrcmp(auxstr, BAD_CAST "last-connected") == 0) {
				new->start_server = 1;
			} else {
				new->start_server = 0;
			}
			xmlFree(auxstr);
		} else if (xmlStrcmp(childnode->name, BAD_CAST "interval-secs") == 0) {
			auxstr = xmlNodeGetContent(childnode);
			new->rec_interval = atoi((const char*)auxstr);
			xmlFree(auxstr);
		} else if (xmlStrcmp(childnode->name, BAD_CAST "count-max") == 0) {
			auxstr = xmlNodeGetContent(childnode);
			new->rec_count = atoi((const char*)auxstr);
			xmlFree(auxstr);
		}
	}

	/* get connection settings */
	new->connection = 0; /* persistent by default */
	auxnode = find_node(node, BAD_CAST "connection-type");
	for (childnode = auxnode->children; childnode != NULL; childnode = childnode->next) {
		if (childnode->type != XML_ELEMENT_NODE) {
			continue;
		}
		if (xmlStrcmp(childnode->name, BAD_CAST "periodic") == 0) {
			new->connection = 1;

			auxnode = find_node(childnode, BAD_CAST "timeout-mins");
			auxstr = xmlNodeGetContent(auxnode);
			new->rep_timeout = atoi((const char*)auxstr);
			xmlFree(auxstr);

			auxnode = find_node(childnode, BAD_CAST "linger-secs");
			auxstr = xmlNodeGetContent(auxnode);
			new->rep_linger = atoi((const char*)auxstr);
			xmlFree(auxstr);

			break;
		}
	}

	/* create cond and lock */
	new->ch_st = malloc(sizeof(struct client_ch_struct));
	new->ch_st->freed = 0;
	pthread_mutex_init(&new->ch_st->ch_lock, NULL);
	pthread_cond_init(&new->ch_st->ch_cond, NULL);

	pthread_create(&(new->thread), NULL, app_loop, new);

	/* insert the created app structure into the list */
	if (!callhome_apps) {
		callhome_apps = new;
		callhome_apps->next = NULL;
		callhome_apps->prev = NULL;
	} else {
		new->prev = NULL;
		new->next = callhome_apps;
		callhome_apps->prev = new;
		callhome_apps = new;
	}

	return EXIT_SUCCESS;

fail:
	for (srv = new->servers; srv != NULL;) {
		del_srv = srv;
		srv = srv->next;

		free(del_srv->address);
		free(del_srv);
	}
	free(new->name);
	free(new);

	return EXIT_FAILURE;
}

static struct ch_app* app_get(const char* name, int ssh) {
	struct ch_app *iter;

	if (name == NULL) {
		return (NULL);
	}

	for (iter = callhome_apps; iter != NULL; iter = iter->next) {
		if (iter->ssh == ssh && strcmp(iter->name, name) == 0) {
			break;
		}
	}

	return (iter);
}

static int app_rm(const char* name, int ssh) {
	struct ch_app* app;
	struct ch_server* srv, *del_srv;

	if ((app = app_get(name, ssh)) == NULL) {
		return EXIT_FAILURE;
	}

	pthread_cancel(app->thread);
	pthread_join(app->thread, NULL);

	if (app->prev) {
		app->prev->next = app->next;
	} else {
		callhome_apps = app->next;
	}
	if (app->next) {
		app->next->prev = app->prev;
	} else if (app->prev) {
		app->prev->next = NULL;
	}

	for (srv = app->servers; srv != NULL;) {
		del_srv = srv;
		srv = srv->next;
		free(del_srv->address);
		free(del_srv);
	}

	/* a valid client running, mark it for deletion */
	if (app->ch_st->freed == 0) {
		app->client->callhome_st = NULL;
		app->client->to_free = 1;
	}

	pthread_mutex_destroy(&app->ch_st->ch_lock);
	pthread_cond_destroy(&app->ch_st->ch_cond);
	free(app->ch_st);

	free(app->name);
	free(app);

	return EXIT_SUCCESS;
}

#endif

int callback_srv_netconf_srv_call_home_srv_applications_srv_application(XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err** error, int ssh) {
	char* name;

	switch (op) {
	case XMLDIFF_ADD:
		app_create(new_node, error, ssh);
		break;
	case XMLDIFF_REM:
		name = (char*)xmlNodeGetContent(find_node(old_node, BAD_CAST "name"));
		app_rm(name, ssh);
		free(name);
		break;
	case XMLDIFF_MOD:
		name = (char*)xmlNodeGetContent(find_node(old_node, BAD_CAST "name"));
		app_rm(name, ssh);
		free(name);
		app_create(new_node, error, ssh);
		break;
	default:
		;/* do nothing */
	}

	return EXIT_SUCCESS;
}

int server_transapi_init_ssh(void);
int server_transapi_init_tls(void);

/**
 * @brief Initialize plugin after loaded and before any other functions are called.

 * This function should not apply any configuration data to the controlled device. If no
 * running is returned (it stays *NULL), complete startup configuration is consequently
 * applied via module callbacks. When a running configuration is returned, libnetconf
 * then applies (via module's callbacks) only the startup configuration data that
 * differ from the returned running configuration data.

 * Please note, that copying startup data to the running is performed only after the
 * libnetconf's system-wide close - see nc_close() function documentation for more
 * information.

 * @param[out] running	Current configuration of managed device.

 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int server_transapi_init(xmlDocPtr* UNUSED(running)) {
	if (ncds_feature_isenabled("ietf-netconf-server", "ssh") &&	ncds_feature_isenabled("ietf-netconf-server", "inbound-ssh") &&
			server_transapi_init_ssh() != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	if (ncds_feature_isenabled("ietf-netconf-server", "tls") &&	ncds_feature_isenabled("ietf-netconf-server", "inbound-tls") &&
			server_transapi_init_tls() != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void server_transapi_close_ssh(void);
void server_transapi_close_tls(void);

/**
 * @brief Free all resources allocated on plugin runtime and prepare plugin for removal.
 */
void server_transapi_close(void) {
	if (ncds_feature_isenabled("ietf-netconf-server", "tls") &&	ncds_feature_isenabled("ietf-netconf-server", "inbound-tls")) {
		server_transapi_close_tls();
	}

	if (ncds_feature_isenabled("ietf-netconf-server", "ssh") &&	ncds_feature_isenabled("ietf-netconf-server", "inbound-ssh")) {
		server_transapi_close_ssh();
	}

	pthread_mutex_lock(&netopeer_options.binds_lock);
	free_all_bind_addr(&netopeer_options.binds);
	pthread_mutex_unlock(&netopeer_options.binds_lock);
}

/*
* Structure transapi_config_callbacks provide mapping between callback and path in configuration datastore.
* It is used by libnetconf library to decide which callbacks will be run.
* DO NOT alter this structure
*/
struct transapi_data_callbacks server_clbks =  {
	.callbacks_count = 0,
	.data = NULL,
	.callbacks = NULL
};

/*
* RPC callbacks
* Here follows set of callback functions run every time RPC specific for this device arrives.
* You can safely modify the bodies of all function as well as add new functions for better lucidity of code.
* Every function takes array of inputs as an argument. On few first lines they are assigned to named variables. Avoid accessing the array directly.
* If input was not set in RPC message argument in set to NULL.
*/

/*
* Structure transapi_rpc_callbacks provide mapping between callbacks and RPC messages.
* It is used by libnetconf library to decide which callbacks will be run when RPC arrives.
* DO NOT alter this structure
*/
struct transapi_rpc_callbacks server_rpc_clbks = {
	.callbacks_count = 0,
	.callbacks = {
	}
};

struct transapi server_transapi = {
	.init = server_transapi_init,
	.close = server_transapi_close,
	.get_state = server_get_state_data,
	.clbks_order = TRANSAPI_CLBCKS_LEAF_TO_ROOT,
	.data_clbks = &server_clbks,
	.rpc_clbks = &server_rpc_clbks,
	.ns_mapping = server_namespace_mapping,
	.config_modified = &server_config_modified,
	.erropt = &server_erropt,
	.file_clbks = NULL,
};