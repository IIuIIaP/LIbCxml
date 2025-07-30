# LIbCxml
XML Parsing Library for Embedded Systems

## example:

### instantiation
```
libCxml_document_t *xml_doc = NULL;
```
### parsing document
```	
xml_doc = libCxml_parse_document("/usr/config.xml");
if((xml_doc->nodes == NULL) && (!xml_doc->no_file)) __logWrite("\"/usr/config.xml\" parsing error! Setting default configuration.\n");
``` 
### reading
```
libCxml_node_t *node_network = libCxml_GetChildNode(xml_doc->nodes, "net");
if(node_network != NULL){
	libCxml_node_t *node = libCxml_GetChildNode(node_network, "ip");
	if(node != NULL){
		char *paddr = libCxml_getTagValue(node);
		if(paddr != NULL)memcpy(saddr, paddr, strlen(paddr));
	}
}
```
### reading in a loop
```
libCxml_node_t *node_dsl = libCxml_GetChildNode(xml_doc->nodes, "dsl");
if(node_dsl != NULL){
	dEFM.config.mode = atoi(libCxml_getAttributeValue(node_dsl, "mode"));
	uint8_t id = 0;
	char *str = NULL;
	libCxml_node_t *node_line = libCxml_FirstChildNode(node_dsl);
	while(node_line != NULL){
		id = atoi(libCxml_getAttributeValue(node_line, "id"));
		str = libCxml_getTagValue(node_line);
		libCxml_split_arr_typedef *arr = libCxml_split_int(str);
		if(arr != NULL){
			dEFM.config.is_blocked[id] = arr->arr[0];
			dEFM.config.is_mask[id] = arr->arr[1];
			dEFM.config.tcpam[id] = arr->arr[2];
			dEFM.config.rate[id] = arr->arr[3];
			dEFM.config.wetting_current[id] = arr->arr[4];
			dEFM.config.line_probe_enable[id] = arr->arr[5];
			dEFM.config.cc_snr_threshold[id] = arr->arr[6];
			dEFM.config.extPMMS_mask[id] = arr->arr[7];
			dEFM.config.extPMMS_max_rate[id] = arr->arr[8];
			efm.flags.config_recerved[id] = true;
			libCxml_split_free(arr);
		}
		node_line = libCxml_NextSiblingNode(node_line);		
	}
}
```
