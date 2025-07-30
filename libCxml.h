#ifndef LIBCXML_H
#define LIBCXML_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// <30.07.2025 - start version>
#define LIBCXML_VERSION 		1

#define LIBCXML_MAX_TOKEN		32

typedef enum {
	LIBCXML_OK,
	LIBCXML_ERROR,
	LIBCXML_EOF,
	LIBCXML_NOTAG_CLOSE,
}libCxml_result;

typedef enum {
	LIBCXML_TOKEN_TYPE_IS_UNKNOWN,
	LIBCXML_TOKEN_TYPE_IS_TAG_OPEN,
	LIBCXML_TOKEN_TYPE_IS_TAG_VAL,
	LIBCXML_TOKEN_TYPE_IS_ATTR,
	LIBCXML_TOKEN_TYPE_IS_ATTR_VAL,
	LIBCXML_TOKEN_TYPE_IS_TAG_CLOSE,
}libCxml_type_token_t;

typedef struct {
	uint32_t start;
	uint32_t len;
	libCxml_type_token_t type;
}libCxml_token_t;

typedef struct {
	uint8_t num_token;
	libCxml_token_t token[LIBCXML_MAX_TOKEN];
}libCxml_token_data_t;
/*******************************************************/
typedef struct {
	uint8_t lenght;
	int *arr;
}libCxml_split_arr_typedef;

typedef enum {
	LIBCXML_UNKNOWN,
	LIBCXML_IS_TAG,
	LIBCXML_IS_TAG_VAL,
	LIBCXML_IS_ATTR,
	LIBCXML_IS_ATTR_VAL,
}libCxml_type_element_t;

typedef struct{
	libCxml_type_element_t type;
	char *value;
}libCxml_element_t;

typedef struct libCxml_node{
	uint8_t num_element;
	libCxml_element_t *element;
	struct libCxml_node *first_child;
	struct libCxml_node *next_child;
	bool need_close_tag; 
}libCxml_node_t;

typedef struct libCxml_document {
	char version[32];
	char encoding[32];
	libCxml_node_t *nodes;
	bool no_file;
}libCxml_document_t;

libCxml_document_t* libCxml_parse_document(const char *path);
void libCxml_free_document(libCxml_document_t *doc);

libCxml_node_t *libCxml_GetRootNode(libCxml_node_t *node);
libCxml_node_t *libCxml_GetChildNode(libCxml_node_t *node, const char *str);
libCxml_node_t *libCxml_FirstChildNode(libCxml_node_t *node);
libCxml_node_t *libCxml_NextSiblingNode(libCxml_node_t *node);
char *libCxml_getTagValue(libCxml_node_t *node);
char *libCxml_getAttributeValue(libCxml_node_t *node, const char *attr);

libCxml_split_arr_typedef *libCxml_split_int(const char *str);
void libCxml_split_free(libCxml_split_arr_typedef *arr);

#endif
