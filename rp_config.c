#include "rp_config.h"

static rp_config_command_t main_commands[] = {
    { "listen", rp_config_setting_listen },
    { "server", rp_config_setting_server },
    {  NULL,    NULL                     }
};

static rp_config_command_t server_commands[] = {
    { "address", rp_config_server_address },
    { "port",    rp_config_server_port    },
    {  NULL,     NULL                     }
};

static rp_config_command_t *commands[] = {
    main_commands,
    server_commands,
    NULL
};


int rp_config_file_parse(char *filename, rp_settings_t *s)
{
    rp_config_t cfg;

    if((cfg.f = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "fopen at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    cfg.line = 1;
    cfg.ctx = RP_CFG_MAIN_CTX;
    cfg.filename = filename;
    cfg.buffer.s.data = NULL;
    cfg.buffer.s.length = 0;
    while(!feof(cfg.f)) {
        cfg.buffer.used = 0;
        if(rp_config_setting_process(&cfg, s) != RP_SUCCESS) {
            fclose(cfg.f);
            return RP_FAILURE;
        }
    }
    free(cfg.buffer.s.data);
    fclose(cfg.f);

    return RP_SUCCESS;
}

int rp_config_setting_process(rp_config_t *cfg, rp_settings_t *s)
{
    int i, chr;
    char name[7];
    rp_config_command_t *cmd;
    do {
        if((chr = fgetc(cfg->f)) == '\n') {
            cfg->line++;
        }
    } while(RP_ISSPACE(chr));
    if(chr == RP_CFG_COMMENT) {
        while((chr = fgetc(cfg->f)) != EOF) {
            if(chr == '\n') {
                cfg->line++;
                return RP_SUCCESS;
            }
        }
    }
    if(chr == EOF) {
        if(cfg->ctx != RP_CFG_MAIN_CTX) {
            fprintf(stderr, "%s:%d unexpected EOF\n", cfg->filename, cfg->line);
            return RP_FAILURE;
        }
        return RP_SUCCESS;
    } else if(chr == RP_CFG_BLOCK_END) {
        if(cfg->ctx != RP_CFG_SRV_CTX) {
            fprintf(stderr, "%s:%d syntax error\n", cfg->filename, cfg->line);
            return RP_FAILURE;
        }
        cfg->ctx = RP_CFG_MAIN_CTX;
        return RP_SUCCESS;
    }
    for(i = 0; i < sizeof(name) && (chr >= 'a' && chr <= 'z'); i++) {
        name[i] = chr;
        chr = fgetc(cfg->f);
    }
    name[i] = '\0';
    if(!RP_ISSPACE(chr)) {
        fprintf(stderr, "%s:%d syntax error\n", cfg->filename, cfg->line);
        return RP_FAILURE;
    }
    cmd = commands[cfg->ctx];
    while(cmd->name != NULL) {
        if(strcmp(name, cmd->name) == 0) {
            break;
        }
        cmd++;
    }
    if(cmd->name == NULL) {
        fprintf(stderr, "%s:%d unknown command %s\n", cfg->filename, cfg->line, name);
        return RP_FAILURE;
    }
    return cmd->set(cfg, s);
}

int rp_config_setting_listen(rp_config_t *cfg, rp_settings_t *s)
{
    int port;
    char *ptr, *ip = "0.0.0.0";
    struct in_addr addr = { INADDR_ANY };

    if(rp_config_read_value(cfg) != RP_SUCCESS) {
        return RP_FAILURE;
    }
    if((ptr = strchr(cfg->buffer.s.data, ':')) != NULL) {
        *ptr++ = '\0';
        if(!inet_pton(AF_INET, cfg->buffer.s.data, &addr)) {
            fprintf(stderr, "%s:%d invalid address %s\n", cfg->filename, cfg->line, cfg->buffer.s.data);
            return RP_FAILURE;
        }
        ip = cfg->buffer.s.data;
    } else {
        ptr = cfg->buffer.s.data;
    }
    if((port = rp_config_parse_port(ptr)) < 0) {
        fprintf(stderr, "%s:%d invalid port %s\n", cfg->filename, cfg->line, ptr);
        return RP_FAILURE;
    }
    s->listen->address = addr.s_addr;
    s->listen->hr.address.length = strlen(ip);
    s->listen->hr.address.data = strcpy(malloc(s->listen->hr.address.length + 1), ip);
    s->listen->port = htons(port);
    s->listen->hr.port = port;
    return RP_SUCCESS;
}

int rp_config_setting_server(rp_config_t *cfg, rp_settings_t *s)
{
    int chr;
    do {
        if((chr = fgetc(cfg->f)) == '\n') {
            cfg->line++;
        }
    } while(RP_ISSPACE(chr));
    if(chr != RP_CFG_BLOCK_BEGIN) {
        fprintf(stderr, "%s:%d syntax error\n", cfg->filename, cfg->line);
        return RP_FAILURE;
    }
    if((s->servers->c = realloc(s->servers->c, ++s->servers->size * sizeof(rp_connection_t))) == NULL) {
        fprintf(stderr, "realloc at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    cfg->ctx = RP_CFG_SRV_CTX;
    s->servers->c[s->servers->size - 1].sockfd = -1;
    s->servers->c[s->servers->size - 1].data = NULL;
    s->servers->c[s->servers->size - 1].flags = 0;
    s->servers->c[s->servers->size - 1].time = 0;
    return RP_SUCCESS;
}


int rp_config_server_address(rp_config_t *cfg, rp_settings_t *s)
{
    struct in_addr *in;
    struct hostent *he;

    if(rp_config_read_value(cfg) != RP_SUCCESS) {
        return RP_FAILURE;
    }
    if((he = gethostbyname(cfg->buffer.s.data)) == NULL) {
        fprintf(stderr, "gethostbyname at %s:%d\n", __FILE__, __LINE__);
        return RP_FAILURE;
    }
    in = (struct in_addr *)he->h_addr_list[0];
    cfg->buffer.used = sprintf(cfg->buffer.s.data, "%s", inet_ntoa(*in));
    s->servers->c[s->servers->size - 1].hr.address.length = cfg->buffer.used;
    s->servers->c[s->servers->size - 1].hr.address.data = strcpy(malloc(cfg->buffer.used + 1), cfg->buffer.s.data);
    s->servers->c[s->servers->size - 1].address = in->s_addr;
    return RP_SUCCESS;
}


int rp_config_server_port(rp_config_t *cfg, rp_settings_t *s)
{
    int port;
    if(rp_config_read_value(cfg) != RP_SUCCESS) {
        return RP_FAILURE;
    }
    if((port = rp_config_parse_port(cfg->buffer.s.data)) < 0) {
        fprintf(stderr, "%s:%d invalid port %s\n", cfg->filename, cfg->line, cfg->buffer.s.data);
        return RP_FAILURE;
    }
    s->servers->c[s->servers->size - 1].port = htons(port);
    s->servers->c[s->servers->size - 1].hr.port = port;
    return RP_SUCCESS;
}


int rp_config_read_value(rp_config_t *cfg)
{
    int chr;
    do {
        if((chr = fgetc(cfg->f)) == '\n') {
            cfg->line++;
        }
    } while(RP_ISSPACE(chr));
    if(chr == EOF) {
        fprintf(stderr, "%s:%d unexpected EOF\n", cfg->filename, cfg->line);
        return RP_FAILURE;
    }
    do {
        if(cfg->buffer.used + 1 > cfg->buffer.s.length) {
            if(rp_resize_buffer(&cfg->buffer, RP_BUFFER_SIZE) != RP_SUCCESS) {
                return RP_FAILURE;
            }
        }
        cfg->buffer.s.data[cfg->buffer.used++] = chr;
        if((chr = fgetc(cfg->f)) == EOF) {
            fprintf(stderr, "%s:%d unexpected EOF\n", cfg->filename, cfg->line);
            return RP_FAILURE;
        }
    } while(chr != RP_CFG_TERMINATION);
    cfg->buffer.s.data[cfg->buffer.used] = '\0';
    return RP_SUCCESS;
}

int rp_config_parse_port(char *ptr)
{
    int port;
    if(*ptr < '1' || *ptr > '9') {
        return -1;
    }
    port = *ptr++ - '0';
    while(*ptr >= '0' && *ptr <= '9') {
        if((port = port * 10 + *ptr++ - '0') > 65535) {
            return -1;
        }
    }
    if(*ptr != '\0') {
        return -1;
    }
    return port;
}
