#ifndef _RP_CONFIG_H_
#define _RP_CONFIG_H_

#include "rp_common.h"
#include "rp_connection.h"

#define RP_CFG_MAIN_CTX     0
#define RP_CFG_SRV_CTX      1

#define RP_CFG_BLOCK_BEGIN '{'
#define RP_CFG_BLOCK_END   '}'
#define RP_CFG_TERMINATION ';'
#define RP_CFG_COMMENT     '#'

#define RP_ISSPACE(c) (c == ' ' || c == '\t' || c == '\r' || c == '\n')

typedef struct {
    rp_connection_t *listen;
    rp_connection_pool_t *servers;
} rp_settings_t;

typedef struct {
    FILE *f;
    rp_buffer_t buffer;
    char *filename;
    unsigned ctx;
    unsigned line;
} rp_config_t;

typedef struct {
    char *name;
    int (*set)(rp_config_t *cfg, rp_settings_t *s);
} rp_config_command_t;

int rp_config_file_parse(char *filename, rp_settings_t *s);
int rp_config_setting_process(rp_config_t *cfg, rp_settings_t *s);
int rp_config_main_setting_server(rp_config_t *cfg, rp_settings_t *s);
int rp_config_main_setting_listen(rp_config_t *cfg, rp_settings_t *s);
int rp_config_main_setting_auth(rp_config_t *cfg, rp_settings_t *s);
int rp_config_server_setting_address(rp_config_t *cfg, rp_settings_t *s);
int rp_config_server_setting_port(rp_config_t *cfg, rp_settings_t *s);
int rp_config_server_setting_ping(rp_config_t *cfg, rp_settings_t *s);
int rp_config_read_value(rp_config_t *cfg);
int rp_config_parse_port(char *ptr);

#endif /* _RP_CONFIG_H_ */
