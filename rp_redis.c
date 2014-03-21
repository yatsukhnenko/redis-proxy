#include "rp_redis.h"
#include "rp_connection.h"

static rp_command_proto_t rp_commands_3[] = {
    { "DEL", RP_MASTER_COMMAND, -2, NULL },
    { "GET", RP_SLAVE_COMMAND,   2, NULL },
    { "SET", RP_MASTER_COMMAND, -3, NULL },
    { "TTL", RP_SLAVE_COMMAND,   2, NULL },
    {  NULL, 0,                  0, NULL }
};

static rp_command_proto_t rp_commands_4[] = {
    { "AUTH", RP_LOCAL_COMMAND|RP_WITHOUT_AUTH, 2, rp_command_auth },
    { "DECR", RP_MASTER_COMMAND,                2, NULL            },
    { "DUMP", RP_SLAVE_COMMAND,                 2, NULL            },
    { "ECHO", RP_SLAVE_COMMAND,                 2, NULL            },
    { "EXEC", RP_MASTER_COMMAND,                1, NULL            },
    { "HDEL", RP_MASTER_COMMAND,               -4, NULL            },
    { "HGET", RP_SLAVE_COMMAND,                 3, NULL            },
    { "HLEN", RP_SLAVE_COMMAND,                 2, NULL            },
    { "HSET", RP_MASTER_COMMAND,                4, NULL            },
    { "INCR", RP_MASTER_COMMAND,                2, NULL            },
    { "INFO", RP_MASTER_COMMAND,               -1, NULL            },
    { "KEYS", RP_SLAVE_COMMAND,                 2, NULL            },
    { "LLEN", RP_SLAVE_COMMAND,                 2, NULL            },
    { "LPOP", RP_MASTER_COMMAND,                2, NULL            },
    { "LREM", RP_MASTER_COMMAND,                4, NULL            },
    { "LSET", RP_MASTER_COMMAND,                4, NULL            },
    { "MGET", RP_SLAVE_COMMAND,                -2, NULL            },
    { "MOVE", RP_MASTER_COMMAND,                3, NULL            },
    { "MSET", RP_MASTER_COMMAND,               -3, NULL            },
    { "PING", RP_LOCAL_COMMAND,                 1, rp_command_ping },
    { "PTTL", RP_SLAVE_COMMAND,                 2, NULL            },
    { "QUIT", RP_LOCAL_COMMAND|RP_WITHOUT_AUTH, 1, rp_command_quit },
    { "RPOP", RP_MASTER_COMMAND,                2, NULL            },
    { "SADD", RP_MASTER_COMMAND,               -3, NULL            },
    { "SAVE", RP_SLAVE_COMMAND,                 1, NULL            },
    { "SORT", RP_SLAVE_COMMAND,                -2, NULL            },
    { "SPOP", RP_MASTER_COMMAND,                2, NULL            },
    { "SREM", RP_MASTER_COMMAND,               -3, NULL            },
    { "TIME", RP_SLAVE_COMMAND,                 1, NULL            },
    { "TYPE", RP_SLAVE_COMMAND,                 2, NULL            },
    { "ZADD", RP_MASTER_COMMAND,               -4, NULL            },
    { "ZREM", RP_MASTER_COMMAND,               -3, NULL            },
    {  NULL,  0,                                0, NULL            }
};

static rp_command_proto_t rp_commands_5[] = {
    { "BITOP", RP_MASTER_COMMAND, -4, NULL },
    { "BLPOP", RP_MASTER_COMMAND, -3, NULL },
    { "BRPOP", RP_MASTER_COMMAND, -3, NULL },
    { "HKEYS", RP_SLAVE_COMMAND,   2, NULL },
    { "HMGET", RP_SLAVE_COMMAND,  -3, NULL },
    { "HMSET", RP_MASTER_COMMAND, -4, NULL },
    { "HVALS", RP_SLAVE_COMMAND,   2, NULL },
    { "LPUSH", RP_MASTER_COMMAND, -3, NULL },
    { "LTRIM", RP_MASTER_COMMAND,  4, NULL },
    { "MULTI", RP_MASTER_COMMAND,  1, NULL },
    { "RPUSH", RP_MASTER_COMMAND, -3, NULL },
    { "SCARD", RP_SLAVE_COMMAND,   2, NULL },
    { "SDIFF", RP_SLAVE_COMMAND,  -2, NULL },
    { "SETEX", RP_MASTER_COMMAND,  4, NULL },
    { "SETNX", RP_MASTER_COMMAND,  3, NULL },
    { "SMOVE", RP_MASTER_COMMAND,  4, NULL },
    { "WATCH", RP_MASTER_COMMAND, -2, NULL },
    { "ZCARD", RP_SLAVE_COMMAND,   2, NULL },
    { "ZRANK", RP_SLAVE_COMMAND,   3, NULL },
    {  NULL,   0,                  0, NULL }
};

static rp_command_proto_t rp_commands_6[] = {
    { "APPEND", RP_MASTER_COMMAND,  3, NULL },
    { "BGSAVE", RP_SLAVE_COMMAND,   1, NULL },
    { "DBSIZE", RP_SLAVE_COMMAND,   1, NULL },
    { "DECRBY", RP_MASTER_COMMAND,  3, NULL },
    { "EXISTS", RP_SLAVE_COMMAND,   2, NULL },
    { "EXPIRE", RP_MASTER_COMMAND,  3, NULL },
    { "GETBIT", RP_SLAVE_COMMAND,   3, NULL },
    { "GETSET", RP_MASTER_COMMAND,  3, NULL },
    { "HSETNX", RP_MASTER_COMMAND,  4, NULL },
    { "INCRBY", RP_MASTER_COMMAND,  3, NULL },
    { "LINDEX", RP_SLAVE_COMMAND,   3, NULL },
    { "LPUSHX", RP_MASTER_COMMAND,  3, NULL },
    { "LRANGE", RP_SLAVE_COMMAND,   4, NULL },
    { "MSETNX", RP_MASTER_COMMAND, -3, NULL },
    { "OBJECT", RP_SLAVE_COMMAND,  -2, NULL },
    { "PSETEX", RP_MASTER_COMMAND,  4, NULL },
    { "PUBSUB", RP_SLAVE_COMMAND,  -2, NULL },
    { "RENAME", RP_MASTER_COMMAND,  3, NULL },
    { "RPUSHX", RP_MASTER_COMMAND,  3, NULL },
    { "SCRIPT", RP_MASTER_COMMAND, -2, NULL },
    { "SETBIT", RP_MASTER_COMMAND,  4, NULL },
    { "SINTER", RP_SLAVE_COMMAND,  -2, NULL },
    { "STRLEN", RP_SLAVE_COMMAND,   2, NULL },
    { "SUNION", RP_MASTER_COMMAND, -2, NULL },
    { "ZCOUNT", RP_SLAVE_COMMAND,   4, NULL },
    { "ZRANGE", RP_SLAVE_COMMAND,  -4, NULL },
    { "ZSCORE", RP_SLAVE_COMMAND,   3, NULL },
    {  NULL,    0,                  0, NULL }
};

static rp_command_proto_t rp_commands_7[] = {
    { "DISCARD", RP_MASTER_COMMAND,  1, NULL },
    { "EVALSHA", RP_MASTER_COMMAND, -4, NULL },
    { "FLUSHDB", RP_MASTER_COMMAND,  1, NULL },
    { "HGETALL", RP_SLAVE_COMMAND,   2, NULL },
    { "HINCRBY", RP_MASTER_COMMAND,  4, NULL },
    { "LINSERT", RP_MASTER_COMMAND, -3, NULL },
    { "PERSIST", RP_MASTER_COMMAND,  2, NULL },
    { "PEXPIRE", RP_MASTER_COMMAND,  3, NULL },
    { "RESTORE", RP_MASTER_COMMAND,  4, NULL },
    { "UNWATCH", RP_MASTER_COMMAND,  1, NULL },
    { "ZINCRBY", RP_MASTER_COMMAND,  4, NULL },
    {  NULL,     0,                  0, NULL }
};

static rp_command_proto_t rp_commands_8[] = {
    { "BITCOUNT", RP_SLAVE_COMMAND, -2, NULL },
    { "EXPIREAT", RP_MASTER_COMMAND, 3, NULL },
    { "FLUSHALL", RP_MASTER_COMMAND, 1, NULL },
    { "GETRANGE", RP_SLAVE_COMMAND,  4, NULL },
    { "LASTSAVE", RP_SLAVE_COMMAND,  1, NULL },
    { "RENAMENX", RP_MASTER_COMMAND, 3, NULL },
    { "SETRANGE", RP_MASTER_COMMAND, 4, NULL },
    { "SMEMBERS", RP_SLAVE_COMMAND,  2, NULL },
    { "ZREVRANK", RP_SLAVE_COMMAND,  3, NULL },
    {  NULL,      0,                 0, NULL }
};

static rp_command_proto_t rp_commands_9[] = {
    { "PEXPIREAT", RP_MASTER_COMMAND, 3, NULL },
    { "RANDOMKEY", RP_SLAVE_COMMAND,  1, NULL },
    { "RPOPLPUSH", RP_MASTER_COMMAND, 3, NULL },
    { "SISMEMBER", RP_SLAVE_COMMAND,  3, NULL },
    { "ZREVRANGE", RP_SLAVE_COMMAND, -4, NULL },
    {  NULL,       0,                 0, NULL }
};

static rp_command_proto_t rp_commands_10[] = {
    { "BRPOPLPUSH", RP_MASTER_COMMAND,  4, NULL },
    { "SDIFFSTORE", RP_MASTER_COMMAND, -3, NULL },
    {  NULL,        0,                  0, NULL }
};

static rp_command_proto_t rp_commands_11[] = {
    { "INCRBYFLOAT", RP_MASTER_COMMAND,  3, NULL },
    { "SINTERSTORE", RP_MASTER_COMMAND, -3, NULL },
    { "SRANDMEMBER", RP_SLAVE_COMMAND,  -2, NULL },
    { "SUNIONSTORE", RP_MASTER_COMMAND, -3, NULL },
    { "ZINTERSTORE", RP_MASTER_COMMAND, -4, NULL },
    {  NULL,         0,                  0, NULL }
};

static rp_command_proto_t rp_commands_12[] = {
    { "BGREWRITEAOF", RP_SLAVE_COMMAND,  1, NULL },
    { "HINCRBYFLOAT", RP_MASTER_COMMAND, 4, NULL },
    {  NULL,          0,                 0, NULL }
};

static rp_command_proto_t rp_commands_13[] = {
    { "ZRANGEBYSCORE", RP_SLAVE_COMMAND, -4, NULL },
    {  NULL,           0,                 0, NULL }
};

static rp_command_proto_t rp_commands_15[] = {
    { "ZREMRANGEBYRANK", RP_MASTER_COMMAND, 3, NULL },
    {  NULL,             0,                 0, NULL }
};

static rp_command_proto_t rp_commands_16[] = {
    { "ZREMRANGEBYSCORE", RP_MASTER_COMMAND, 3, NULL },
    { "ZREVRANGEBYSCORE", RP_SLAVE_COMMAND, -3, NULL },
    {  NULL,              0,                 0, NULL }
};

static rp_command_proto_t *rp_commands[] = {
    NULL,
    NULL,
    rp_commands_3,
    rp_commands_4,
    rp_commands_5,
    rp_commands_6,
    rp_commands_7,
    rp_commands_8,
    rp_commands_9,
    rp_commands_10,
    rp_commands_11,
    rp_commands_12,
    rp_commands_13,
    NULL,
    rp_commands_15,
    rp_commands_16
};

rp_command_proto_t *rp_command_lookup(rp_string_t *name)
{
    rp_command_proto_t *ptr;

    if(name->length > 0 && name->length < RP_COMMAND_NAME_MAX &&
        (ptr = rp_commands[name->length - 1]) != NULL) {
        while(ptr->name != NULL) {
            if(strncasecmp(name->data, ptr->name, name->length) == 0) {
                return ptr;
            }
            ptr++;
        }
    }
    return NULL;
}

rp_string_t *rp_command_auth(rp_string_t *s, void *data)
{
    rp_connection_t *c = data;
    rp_client_t *client = c->data;

    if(!(c->flags & RP_AUTHENTICATED) && (
        client->cmd.argc != client->cmd.proto->argc ||
        c->settings.auth.length != client->cmd.argv.length ||
        strncmp(c->settings.auth.data, client->cmd.argv.data, c->settings.auth.length))
    ) {
        return rp_sprintf(s, "-ERR operation not permitted");
    }
    c->flags |= RP_AUTHENTICATED;
    return rp_sprintf(s, "+OK");
}

rp_string_t *rp_command_ping(rp_string_t *s, void *data)
{
    return rp_sprintf(s, "+PONG");
}

rp_string_t *rp_command_quit(rp_string_t *s, void *data)
{
    rp_connection_t *c = data;

    c->flags |= RP_SHUTDOWN;
    return rp_sprintf(s, "+OK");
}

int rp_request_parse(rp_buffer_t *buffer, rp_command_t *cmd)
{
    int i;
    char *ptr;
    rp_string_t s;

    s.data = &buffer->s.data[buffer->r];
    s.length = buffer->used - buffer->r;
    while((ptr = rp_strstr(&s, "\r\n")) != NULL) {
        buffer->r = ptr - buffer->s.data + 2;
        if(cmd->argc < RP_NULL_STRLEN) {
            cmd->txt.data = s.data;
            if(*s.data != RP_MULTI_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) <= 0 || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->i = 0;
            cmd->argc = i;
            cmd->argv.data = NULL;
            cmd->argv.length = RP_NULL_STRLEN - 1;
        } else if(cmd->argv.length < RP_NULL_STRLEN) {
            if(*s.data != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->argv.length = i;
            cmd->argv.data = NULL;
        } else {
            if(cmd->argv.data == NULL) {
                cmd->argv.data = s.data;
            }
            i = ptr - cmd->argv.data;
            if(cmd->argv.length == RP_NULL_STRLEN || cmd->argv.length == i) {
                if(!cmd->i) {
                    cmd->name.data = cmd->argv.data;
                    cmd->name.length = cmd->argv.length;
                }
                if(++cmd->i == cmd->argc) {
                    cmd->txt.length = ptr - cmd->txt.data + 2;
                    return RP_SUCCESS;
                }
                cmd->argv.data = NULL;
                cmd->argv.length = RP_NULL_STRLEN - 1;
            } else if(i > cmd->argv.length) {
                return RP_FAILURE;
            }
        }
        s.data = &buffer->s.data[buffer->r];
        s.length = buffer->used - buffer->r;
    }
    return RP_UNKNOWN;
}

int rp_reply_parse(rp_buffer_t *buffer, rp_command_t *cmd)
{
    int i;
    char *ptr;
    rp_string_t s;

    s.data = &buffer->s.data[buffer->r];
    s.length = buffer->used - buffer->r;
    while((ptr = rp_strstr(&s, "\r\n")) != NULL) {
        buffer->r = ptr - buffer->s.data + 2;
        if(cmd->argc < RP_NULL_STRLEN) {
            cmd->i = 0;
            switch(*s.data) {
                case RP_MULTI_BULK_PREFIX:
                    s.data++;
                    s.length--;
                    if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                        return RP_FAILURE;
                    } else if(i <= 0) {
                        return RP_SUCCESS;
                    }
                    cmd->argc = i;
                    cmd->argv.data = NULL;
                    cmd->argv.length = RP_NULL_STRLEN - 1;
                    break;
                case RP_BULK_PREFIX:
                    s.data++;
                    s.length--;
                    if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                        return RP_FAILURE;
                    } else if(i < 0) {
                        return RP_SUCCESS;
                    }
                    cmd->argc = 1;
                    cmd->argv.data = NULL;
                    cmd->argv.length = i;
                    break;
                case RP_INTEGER_PREFIX:
                    s.data++;
                    s.length--;
                    i = rp_strtol(&s);
                    if(s.data != ptr) {
                        return RP_FAILURE;
                    }
                case RP_STATUS_PREFIX:
                case RP_ERROR_PREFIX:
                    return RP_SUCCESS;
                default:
                    return RP_FAILURE;
            }
        } else if(cmd->argv.length < RP_NULL_STRLEN) {
            if(*s.data != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->argv.length = i;
            cmd->argv.data = NULL;
        } else {
            if(cmd->argv.data == NULL) {
                cmd->argv.data = s.data;
            }
            i = ptr - cmd->argv.data;
            if(cmd->argv.length == RP_NULL_STRLEN || cmd->argv.length == i) {
                if(++cmd->i == cmd->argc) {
                    return RP_SUCCESS;
                }
                cmd->argv.data = NULL;
                cmd->argv.length = RP_NULL_STRLEN - 1;
            } else if(i > cmd->argv.length) {
                return RP_FAILURE;
            }
        }
        s.data = &buffer->s.data[buffer->r];
        s.length = buffer->used - buffer->r;
    }
    return RP_UNKNOWN;
}
