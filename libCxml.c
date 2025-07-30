#include "libCxml.h"
#include <ctype.h>
#include <stdlib.h>

/* debug */
#include "debug.h"
#define LIBCXML_DBG(...) print(__VA_ARGS__)

/* select file system */
#define USE_FATFS

#ifdef USE_FATFS
#include "ff.h"

#define LIBCXML_READ	FA_READ
#define LIBCXML_WRITE	FA_WRITE

FIL libCxml_file;
#else
/* other implementations */
#endif

/* other defines */
#define MAX_SIZE_DOCUMENT		512

typedef enum{
	LIBCXML_HDR_STATE_START,
	LIBCXML_HDR_STATE_END
}libCxml_hdr_state_typeDef;

/* functions for access file system */
static libCxml_result libCxml_fopen(const char *path, uint8_t opt)
{
	#ifdef USE_FATFS
	if(f_open(&libCxml_file, path, opt) == FR_OK) return LIBCXML_OK;
	#endif
	
	return LIBCXML_ERROR;
}

static libCxml_result libCxml_fclose(void)
{
	#ifdef USE_FATFS
	f_close(&libCxml_file);
	#endif
	
	return LIBCXML_OK;
}

static libCxml_result libCxml_fseek(uint32_t offset)
{
	#ifdef USE_FATFS
	if(f_lseek(&libCxml_file, offset) == FR_OK) return LIBCXML_OK;
	#endif
	
	return LIBCXML_ERROR;
}

static libCxml_result libCxml_fgets(uint8_t *buffer, uint32_t size)
{
	#ifdef USE_FATFS
	if(f_gets((TCHAR*)buffer, MAX_SIZE_DOCUMENT, &libCxml_file) != NULL) return LIBCXML_OK;
	#endif
	
	return LIBCXML_ERROR;
}

static libCxml_result libCxml_fwrite(uint8_t *buffer, uint32_t size)
{
	#ifdef USE_FATFS

	#endif
	
	return LIBCXML_OK;
}

static uint32_t libCxml_ftell(void)
{
	#ifdef USE_FATFS
	return f_tell(&libCxml_file);
	#endif
	
	return 0;
}

/***********************************************************************************************
***********************************************************************************************/
static uint32_t libCxml_substr(char *str, uint32_t PosStart, char end, char *s)
{
	uint8_t len = 0;
	uint8_t PosEnd = 0; 
	uint8_t StrLen = strlen(str);
	if(StrLen == 0) return 0;
    
	for(; str[PosStart + PosEnd] != end; PosEnd++){
			len++;
			if(StrLen == len) return 0;
	}
	if(s == NULL) return len; 
	
	for(uint8_t i = 0; i < len; i++) s[i] = str[i + PosStart];
	s[len] = '\0';
	return len;
}

/* -1 - parsing error, 0 - not header, 1 - header ok */
static int8_t libCxml_search_hdr(libCxml_document_t *doc, char *buffer)
{
	bool found_equ = false;
	uint16_t offset = 0;
	
	libCxml_fseek(0);
	if(libCxml_fgets((uint8_t*)buffer, MAX_SIZE_DOCUMENT) == LIBCXML_OK){
		if(memcmp(buffer, "<?xml ", 6) == 0){
			offset = 6;
			while(buffer[offset] != '\0'){
				if(buffer[offset] == '<') return -1;
				if(buffer[offset] == '?'){
					if(buffer[offset + 1] == '>'){
						offset+=2;
						break;
					}
					else return -1;
				}
				offset++;
			}

			char *str = strstr(buffer, "version");
			if(str != NULL){
				for(uint16_t i = (str - buffer); i < offset; i++){
					if(buffer[i] == '=') found_equ = true;
					if(buffer[i] == '\"'){
						if(found_equ){
							libCxml_substr(&buffer[i], 1, '\"', doc->version);
							break;
						}
						else return -1;
					}						
				}
				found_equ = false;
				str = NULL;
			}
			
			str = strstr(buffer, "encoding");
			if(str != NULL){
				for(uint16_t i = (str - buffer); i < offset; i++){
					if(buffer[i] == '=') found_equ = true;
					if(buffer[i] == '\"'){
						if(found_equ){
							libCxml_substr(&buffer[i], 1, '\"', doc->encoding);
							break;
						}
						else return -1;
					}						
				}
			}
		}
		else return 0;
	}
	else return -1;
	
	return 1;
}


static libCxml_node_t *libCxml_create_node(const char *buffer, libCxml_token_data_t *token_data)
{
	uint8_t num = 0;
	
	libCxml_node_t *node = (libCxml_node_t*)pvPortCalloc(1, sizeof(libCxml_node_t));
	if(node == NULL) return NULL;

	node->element = (libCxml_element_t*)pvPortCalloc(token_data->num_token, sizeof(libCxml_element_t));
	if(node->element == NULL) return node;

	for(uint8_t i = 0; i < token_data->num_token; i++){
		if(token_data->token[i].type == LIBCXML_TOKEN_TYPE_IS_TAG_CLOSE) continue;
		node->element[i].type = (libCxml_type_element_t)token_data->token[i].type;
		node->element[i].value = pvPortCalloc((token_data->token[i].len + 1), sizeof(char));
		if(token_data->token[i].len != 0) {
			if(node->element[i].value != NULL) {
				memcpy(node->element[i].value, &buffer[token_data->token[i].start], token_data->token[i].len);
				num++;
			}
		}
		else num++;
	}
	node->num_element = num;
	return node;
}

static bool _libCxml_symbol_valid(char symbol)
{
	switch(symbol){
		case ',': return true;
		case '!': return true;	
		case '@': return true;
		case '$': return true;
		case '%': return true;	
		case '^': return true;
		case '*': return true;
		case '(': return true;	
		case ')': return true;
		case '_': return true;
		case '+': return true;	
		case '\\': return true;
		case '/': return true;
		case '-': return true;	
		case ';': return true;
		case ':': return true;	
		case '?': return true;
		case '=': return true;
		case '~': return true;	
		case '`': return true;
	}
	
	return false;
}

static libCxml_result libCxml_parse(const char *buffer, libCxml_token_data_t *token_data, bool *close_tag_present)
{
	bool found_space = false;
	bool found_equ = false;
	bool parse_success = false;
	bool symbol_valid = false;
	bool attr_found = false;
	bool empty_val = false;
	uint8_t found_quotes = 0;
	uint16_t offset = 0;
	libCxml_type_token_t token_type = LIBCXML_TOKEN_TYPE_IS_UNKNOWN;
	uint16_t str_len = 0;
	
	*close_tag_present = false;
	token_data->num_token = 0;
	token_data->token[token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_UNKNOWN;
	
	if(libCxml_fgets((uint8_t*)buffer, MAX_SIZE_DOCUMENT) == LIBCXML_OK){
		str_len = strlen(buffer) - 1;
		char *str = strchr(buffer, '<');
		if(str != NULL){
			offset = (str - buffer);
			if(str[1] != '/'){
				offset++;
				while(offset != str_len){
					if(!isalnum(buffer[offset])){
						if((found_quotes == 1) && _libCxml_symbol_valid(buffer[offset]))symbol_valid = true;
						else if(isspace(buffer[offset])) {
							if(!found_space){
								if(token_type == LIBCXML_TOKEN_TYPE_IS_TAG_OPEN){
									token_data->token[token_data->num_token].len = (offset - token_data->token[token_data->num_token].start);
									token_data->token[++token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_UNKNOWN;
								}
								else if(token_type == LIBCXML_TOKEN_TYPE_IS_ATTR_VAL) token_type = LIBCXML_TOKEN_TYPE_IS_TAG_OPEN;
								found_quotes = 0;
								found_space = true;
							}
							else if(found_quotes == 1) symbol_valid = true;
							else if(token_type == LIBCXML_TOKEN_TYPE_IS_ATTR){
									token_data->token[token_data->num_token].len = (offset - token_data->token[token_data->num_token].start);
									token_data->token[++token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_UNKNOWN;
									attr_found = true;
							}
						}
						else if(buffer[offset] == '='){
							if(found_equ) return LIBCXML_ERROR;
							found_equ = true;
							if(!found_space) return LIBCXML_ERROR;
							if((token_type == LIBCXML_TOKEN_TYPE_IS_ATTR) && (!attr_found)){
								token_data->token[token_data->num_token].len = (offset - token_data->token[token_data->num_token].start);
								token_data->token[++token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_UNKNOWN;
							}
							attr_found = false;
						}
						else if(buffer[offset] == '\"'){ 
							found_quotes++;
							if((!found_equ) && (found_quotes != 2))return LIBCXML_ERROR;
							if(empty_val){
								token_type = LIBCXML_TOKEN_TYPE_IS_ATTR_VAL;
								token_data->token[token_data->num_token].start = offset;
								token_data->token[token_data->num_token].type = token_type;
								empty_val = false;
							}
							else empty_val = true;
							if(token_type == LIBCXML_TOKEN_TYPE_IS_ATTR_VAL){
								token_data->token[token_data->num_token].len = (offset - token_data->token[token_data->num_token].start);
								token_data->token[++token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_UNKNOWN;
							}
							if(found_quotes == 2){
								found_space = false;
								found_quotes = 0;
							}
							found_equ = false;
						}
						else if((buffer[offset] == '>') || ((buffer[offset] == '/') && (buffer[offset + 1] == '>'))){
							if(found_equ)return LIBCXML_ERROR;
							else if(found_quotes & 0x03) return LIBCXML_ERROR;	
							if(token_type == LIBCXML_TOKEN_TYPE_IS_TAG_OPEN){
								token_data->token[token_data->num_token].len = (offset - token_data->token[token_data->num_token].start);
								token_data->token[++token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_UNKNOWN;
							}
							if(buffer[offset] == '/') {
								*close_tag_present = true;
								offset++;
							}
							found_quotes = 0;
							parse_success = true;
							break;
						}
						else return LIBCXML_ERROR;
					}
					else symbol_valid = true;
					
					if(symbol_valid) {
						empty_val = false;
						if(token_data->token[token_data->num_token].type == LIBCXML_TOKEN_TYPE_IS_UNKNOWN){
							token_data->token[token_data->num_token].start = offset;
							if(token_type == LIBCXML_TOKEN_TYPE_IS_UNKNOWN)token_type = LIBCXML_TOKEN_TYPE_IS_TAG_OPEN;
							else if(token_type == LIBCXML_TOKEN_TYPE_IS_TAG_OPEN)token_type = LIBCXML_TOKEN_TYPE_IS_ATTR;
							else if(token_type == LIBCXML_TOKEN_TYPE_IS_ATTR) token_type = LIBCXML_TOKEN_TYPE_IS_ATTR_VAL;
							else if(token_type == LIBCXML_TOKEN_TYPE_IS_ATTR_VAL)token_type = LIBCXML_TOKEN_TYPE_IS_TAG_OPEN;
							token_data->token[token_data->num_token].type = token_type;
						}
						symbol_valid = false;
					}
					offset++;
				}
				offset++;
				str = NULL;
			}
			else parse_success = true;
		}
		else return LIBCXML_ERROR;
		if(!parse_success) return LIBCXML_ERROR;
		
		if(offset != str_len){
			token_data->token[token_data->num_token].start = offset;
			token_data->token[token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_TAG_VAL;
			while(offset != str_len){
				if(!isalnum(buffer[offset])) {
					if(buffer[offset] == '<') {
						if(token_data->token[token_data->num_token].start != offset){
							token_data->token[token_data->num_token].len = (offset - token_data->token[token_data->num_token].start);
							token_data->num_token++;
						}
						break;
					}
					else if(buffer[offset] == '&')return LIBCXML_ERROR;
				}
				offset++;
			}
		}

		str = strchr(&buffer[offset], '<');
		if(str != NULL){
			offset = ((str + 1) - buffer);
			if(buffer[offset++] != '/')return LIBCXML_ERROR;
			if(*close_tag_present) return LIBCXML_ERROR;
			token_data->token[token_data->num_token].start = offset;
			token_data->token[token_data->num_token].type = LIBCXML_TOKEN_TYPE_IS_TAG_CLOSE;
			while(offset != str_len){
				if(!isalnum(buffer[offset])) {
					if(buffer[offset] != '>')return LIBCXML_ERROR;
					else {
						*close_tag_present = true;
						break;
					}
				}
				offset++;
			}
			if(!(*close_tag_present)) return LIBCXML_ERROR;
			token_data->token[token_data->num_token].len = (offset - token_data->token[token_data->num_token].start);
			token_data->num_token++;
		}
	}
	else return LIBCXML_EOF;
	
	if((token_data->token[0].type == LIBCXML_TOKEN_TYPE_IS_TAG_OPEN) && (token_data->token[token_data->num_token - 1].type == LIBCXML_TOKEN_TYPE_IS_TAG_CLOSE)){
		if(memcmp(&buffer[token_data->token[0].start], &buffer[token_data->token[token_data->num_token - 1].start], token_data->token[0].len) != 0) return LIBCXML_ERROR;
	}

	return LIBCXML_OK;
}

static libCxml_node_t *libCxml_create_list(const char *buffer, libCxml_token_data_t *token_data, libCxml_node_t *parent, libCxml_result *result)
{
	bool close_tag = false;
	libCxml_result parse_result;
	libCxml_node_t *node = NULL;

	parse_result = libCxml_parse(buffer, token_data, &close_tag);
	if(parse_result == LIBCXML_ERROR) {
		*result = LIBCXML_ERROR;
		return NULL;
	}
	else if(parse_result == LIBCXML_EOF) return NULL;
	
	else if(parse_result == LIBCXML_OK){
		if(token_data->token[0].type == LIBCXML_TOKEN_TYPE_IS_TAG_CLOSE){
			for(uint8_t i = 0; i < parent->num_element; i++){
				if(parent->element[i].type == LIBCXML_IS_TAG){
					if(memcmp(parent->element[i].value, &buffer[token_data->token[0].start], token_data->token[0].len) != 0) *result = LIBCXML_ERROR;
				}
			}
			return NULL; 
		}
		node = libCxml_create_node(buffer, token_data);
		if(!close_tag){
			if(node->first_child == NULL)node->first_child = libCxml_create_list(buffer, token_data, node, result);
			node->next_child = libCxml_create_list(buffer, token_data, parent, result); 
		}
		else node->next_child = libCxml_create_list(buffer, token_data, parent, result);	
	}
	return node;
}

static void libCxml_free(libCxml_node_t *node)
{
	if(node == NULL) return;
	for(uint8_t i = 0; i < node->num_element; i++) vPortFree(node->element[i].value);
 	vPortFree(node->element);
	if(node->first_child != NULL) libCxml_free(node->first_child);
	if(node->next_child != NULL) libCxml_free(node->next_child);
	vPortFree(node);
}

void libCxml_free_document(libCxml_document_t *doc)
{
	libCxml_free(doc->nodes);
	doc->nodes = NULL;
}

static void libCxml_print(libCxml_node_t *node)
{
	for(uint8_t i = 0; i < node->num_element; i++)LIBCXML_DBG("%s\r\n", node->element[i].value);
	LIBCXML_DBG("***********************************************\r\n");
	if(node->first_child != NULL){
		LIBCXML_DBG("first child\r\n");
		libCxml_print(node->first_child);
	}
	if(node->next_child != NULL){
		LIBCXML_DBG("next child\r\n");
		libCxml_print(node->next_child);
	}
}

libCxml_document_t* libCxml_parse_document(const char *path)
{
	char* buffer = NULL;
	int8_t res_parse_hdr = -1;
	libCxml_token_data_t token_data = {0};
	libCxml_document_t *document = NULL;
	libCxml_result result = LIBCXML_OK;
	document = pvPortCalloc(1, sizeof(libCxml_document_t));
	
	if(libCxml_fopen(path, LIBCXML_READ) == LIBCXML_OK){
		buffer = pvPortMalloc((MAX_SIZE_DOCUMENT * sizeof(uint8_t) + 1));
		if(buffer != NULL){
			res_parse_hdr = libCxml_search_hdr(document, buffer);
			if(res_parse_hdr == -1) goto PARSE_ERROR;
			else if(!res_parse_hdr){
				memcpy(document->version, "1.0", 3);
				memcpy(document->encoding, "UTF-8", 5);
			}
			
			document->nodes = libCxml_create_list(buffer, &token_data, NULL, &result);
			if(result != LIBCXML_OK) goto PARSE_ERROR;
			//libCxml_print(document->nodes);
			vPortFree(buffer);
		}
		libCxml_fclose();
	}
	else document->no_file = true;
	
	return document;
	
PARSE_ERROR:
	LIBCXML_DBG("\"%s\" parsing error\r\n", path);
	libCxml_fclose();
	libCxml_free_document(document);
	vPortFree(buffer);
	
	return document;
}

libCxml_node_t *libCxml_FirstChildNode(libCxml_node_t *node)
{
	if(node == NULL) return NULL;
	if(node->first_child == NULL) return NULL;
	if(node->first_child->element[0].type == LIBCXML_IS_TAG)return node->first_child;

	return NULL;
}

libCxml_node_t *libCxml_NextSiblingNode(libCxml_node_t *node)
{
	if(node == NULL) return NULL;
	if(node->next_child == NULL) return NULL;
	if(node->next_child->element[0].type == LIBCXML_IS_TAG)return node->next_child;
	
	return NULL;
}

libCxml_node_t *libCxml_GetRootNode(libCxml_node_t *node)
{
	return node;
}

libCxml_node_t *libCxml_GetChildNode(libCxml_node_t *node, const char *str)
{
	if(node == NULL) return NULL;
	libCxml_node_t *child = node->first_child;

	while(child != NULL){
		for(uint16_t i = 0; i < child->num_element; i++){
			if(child->element[i].type == LIBCXML_IS_TAG){
				if(memcmp(child->element[i].value, str, (strlen(child->element[i].value) > strlen(str) ? strlen(child->element[i].value) : strlen(str))) == 0) return child;
				break;
			}
		}
		child = child->next_child;
	}
	
	return NULL;
}

char *libCxml_getTagValue(libCxml_node_t *node)
{
	if(node == NULL) return NULL;
	for(uint16_t i = 0; i < node->num_element; i++){
		if(node->element[i].type == LIBCXML_IS_TAG_VAL) return node->element[i].value;
	}
	
	return NULL;
}

char *libCxml_getAttributeValue(libCxml_node_t *node, const char *attr)
{
	if(node == NULL) return NULL;
	for(uint16_t i = 0; i < node->num_element; i++){
		if(node->element[i].type == LIBCXML_IS_ATTR) {
			if(memcmp(node->element[i].value, attr, (strlen(node->element[i].value) > strlen(attr) ? strlen(node->element[i].value) : strlen(attr))) == 0) {
				if(node->element[i + 1].type == LIBCXML_IS_ATTR_VAL)return node->element[i + 1].value;
			}
		}
	}
	
	return NULL;
}

void libCxml_split_free(libCxml_split_arr_typedef *arr)
{
	if(arr->arr != NULL) vPortFree(arr->arr);
	if(arr != NULL) vPortFree(arr);
}

libCxml_split_arr_typedef *libCxml_split_int(const char *str)
{
	if(str == NULL) return NULL;
	char *end = (char*)str;
	libCxml_split_arr_typedef *arr;
	
	arr = pvPortCalloc(1, sizeof(libCxml_split_arr_typedef));
	if(arr == NULL) return NULL;	
	while(*str != '\0') {
		if(*str == ',') arr->lenght++;
		str++;
	}
	arr->lenght++;
	arr->arr = pvPortCalloc(arr->lenght, sizeof(int));
	if(arr->arr != NULL){
		uint8_t i = 0;
		while(i != arr->lenght) {
			arr->arr[i++] = strtol(end, &end, 10);
			end++;
		}
		return arr;
	}
	else libCxml_split_free(arr);
	
	return NULL;
}
