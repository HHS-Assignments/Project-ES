/*
 * Minimal cJSON implementation for embedded systems
 * Implements only essential functions needed for socket communication
 */

#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static void* (*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

static cJSON *cJSON_New_Item(void)
{
    cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
    if (node) {
        memset(node, 0, sizeof(cJSON));
    }
    return node;
}

cJSON *cJSON_CreateNull(void)
{
    cJSON *item = cJSON_New_Item();
    if(item) item->type = cJSON_NULL;
    return item;
}

cJSON *cJSON_CreateTrue(void)
{
    cJSON *item = cJSON_New_Item();
    if(item) item->type = cJSON_True;
    return item;
}

cJSON *cJSON_CreateFalse(void)
{
    cJSON *item = cJSON_New_Item();
    if(item) item->type = cJSON_False;
    return item;
}

cJSON *cJSON_CreateBool(int b)
{
    cJSON *item = cJSON_New_Item();
    if(item) item->type = b ? cJSON_True : cJSON_False;
    return item;
}

cJSON *cJSON_CreateNumber(double num)
{
    cJSON *item = cJSON_New_Item();
    if(item) {
        item->type = cJSON_Number;
        item->valuedouble = num;
        item->valueint = (int)num;
    }
    return item;
}

cJSON *cJSON_CreateString(const char *string)
{
    cJSON *item = cJSON_New_Item();
    if(item && string) {
        item->type = cJSON_String;
        item->valuestring = (char*)cJSON_malloc(strlen(string)+1);
        if(item->valuestring) strcpy(item->valuestring, string);
    }
    return item;
}

cJSON *cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item();
    if(item) item->type = cJSON_Array;
    return item;
}

cJSON *cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item();
    if(item) item->type = cJSON_Object;
    return item;
}

void cJSON_Delete(cJSON *c)
{
    cJSON *next;
    while (c) {
        next = c->next;
        if (!(c->type & cJSON_IsReference)) {
            if (c->child) cJSON_Delete(c->child);
            if (c->valuestring) cJSON_free(c->valuestring);
            if (c->string) cJSON_free(c->string);
        }
        cJSON_free(c);
        c = next;
    }
}

void cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
    if(!item) return;
    if(!array->child) {
        array->child = item;
    } else {
        cJSON *c = array->child;
        while(c->next) c = c->next;
        c->next = item;
        item->prev = c;
    }
}

void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    if(!item) return;
    if(item->string) cJSON_free(item->string);
    item->string = (char*)cJSON_malloc(strlen(string)+1);
    if(item->string) strcpy(item->string, string);
    cJSON_AddItemToArray(object, item);
}

cJSON *cJSON_AddNullToObject(cJSON * const object, const char * const name)
{
    cJSON *no = cJSON_CreateNull();
    if(no) cJSON_AddItemToObject(object, name, no);
    return no;
}

cJSON *cJSON_AddTrueToObject(cJSON * const object, const char * const name)
{
    cJSON *b = cJSON_CreateTrue();
    if(b) cJSON_AddItemToObject(object, name, b);
    return b;
}

cJSON *cJSON_AddFalseToObject(cJSON * const object, const char * const name)
{
    cJSON *b = cJSON_CreateFalse();
    if(b) cJSON_AddItemToObject(object, name, b);
    return b;
}

cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, double num)
{
    cJSON *n = cJSON_CreateNumber(num);
    if(n) cJSON_AddItemToObject(object, name, n);
    return n;
}

cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string)
{
    cJSON *n = cJSON_CreateString(string);
    if(n) cJSON_AddItemToObject(object, name, n);
    return n;
}

cJSON *cJSON_GetObjectItem(cJSON *const object, const char * const string)
{
    cJSON *c = object->child;
    while(c && strcmp(c->string, string)) c = c->next;
    return c;
}

cJSON *cJSON_GetObjectItemCaseSensitive(cJSON * const object, const char * const string)
{
    if (!object || !string) return NULL;
    return cJSON_GetObjectItem(object, string);
}

cJSON *cJSON_GetArrayItem(cJSON *array, int item)
{
    cJSON *c = array->child;
    while(c && item) {
        item--;
        c = c->next;
    }
    return c;
}

int cJSON_GetArraySize(cJSON *array)
{
    cJSON *c = array->child;
    int i = 0;
    while(c) {
        i++;
        c = c->next;
    }
    return i;
}

/* Parsing - simplified JSON parser */
static const char *parse_value(cJSON *item, const char *value);

static const char *parse_string(cJSON *item, const char *str)
{
    const char *ptr = str+1;
    char *ptr2;
    char buff[256];
    int len = 0;
    
    if (*str != '\"') return 0;
    
    while (*ptr != '\"' && *ptr && len < 255) {
        if (*ptr == '\\') {
            ptr++;
            if (*ptr == '\"' || *ptr == '\\' || *ptr == '/') {
                buff[len++] = *ptr;
            } else if (*ptr == 'b') {
                buff[len++] = '\b';
            } else if (*ptr == 'f') {
                buff[len++] = '\f';
            } else if (*ptr == 'n') {
                buff[len++] = '\n';
            } else if (*ptr == 'r') {
                buff[len++] = '\r';
            } else if (*ptr == 't') {
                buff[len++] = '\t';
            }
        } else {
            buff[len++] = *ptr;
        }
        ptr++;
    }
    buff[len] = 0;
    
    if (*ptr != '\"') return 0;
    
    item->type = cJSON_String;
    item->valuestring = (char*)cJSON_malloc(len+1);
    if(item->valuestring) strcpy(item->valuestring, buff);
    
    return ptr+1;
}

static const char *parse_number(cJSON *item, const char *num)
{
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;
    
    if (*num == '-') sign = -1, num++;
    
    if (*num == '0') num++;
    
    if (*num >= '1' && *num <= '9') {
        do n = (n*10.0)+(*num++ -'0');
        while (*num >= '0' && *num <= '9');
    }
    
    if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
        num++;
        do {
            n = (n*10.0)+(*num++ -'0');
            scale--;
        } while (*num >= '0' && *num <= '9');
    }
    
    if (*num == 'e' || *num == 'E') {
        num++;
        if (*num == '+') num++;
        else if (*num == '-') signsubscale = -1, num++;
        while (*num >= '0' && *num <= '9') subscale = (subscale*10)+(*num++ - '0');
    }
    
    n = sign * n * pow(10.0, (scale+subscale*signsubscale));
    
    item->type = cJSON_Number;
    item->valuedouble = n;
    item->valueint = (int)n;
    
    return num;
}

static const char *parse_array(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '[') return 0;
    
    item->type = cJSON_Array;
    value++;
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
    
    if (*value == ']') return value+1;
    
    item->child = child = cJSON_New_Item();
    if (!item->child) return 0;
    
    value = parse_value(child, value);
    if (!value) return 0;
    
    while (*value) {
        while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
        if (*value == ']') return value+1;
        if (*value != ',') return 0;
        value++;
        while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
        
        child->next = cJSON_New_Item();
        if (!child->next) return 0;
        child->next->prev = child;
        child = child->next;
        value = parse_value(child, value);
        if (!value) return 0;
    }
    
    return 0;
}

static const char *parse_object(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '{') return 0;
    
    item->type = cJSON_Object;
    value++;
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
    
    if (*value == '}') return value+1;
    
    item->child = child = cJSON_New_Item();
    if (!item->child) return 0;
    
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
    if (*value != '\"') return 0;
    
    child->string = (char*)cJSON_malloc(256);
    if (!child->string) return 0;
    
    const char *ptr = value+1;
    int len = 0;
    while (*ptr != '\"' && *ptr && len < 255) {
        child->string[len++] = *ptr++;
    }
    child->string[len] = 0;
    
    if (*ptr != '\"') return 0;
    ptr++;
    
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
    if (*ptr != ':') return 0;
    ptr++;
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
    
    value = parse_value(child, ptr);
    if (!value) return 0;
    
    while (*value) {
        while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
        if (*value == '}') return value+1;
        if (*value != ',') return 0;
        value++;
        while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
        
        child->next = cJSON_New_Item();
        if (!child->next) return 0;
        child->next->prev = child;
        child = child->next;
        
        while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
        if (*value != '\"') return 0;
        
        child->string = (char*)cJSON_malloc(256);
        if (!child->string) return 0;
        
        ptr = value+1;
        len = 0;
        while (*ptr != '\"' && *ptr && len < 255) {
            child->string[len++] = *ptr++;
        }
        child->string[len] = 0;
        
        if (*ptr != '\"') return 0;
        ptr++;
        
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
        if (*ptr != ':') return 0;
        ptr++;
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
        
        value = parse_value(child, ptr);
        if (!value) return 0;
    }
    
    return 0;
}

static const char *parse_value(cJSON *item, const char *value)
{
    if (!value) return 0;
    
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;
    
    if (!strncmp(value, "null", 4)) {
        item->type = cJSON_NULL;
        return value+4;
    }
    if (!strncmp(value, "true", 4)) {
        item->type = cJSON_True;
        return value+4;
    }
    if (!strncmp(value, "false", 5)) {
        item->type = cJSON_False;
        return value+5;
    }
    if (*value == '\"') return parse_string(item, value);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);
    if (*value == '[') return parse_array(item, value);
    if (*value == '{') return parse_object(item, value);
    
    return 0;
}

cJSON *cJSON_Parse(const char *value)
{
    cJSON *c = cJSON_New_Item();
    if (!c) return 0;
    
    if (!parse_value(c, value)) {
        cJSON_Delete(c);
        return 0;
    }
    
    return c;
}

cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated)
{
    const char *end = 0;
    cJSON *c = cJSON_Parse(value);
    if (return_parse_end) *return_parse_end = end;
    return c;
}

/* Print JSON */
static char *print_value(cJSON *item, int depth, int fmt);

static char *print_string_ptr(const char *str)
{
    const char *ptr = str;
    int len = 0;
    unsigned char token;
    
    if (!str) return cJSON_malloc(3);
    
    len = strlen(str) + 2;
    while ((token = *ptr)) {
        if (token < 32 || token == '\"' || token == '\\') len++;
        ptr++;
    }
    
    char *out = (char*)cJSON_malloc(len+1);
    if (!out) return 0;
    
    ptr = str;
    out[0] = '\"';
    int i = 1;
    
    while ((token = *ptr)) {
        if ((unsigned char)token > 31 && token != '\"' && token != '\\') {
            out[i++] = token;
        } else {
            out[i++] = '\\';
            switch(token) {
                case '\"': out[i++] = '\"'; break;
                case '\\': out[i++] = '\\'; break;
                case '\b': out[i++] = 'b'; break;
                case '\f': out[i++] = 'f'; break;
                case '\n': out[i++] = 'n'; break;
                case '\r': out[i++] = 'r'; break;
                case '\t': out[i++] = 't'; break;
            }
        }
        ptr++;
    }
    out[i++] = '\"';
    out[i] = 0;
    return out;
}

static char *print_number(cJSON *item)
{
    char *str;
    double d = item->valuedouble;
    
    if (d == 0) {
        str = (char*)cJSON_malloc(2);
        if (str) strcpy(str, "0");
    } else if (fabs(floor(d) - d) <= 1e-6 && fabs(d) < 1e15) {
        str = (char*)cJSON_malloc(21);
        if (str) sprintf(str, "%.0f", d);
    } else {
        str = (char*)cJSON_malloc(64);
        if (str) sprintf(str, "%.17g", d);
    }
    return str;
}

static char *print_array(cJSON *item, int depth, int fmt)
{
    char **entries, *out, *ptr;
    int len = 7;
    cJSON *child = item->child;
    int numentries = 0, i = 0, fail = 0;
    
    while (child) {
        numentries++;
        child = child->next;
    }
    
    if (!numentries) {
        out = (char*)cJSON_malloc(3);
        if (out) strcpy(out, "[]");
        return out;
    }
    
    entries = (char**)cJSON_malloc(numentries*sizeof(char*));
    memset(entries, 0, numentries*sizeof(char*));
    
    child = item->child;
    while (child && !fail) {
        entries[i] = print_value(child, depth+1, fmt);
        if (entries[i]) len += strlen(entries[i])+2+(fmt?1:0);
        else fail = 1;
        child = child->next;
        i++;
    }
    
    if (fail) {
        for (i = 0; i < numentries; i++) if (entries[i]) cJSON_free(entries[i]);
        cJSON_free(entries);
        return 0;
    }
    
    out = (char*)cJSON_malloc(len);
    if (!out) {
        for (i = 0; i < numentries; i++) if (entries[i]) cJSON_free(entries[i]);
        cJSON_free(entries);
        return 0;
    }
    
    ptr = out;
    *ptr = '[';
    ptr++;
    if (fmt) *ptr++ = '\n';
    
    for (i = 0; i < numentries; i++) {
        if (fmt) {
            for (int j = 0; j < depth+1; j++) *ptr++ = '\t';
        }
        strcpy(ptr, entries[i]);
        ptr += strlen(entries[i]);
        if (i < numentries-1) {
            *ptr++ = ',';
            if (fmt) *ptr++ = '\n';
        }
        cJSON_free(entries[i]);
    }
    cJSON_free(entries);
    if (fmt) {
        *ptr++ = '\n';
        for (int j = 0; j < depth; j++) *ptr++ = '\t';
    }
    *ptr++ = ']';
    *ptr = 0;
    
    return out;
}

static char *print_object(cJSON *item, int depth, int fmt)
{
    char **entries, **names, *out, *ptr;
    int len = 7, i = 0, j;
    cJSON *child = item->child;
    int numentries = 0, fail = 0;
    
    while (child) {
        numentries++;
        child = child->next;
    }
    
    if (!numentries) {
        out = (char*)cJSON_malloc(3);
        if (out) strcpy(out, "{}");
        return out;
    }
    
    entries = (char**)cJSON_malloc(numentries*sizeof(char*));
    names = (char**)cJSON_malloc(numentries*sizeof(char*));
    memset(entries, 0, numentries*sizeof(char*));
    memset(names, 0, numentries*sizeof(char*));
    
    child = item->child;
    i = 0;
    while (child) {
        names[i] = print_string_ptr(child->string);
        entries[i] = print_value(child, depth+1, fmt);
        if (names[i] && entries[i]) {
            len += strlen(names[i])+strlen(entries[i])+2+(fmt?2:0);
        } else {
            fail = 1;
        }
        child = child->next;
        i++;
    }
    
    if (fail) {
        for (i = 0; i < numentries; i++) {
            if (names[i]) cJSON_free(names[i]);
            if (entries[i]) cJSON_free(entries[i]);
        }
        cJSON_free(names);
        cJSON_free(entries);
        return 0;
    }
    
    out = (char*)cJSON_malloc(len);
    if (!out) {
        for (i = 0; i < numentries; i++) {
            if (names[i]) cJSON_free(names[i]);
            if (entries[i]) cJSON_free(entries[i]);
        }
        cJSON_free(names);
        cJSON_free(entries);
        return 0;
    }
    
    ptr = out;
    *ptr = '{';
    ptr++;
    if (fmt) *ptr++ = '\n';
    
    for (i = 0; i < numentries; i++) {
        if (fmt) {
            for (j = 0; j < depth+1; j++) *ptr++ = '\t';
        }
        strcpy(ptr, names[i]);
        ptr += strlen(names[i]);
        *ptr++ = ':';
        if (fmt) *ptr++ = '\t';
        strcpy(ptr, entries[i]);
        ptr += strlen(entries[i]);
        if (i < numentries-1) {
            *ptr++ = ',';
            if (fmt) *ptr++ = '\n';
        }
        cJSON_free(names[i]);
        cJSON_free(entries[i]);
    }
    cJSON_free(names);
    cJSON_free(entries);
    if (fmt) {
        *ptr++ = '\n';
        for (j = 0; j < depth; j++) *ptr++ = '\t';
    }
    *ptr++ = '}';
    *ptr = 0;
    
    return out;
}

static char *print_value(cJSON *item, int depth, int fmt)
{
    char *out = 0;
    if (!item) return 0;
    
    switch ((item->type) & 255) {
        case cJSON_NULL: out = (char*)cJSON_malloc(6); if (out) strcpy(out, "null"); break;
        case cJSON_False: out = (char*)cJSON_malloc(6); if (out) strcpy(out, "false"); break;
        case cJSON_True: out = (char*)cJSON_malloc(5); if (out) strcpy(out, "true"); break;
        case cJSON_Number: out = print_number(item); break;
        case cJSON_String: out = print_string_ptr(item->valuestring); break;
        case cJSON_Array: out = print_array(item, depth, fmt); break;
        case cJSON_Object: out = print_object(item, depth, fmt); break;
    }
    return out;
}

char *cJSON_Print(cJSON *item)
{
    return print_value(item, 0, 1);
}

char *cJSON_PrintUnformatted(cJSON *item)
{
    return print_value(item, 0, 0);
}
