/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
*/

#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

/* cJSON Types: */
#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)

#define cJSON_IsReference 256
#define cJSON_StringIsConst 512

/* The cJSON structure: */
typedef struct cJSON
{
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

typedef struct cJSON_Hooks
{
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} cJSON_Hooks;

extern cJSON *cJSON_Parse(const char *value);
extern cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated);
extern char  *cJSON_Print(cJSON *item);
extern char  *cJSON_PrintUnformatted(cJSON *item);
extern void   cJSON_Delete(cJSON *item);
extern int    cJSON_GetArraySize(cJSON *array);
extern cJSON *cJSON_GetArrayItem(cJSON *array, int item);
extern cJSON *cJSON_GetObjectItem(cJSON *const object, const char * const string);
extern cJSON *cJSON_GetObjectItemCaseSensitive(cJSON * const object, const char * const string);
extern cJSON  *cJSON_CreateNull(void);
extern cJSON  *cJSON_CreateTrue(void);
extern cJSON  *cJSON_CreateFalse(void);
extern cJSON  *cJSON_CreateBool(int b);
extern cJSON  *cJSON_CreateNumber(double num);
extern cJSON  *cJSON_CreateString(const char *string);
extern cJSON  *cJSON_CreateArray(void);
extern cJSON  *cJSON_CreateObject(void);
extern cJSON  *cJSON_CreateStringReference(const char *string);
extern cJSON  *cJSON_CreateObjectProto(cJSON *proto);
extern cJSON  *cJSON_CreateArrayProto(cJSON *proto);
extern cJSON  *cJSON_CreateIntArray(const int *numbers, int count);
extern cJSON  *cJSON_CreateFloatArray(const float *numbers, int count);
extern cJSON  *cJSON_CreateDoubleArray(const double *numbers, int count);
extern cJSON  *cJSON_CreateStringArray(const char **strings, int count);

extern void cJSON_AddItemToArray(cJSON *array, cJSON *item);
extern void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
extern void cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item);
extern void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
extern void cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item);
extern cJSON *cJSON_AddNullToObject(cJSON * const object, const char * const name);
extern cJSON *cJSON_AddTrueToObject(cJSON * const object, const char * const name);
extern cJSON *cJSON_AddFalseToObject(cJSON * const object, const char * const name);
extern cJSON *cJSON_AddBoolToObject(cJSON * const object, const char * const name, int b);
extern cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, double num);
extern cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string);
extern cJSON *cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw);

extern cJSON *cJSON_DetachItemFromArray(cJSON *array, int which);
extern void cJSON_DeleteItemFromArray(cJSON *array, int which);
extern cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string);
extern cJSON *cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string);
extern void cJSON_DeleteItemFromObject(cJSON *object, const char *string);
extern void cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string);

extern void cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem);
extern cJSON *cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem);
extern cJSON *cJSON_ReplaceItemInObject(cJSON *object,const char *string, cJSON *newitem);
extern cJSON *cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object,const char *string, cJSON *newitem);

extern cJSON *cJSON_Duplicate(cJSON *item, int recurse);
extern int cJSON_Compare(cJSON *a, cJSON *b, int case_sensitive);

extern void cJSON_Minify(char *json);

extern void cJSON_SetNumberHelper(cJSON *object, double number);
extern void cJSON_SetErrorHandler(void (*handler)(const char *fmt, ...));

#ifdef __cplusplus
}
#endif

#endif
