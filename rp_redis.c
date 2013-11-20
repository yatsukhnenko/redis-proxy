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
    { "AUTH", RP_LOCAL_COMMAND|RP_WITHOUT_AUTH, 2, rp_command_auth  },
    { "DECR", RP_MASTER_COMMAND,                2, NULL             },
    { "DUMP", RP_SLAVE_COMMAND,                 2, NULL             },
    { "ECHO", RP_LOCAL_COMMAND,                 2, rp_command_error },
    { "EVAL", RP_LOCAL_COMMAND,                -3, rp_command_error },
    { "EXEC", RP_MASTER_COMMAND,                1, NULL             },
    { "HDEL", RP_MASTER_COMMAND,               -4, NULL             },
    { "HGET", RP_SLAVE_COMMAND,                 3, NULL             },
    { "HLEN", RP_SLAVE_COMMAND,                 2, NULL             },
    { "HSET", RP_MASTER_COMMAND,                4, NULL             },
    { "INCR", RP_MASTER_COMMAND,                2, NULL             },
    { "INFO", RP_MASTER_COMMAND,                2, NULL             },
    { "KEYS", RP_SLAVE_COMMAND,                 2, NULL             },
    { "LLEN", RP_SLAVE_COMMAND,                 2, NULL             },
    { "LPOP", RP_MASTER_COMMAND,                2, NULL             },
    { "LREM", RP_MASTER_COMMAND,                4, NULL             },
    { "LSET", RP_MASTER_COMMAND,                4, NULL             },
    { "MGET", RP_SLAVE_COMMAND,                -2, NULL             },
    { "MOVE", RP_MASTER_COMMAND,                3, NULL             },
    { "MSET", RP_MASTER_COMMAND,               -3, NULL             },
    { "PING", RP_LOCAL_COMMAND,                 1, rp_command_ping  },
    { "PTTL", RP_SLAVE_COMMAND,                 2, NULL             },
    { "QUIT", RP_LOCAL_COMMAND|RP_WITHOUT_AUTH, 1, rp_command_quit  },
    { "RPOP", RP_MASTER_COMMAND,                2, NULL             },
    { "SADD", RP_MASTER_COMMAND,               -3, NULL             },
    { "SAVE", RP_SLAVE_COMMAND,                 1, NULL             },
    { "SORT", RP_SLAVE_COMMAND,                -2, NULL             },
    { "SPOP", RP_MASTER_COMMAND,                2, NULL             },
    { "SREM", RP_MASTER_COMMAND,               -3, NULL             },
    { "SYNC", RP_LOCAL_COMMAND,                 1, rp_command_error },
    { "TIME", RP_LOCAL_COMMAND,                 1, rp_command_time  },
    { "TYPE", RP_SLAVE_COMMAND,                 2, NULL             },
    { "ZADD", RP_MASTER_COMMAND,               -4, NULL             },
    { "ZREM", RP_MASTER_COMMAND,               -3, NULL             },
    {  NULL,  0,                                0, NULL             }
};

static rp_command_proto_t rp_commands_5[] = {
    { "BITOP", RP_MASTER_COMMAND, -4, NULL             },
    { "BLPOP", RP_MASTER_COMMAND, -3, NULL             },
    { "BRPOP", RP_MASTER_COMMAND, -3, NULL             },
    { "DEBUG", RP_LOCAL_COMMAND,  -3, rp_command_error },
    { "HKEYS", RP_SLAVE_COMMAND,   2, NULL             },
    { "HMGET", RP_SLAVE_COMMAND,  -3, NULL             },
    { "HMSET", RP_MASTER_COMMAND, -4, NULL             },
    { "HVALS", RP_SLAVE_COMMAND,   2, NULL             },
    { "LPUSH", RP_MASTER_COMMAND, -3, NULL             },
    { "LTRIM", RP_MASTER_COMMAND,  4, NULL             },
    { "MULTI", RP_MASTER_COMMAND,  1, NULL             },
    { "RPUSH", RP_MASTER_COMMAND, -3, NULL             },
    { "SCARD", RP_SLAVE_COMMAND,   2, NULL             },
    { "SDIFF", RP_SLAVE_COMMAND,  -2, NULL             },
    { "SETEX", RP_MASTER_COMMAND,  4, NULL             },
    { "SETNX", RP_MASTER_COMMAND,  3, NULL             },
    { "SMOVE", RP_MASTER_COMMAND,  4, NULL             },
    { "WATCH", RP_MASTER_COMMAND, -2, NULL             },
    { "ZCARD", RP_SLAVE_COMMAND,   2, NULL             },
    { "ZRANK", RP_SLAVE_COMMAND,   3, NULL             },
    {  NULL, 0,                    1, NULL             }
};

static rp_command_proto_t rp_commands_6[] = {
    { "APPEND", RP_MASTER_COMMAND,  3, NULL             },
    { "BGSAVE", RP_SLAVE_COMMAND,   1, NULL             },
    { "CLIENT", RP_LOCAL_COMMAND,  -2, rp_command_error },
    { "CONFIG", RP_LOCAL_COMMAND,  -3, rp_command_error },
    { "DBSIZE", RP_SLAVE_COMMAND,   1, NULL             },
    { "DECRBY", RP_MASTER_COMMAND,  3, NULL             },
    { "EXISTS", RP_SLAVE_COMMAND,   2, NULL             },
    { "EXPIRE", RP_MASTER_COMMAND,  3, NULL             },
    { "GETBIT", RP_SLAVE_COMMAND,   3, NULL             },
    { "GETSET", RP_MASTER_COMMAND,  3, NULL             },
    { "HSETNX", RP_MASTER_COMMAND,  4, NULL             },
    { "INCRBY", RP_MASTER_COMMAND,  3, NULL             },
    { "LINDEX", RP_SLAVE_COMMAND,   3, NULL             },
    { "LPUSHX", RP_MASTER_COMMAND,  3, NULL             },
    { "LRANGE", RP_SLAVE_COMMAND,   4, NULL             },
    { "MSETNX", RP_MASTER_COMMAND, -3, NULL             },
    { "OBJECT", RP_SLAVE_COMMAND,  -2, NULL             },
    { "PSETEX", RP_MASTER_COMMAND,  4, NULL             },
    { "PUBSUB", RP_SLAVE_COMMAND,  -2, NULL             },
    { "RENAME", RP_MASTER_COMMAND,  3, NULL             },
    { "RPUSHX", RP_MASTER_COMMAND,  3, NULL             },
    { "SCRIPT", RP_MASTER_COMMAND, -2, NULL             },
    { "SELECT", RP_LOCAL_COMMAND,   2, rp_command_error },
    { "SETBIT", RP_MASTER_COMMAND,  4, NULL             },
    { "SINTER", RP_SLAVE_COMMAND,  -2, NULL             },
    { "STRLEN", RP_SLAVE_COMMAND,   2, NULL             },
    { "SUNION", RP_MASTER_COMMAND, -2, NULL             },
    { "ZCOUNT", RP_SLAVE_COMMAND,   4, NULL             },
    { "ZRANGE", RP_SLAVE_COMMAND,  -4, NULL             },
    { "ZSCORE", RP_SLAVE_COMMAND,   3, NULL             },
    {  NULL,    0,                  1, NULL             }
};

static rp_command_proto_t rp_commands_7[] = {
    { "DISCARD", RP_MASTER_COMMAND,  1, NULL             },
    { "EVALSHA", RP_MASTER_COMMAND, -4, NULL             },
    { "FLUSHDB", RP_MASTER_COMMAND,  1, NULL             },
    { "HGETALL", RP_SLAVE_COMMAND,   2, NULL             },
    { "HINCRBY", RP_MASTER_COMMAND,  4, NULL             },
    { "LINSERT", RP_MASTER_COMMAND, -3, NULL             },
    { "MIGRATE", RP_LOCAL_COMMAND,   6, rp_command_error },
    { "MONITOR", RP_LOCAL_COMMAND,   1, rp_command_error },
    { "PERSIST", RP_MASTER_COMMAND,  2, NULL             },
    { "PEXPIRE", RP_MASTER_COMMAND,  3, NULL             },
    { "PUBLISH", RP_LOCAL_COMMAND,  -2, rp_command_error },
    { "RESTORE", RP_MASTER_COMMAND,  4, NULL             },
    { "SLAVEOF", RP_LOCAL_COMMAND,   3, rp_command_error },
    { "SLOWLOG", RP_LOCAL_COMMAND,  -2, rp_command_error },
    { "UNWATCH", RP_MASTER_COMMAND,  1, NULL             },
    { "ZINCRBY", RP_MASTER_COMMAND,  4, NULL             },
    {  NULL,     0,                  0, NULL             }
};

static rp_command_proto_t rp_commands_8[] = {
    { "BITCOUNT", RP_SLAVE_COMMAND, -2, NULL             },
    { "EXPIREAT", RP_MASTER_COMMAND, 3, NULL             },
    { "FLUSHALL", RP_MASTER_COMMAND, 1, NULL             },
    { "GETRANGE", RP_SLAVE_COMMAND,  4, NULL             },
    { "LASTSAVE", RP_SLAVE_COMMAND,  1, NULL             },
    { "RENAMENX", RP_MASTER_COMMAND, 3, NULL             },
    { "SETRANGE", RP_MASTER_COMMAND, 4, NULL             },
    { "SHUTDOWN", RP_LOCAL_COMMAND, -1, rp_command_error },
    { "SMEMBERS", RP_SLAVE_COMMAND,  2, NULL             },
    { "ZREVRANK", RP_SLAVE_COMMAND,  3, NULL             },
    {  NULL,      0,                 0, 0                }
};

static rp_command_proto_t rp_commands_9[] = {
    { "PEXPIREAT", RP_MASTER_COMMAND, 3, NULL             },
    { "RANDOMKEY", RP_SLAVE_COMMAND,  1, NULL             },
    { "RPOPLPUSH", RP_MASTER_COMMAND, 3, NULL             },
    { "SISMEMBER", RP_SLAVE_COMMAND,  3, NULL             },
    { "SUBSCRIBE", RP_LOCAL_COMMAND, -2, rp_command_error },
    { "ZREVRANGE", RP_SLAVE_COMMAND, -4, NULL             },
    {  NULL,       0,                 0, NULL             }
};

static rp_command_proto_t rp_commands_10[] = {
    { "BRPOPLPUSH", RP_MASTER_COMMAND,  4, NULL             },
    { "PSUBSCRIBE", RP_LOCAL_COMMAND,  -2, rp_command_error },
    { "SDIFFSTORE", RP_MASTER_COMMAND, -3, NULL             },
    {  NULL,        0,                  0, NULL             }
};

static rp_command_proto_t rp_commands_11[] = {
    { "INCRBYFLOAT", RP_MASTER_COMMAND,  3, NULL             },
    { "SINTERSTORE", RP_MASTER_COMMAND, -3, NULL             },
    { "SRANDMEMBER", RP_SLAVE_COMMAND,  -2, NULL             },
    { "SUNIONSTORE", RP_MASTER_COMMAND, -3, NULL             },
    { "UNSUBSCRIBE", RP_LOCAL_COMMAND,  -1, rp_command_error },
    { "ZINTERSTORE", RP_MASTER_COMMAND, -4, NULL             },
    {  NULL,         0,                  0, NULL             }
};

static rp_command_proto_t rp_commands_12[] = {
    { "BGREWRITEAOF", RP_SLAVE_COMMAND,  1, NULL             },
    { "HINCRBYFLOAT", RP_MASTER_COMMAND, 4, NULL             },
    { "PUNSUBSCRIBE", RP_LOCAL_COMMAND, -1, rp_command_error },
    {  NULL,          0,                 0, NULL             }
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

void rp_command_auth(void *data)
{
    rp_connection_t *c = data;
    rp_client_t *client = c->data;

    if(!(c->flags & RP_AUTHENTICATED) && (
        client->cmd.argc != client->cmd.proto->argc ||
        c->auth.length != client->cmd.argv[1].length ||
        strncmp(c->auth.data, client->cmd.argv[1].data, c->auth.length))) {
        client->buffer.used = sprintf(client->buffer.s.data, "-ERR operation not permitted\r\n");
    } else {
        client->buffer.used = sprintf(client->buffer.s.data, "+OK\r\n");
        c->flags |= RP_AUTHENTICATED;
    }
    client->buffer.r = client->buffer.w = 0;
    c->flags |= RP_ALREADY;
}

void rp_command_ping(void *data)
{
    rp_connection_t *c = data;
    rp_client_t *client = c->data;

    client->buffer.r = client->buffer.w = 0;
    client->buffer.used = sprintf(client->buffer.s.data, "+PONG\r\n");
    c->flags |= RP_ALREADY;
}

void rp_command_quit(void *data)
{
    rp_connection_t *c = data;
    rp_client_t *client = c->data;

    client->buffer.r = client->buffer.w = 0;
    client->buffer.used = sprintf(client->buffer.s.data, "+OK\r\n");
    c->flags |= RP_SHUTDOWN;
    c->flags |= RP_ALREADY;
}

void rp_command_time(void *data)
{
    int s, u;
    struct timeval tv;
    char sec[16], usec[16];
    rp_connection_t *c = data;
    rp_client_t *client = c->data;

    if(gettimeofday(&tv, NULL) < 0) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    }
    s = sprintf(sec, "%d", (int)tv.tv_sec);
    u = sprintf(usec, "%ld", (long)tv.tv_usec);
    client->buffer.r = client->buffer.w = 0;
    client->buffer.used = sprintf(client->buffer.s.data, "*2\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n", s, sec, u, usec);
    c->flags |= RP_ALREADY;
}

void rp_command_error(void *data)
{ 
    rp_connection_t *c = data;
    rp_client_t *client = c->data;

    client->buffer.r = client->buffer.w = 0;
    client->buffer.used = sprintf(client->buffer.s.data, "-Service unavailable\r\n");
    c->flags |= RP_SHUTDOWN;
    c->flags |= RP_ALREADY;
}
