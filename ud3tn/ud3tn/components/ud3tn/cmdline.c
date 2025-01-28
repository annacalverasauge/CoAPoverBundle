// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/eid.h"

#include "platform/hal_io.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define QUOTE(s) #s
#define STR(s) QUOTE(s)

static struct ud3tn_cmdline_options global_cmd_opts;

#ifdef DEBUG
#define LOG_LEVELS "1|2|3|4"
#else // DEBUG
#define LOG_LEVELS "1|2|3"
#endif // DEBUG

/**
 * Helper function for parsing a 64-bit unsigned integer from a given C-string.
 */
enum ud3tn_result parse_uint64(const char *str, uint64_t *result);

/**
 * Replaces long options in argv with short ones.
 */
static void shorten_long_cli_options(const int argc, char *argv[]);

static void print_usage_text(void);

static void print_help_text(void);

const struct ud3tn_cmdline_options *parse_cmdline(int argc, char *argv[])
{
	// For now, we use a global variable. (Because why not?)
	// Though, this may be refactored easily.
	struct ud3tn_cmdline_options *result = &global_cmd_opts;
	int opt;

	// If we override sth., first deallocate
	if (result->eid)
		free(result->eid);
	if (result->cla_options)
		free(result->cla_options);

	// Set default values
	result->aap_socket = NULL;
	result->aap_node = NULL;
	result->aap_service = NULL;
	result->aap2_socket = NULL;
	result->aap2_node = NULL;
	result->aap2_service = NULL;
	result->aap2_bdm_secret = NULL;
	result->bundle_version = DEFAULT_BUNDLE_VERSION;
	result->external_dispatch = false;
	result->status_reporting = false;
	result->allow_remote_configuration = false;
	result->exit_immediately = false;
	result->lifetime_s = DEFAULT_BUNDLE_LIFETIME_S;
	result->log_level = DEFAULT_LOG_LEVEL;
	// The following values cannot be 0
	result->mbs = 0;
	// The strings are set afterwards if not provided as an option
	result->eid = NULL;
	result->cla_options = NULL;

	const char *GETOPT_SHORTOPTS = ":a:A:b:c:e:l:L:m:p:P:s:S:x:rRhud";
	int option_index = 0;

	if (!argv || argc <= 1)
		goto finish;

	shorten_long_cli_options(argc, argv);
	while ((opt = getopt(argc, argv, GETOPT_SHORTOPTS)) != -1) {
		switch (opt) {
		case 'a':
			if (!optarg || strlen(optarg) < 1) {
				LOG_ERROR("Invalid AAP node provided!");
				return NULL;
			}
			result->aap_node = strdup(optarg);
			break;
		case 'A':
			if (!optarg || strlen(optarg) < 1) {
				LOG_ERROR("Invalid AAP 2.0 node provided!");
				return NULL;
			}
			result->aap2_node = strdup(optarg);
			break;
		case 'b':
			if (!optarg || strlen(optarg) != 1 || (
					optarg[0] != '6' && optarg[0] != '7')) {
				LOG_ERROR("Invalid BP version provided!");
				return NULL;
			}
			result->bundle_version = (optarg[0] == '6') ? 6 : 7;
			break;
		case 'c':
			if (!optarg) {
				LOG_ERROR("Invalid CLA options string provided!");
				return NULL;
			}
			result->cla_options = strdup(optarg);
			break;
		case 'd':
			result->external_dispatch = true;
			break;
		case 'e':
			if (!optarg || validate_local_eid(optarg) != UD3TN_OK ||
					strcmp("dtn:none", optarg) == 0) {
				LOG_ERROR("Invalid node ID provided!");
				return NULL;
			}
			result->eid = preprocess_local_eid(optarg);
			break;
		case 'h':
			print_help_text();
			result->exit_immediately = true;
			return result;
		case 'l':
			if (parse_uint64(optarg, &result->lifetime_s)
					!= UD3TN_OK || !result->lifetime_s) {
				LOG_ERROR("Invalid lifetime provided!");
				return NULL;
			}
			break;
		case 'L':
			if (!optarg || strlen(optarg) != 1 || (
					optarg[0] != '1' && optarg[0] != '2' &&
					optarg[0] != '3' &&
					(!IS_DEBUG_BUILD ||
					 optarg[0] != '4'))) {
				LOG_ERROR("Invalid log level provided!");
				return NULL;
			}
			result->log_level = optarg[0] - '0';
			LOG_LEVEL = optarg[0] - '0';
			break;
		case 'm':
			if (parse_uint64(optarg, &result->mbs) != UD3TN_OK) {
				LOG_ERROR("Invalid maximum bundle size provided!");
				return NULL;
			}
			break;
		case 'p':
			if (!optarg || strlen(optarg) < 1) {
				LOG_ERROR("Invalid AAP port provided!");
				return NULL;
			}
			result->aap_service = strdup(optarg);
			break;
		case 'P':
			if (!optarg || strlen(optarg) < 1) {
				LOG_ERROR("Invalid AAP 2.0 port provided!");
				return NULL;
			}
			result->aap2_service = strdup(optarg);
			break;
		case 'r':
			result->status_reporting = true;
			break;
		case 'R':
			result->allow_remote_configuration = true;
			break;
		case 's':
			if (!optarg || strlen(optarg) < 1) {
				LOG_ERROR("Invalid AAP unix domain socket provided!");
				return NULL;
			}
			result->aap_socket = strdup(optarg);
			break;
		case 'S':
			if (!optarg || strlen(optarg) < 1) {
				LOG_ERROR("Invalid AAP 2.0 unix domain socket provided!");
				return NULL;
			}
			result->aap2_socket = strdup(optarg);
			break;
		case 'u':
			print_usage_text();
			result->exit_immediately = true;
			return result;
		case 'x':
			if (!optarg || strlen(optarg) == 0) {
				LOG_ERROR("Invalid variable name for AAP 2.0 secret provided!");
				return NULL;
			}

			const char *env = getenv(optarg);

			if (!env || strlen(env) == 0) {
				LOG_ERROR("No or invalid AAP 2.0 secret provided in environment!");
				return NULL;
			}
			result->aap2_bdm_secret = strdup(env);
			break;
		case ':':
			LOGF_ERROR(
				"Required argument of option '%s' is missing",
				argv[option_index + 1]
			);
			print_usage_text();
			return NULL;
		case '?':
			LOGF_ERROR(
				"Invalid option: '%s'",
				argv[option_index + 1]
			);
			print_usage_text();
			return NULL;
		}

		option_index++;
	}

finish:
	// AAP 1.0
	// use Unix domain socket by default
	if (!result->aap_socket && !result->aap_node && !result->aap_service) {
		result->aap_socket = strdup("./" DEFAULT_AAP_SOCKET_FILENAME);
	// prefer Unix domain socket over TCP
	} else if (result->aap_socket && (result->aap_node || result->aap_service)) {
		result->aap_node = NULL;
		result->aap_service = NULL;
	// set default TCP sevice port
	} else if (result->aap_node && !result->aap_service) {
		result->aap_service = strdup(DEFAULT_AAP_SERVICE);
	// set default TCP node IP
	} else if (!result->aap_node && result->aap_service) {
		result->aap_node = strdup(DEFAULT_AAP_NODE);
	}

	// AAP 2.0
	if (!result->aap2_socket && !result->aap2_node && !result->aap2_service) {
		result->aap2_socket = strdup("./" DEFAULT_AAP2_SOCKET_FILENAME);
	// prefer Unix domain socket over TCP
	} else if (result->aap2_socket && (result->aap2_node || result->aap2_service)) {
		result->aap2_node = NULL;
		result->aap2_service = NULL;
	// set default TCP sevice port
	} else if (result->aap2_node && !result->aap2_service) {
		result->aap2_service = strdup(DEFAULT_AAP2_SERVICE);
	// set default TCP node IP
	} else if (!result->aap2_node && result->aap2_service) {
		result->aap2_node = strdup(DEFAULT_AAP2_NODE);
	}

	if (!result->eid)
		result->eid = strdup(DEFAULT_NODE_ID);
	if (!result->cla_options)
		result->cla_options = strdup(DEFAULT_CLA_OPTIONS);

	return result;
}

enum ud3tn_result parse_uint64(const char *str, uint64_t *result)
{
	char *end;
	unsigned long long val;

	if (!str)
		return UD3TN_FAIL;
	errno = 0;
	val = strtoull(str, &end, 10);
	if (errno == ERANGE || end == str || *end != 0)
		return UD3TN_FAIL;
	*result = (uint64_t)val;
	return UD3TN_OK;
}

static void shorten_long_cli_options(const int argc, char *argv[])
{
	struct alias {
		char *long_form;
		char *short_form;
	};

	const struct alias aliases[] = {
		{"--aap-host", "-a"},
		{"--aap-port", "-p"},
		{"--aap-socket", "-s"},
		{"--aap2-host", "-A"},
		{"--aap2-port", "-P"},
		{"--aap2-socket", "-S"},
		{"--bdm-secret-var", "-x"},
		{"--bp-version", "-b"},
		{"--cla", "-c"},
		{"--eid", "-e"}, // deprecated, but retained for compatibility
		{"--external-dispatch", "-d"},
		{"--help", "-h"},
		{"--lifetime", "-l"},
		{"--max-bundle-size", "-m"},
		{"--node-id", "-e"},
		{"--status-reports", "-r"},
		{"--allow-remote-config", "-R"},
		{"--usage", "-u"},
		{"--log-level", "-L"},
	};

	const unsigned long aliases_count = sizeof(aliases) / sizeof(*aliases);

	for (unsigned long i = 1; i < (unsigned long)argc; i++) {
		for (unsigned long j = 0; j < aliases_count; j++) {
			if (strcmp(aliases[j].long_form, argv[i]) == 0) {
				argv[i] = aliases[j].short_form;
				break;
			}
		};
	}
}

static void print_usage_text(void)
{
	const char *usage_text = "Usage: ud3tn\n"
		"    [-a HOST, --aap-host HOST] [-p PORT, --aap-port PORT]\n"
		"    [-A HOST, --aap2-host HOST] [-P PORT, --aap2-port PORT]\n"
		"    [-b 6|7, --bp-version 6|7] [-c CLA_OPTIONS, --cla CLA_OPTIONS]\n"
		"    [-d, --external-dispatch] [-e NODE_ID, --node-id NODE_ID]\n"
		"    [-h, --help] [-l SECONDS, --lifetime SECONDS]\n"
		"    [-m BYTES, --max-bundle-size BYTES] [-r, --status-reports]\n"
		"    [-R, --allow-remote-config] [-L " LOG_LEVELS ", --log-level " LOG_LEVELS "]\n"
		"    [-s PATH --aap-socket PATH] [-S PATH --aap2-socket PATH]\n"
		"    [-u, --usage] [-x, --bdm-secret-var]\n";

	hal_io_message_printf("%s", usage_text);
}

static void print_help_text(void)
{
	const char *help_text = "Usage: ud3tn [OPTION]...\n\n"
		"Mandatory arguments to long options are mandatory for short options, too.\n"
		"\n"
		"  -a, --aap-host HOST         IP / hostname of the application agent service (may be insecure!)\n"
		"  -A, --aap2-host HOST        IP / hostname of the AAP 2.0 service (may be insecure!)\n"
		"  -b, --bp-version 6|7        bundle protocol version of bundles created via AAP\n"
		"  -c, --cla CLA_OPTIONS       configure the CLA subsystem according to the\n"
		"                                syntax documented in the man page\n"
		"  -d, --external-dispatch     do not load the internal minimal router, allow for using an AAP 2.0 BDM\n"
		"  -e, --node-id NODE ID       local node identifier (referring to the administrative endpoint)\n"
		"  -h, --help                  print this text and exit\n"
		"  -l, --lifetime SECONDS      lifetime of bundles created via AAP\n"
		"  -L, --log-level             higher or lower log level " LOG_LEVELS " specifies more or less detailed output\n"
		"  -m, --max-bundle-size BYTES bundle fragmentation threshold\n"
		"  -p, --aap-port PORT         port number of the application agent service (may be insecure!)\n"
		"  -P, --aap2-port PORT        port number of the AAP 2.0 service (may be insecure!)\n"
		"  -r, --status-reports        enable status reporting\n"
		"  -R, --allow-remote-config   allow configuration via bundles received from CLAs and authorize all bundles from AAPv1 for BDM configuration (insecure!)\n"
		"  -s, --aap-socket PATH       path to the UNIX domain socket of the application agent service\n"
		"  -S, --aap2-socket PATH      path to the UNIX domain socket of the AAP 2.0 service\n"
		"  -u, --usage                 print usage summary and exit\n"
		"  -x, --bdm-secret-var VAR    restrict AAP 2.0 BDM functions to clients providing the secret in the given environment variable\n"
		"\n"
		"Default invocation: ud3tn \\\n"
		"  -b " STR(DEFAULT_BUNDLE_VERSION) " \\\n"
		"  -c " STR(DEFAULT_CLA_OPTIONS) " \\\n"
		"  -e " DEFAULT_NODE_ID " \\\n"
		"  -l " STR(DEFAULT_BUNDLE_LIFETIME_S) " \\\n"
		"  -L " STR(DEFAULT_LOG_LEVEL) " \\\n"
		"  -m 0 \\\n"
		"  -s $PWD/" DEFAULT_AAP_SOCKET_FILENAME "\n"
		"  -S $PWD/" DEFAULT_AAP2_SOCKET_FILENAME "\n"
		"\n"
		"Please report bugs to <contact@d3tn.com>.\n";

	hal_io_message_printf("%s", help_text);
}
