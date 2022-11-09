#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dberror.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "record_mgr.h"

int MIN_S = 5;

// funcs - MINE
RC initTable(SM_FileHandle fh, SM_PageHandle ph, Schema *schema);
RC initHeader(SM_PageHandle ph, Schema *schema);
RC checkHeaderSize(int min_s, Schema *schema);


//Helper Function definitions - not MINE
RC initRecordPage(int pageSize,int recordSize,int pageNo, RM_RecordPage * rp, SM_PageHandle ph);
RC calcOffset (Schema *schema, int attrNum, int *result);
int findFreePage(RM_TableData *rel,int pageStart);
void clear(char * s,int bytes);
void writeToHeader(RM_TableData *rel);
char * writeInt(char * start, int value, int bytes);
char * locateRecord(RM_TableData *rel, RID id);
char * getIntFromString(char * start, int * value, int size);



RC checkHeaderSize(int min_s, Schema *schema){
  // num record, num attr, keysize, datatype, type length, key attributes
  int numRecordPagesBytes = min_s * sizeof(char);
  int numAttrBytes = min_s * sizeof(char);
  int keySizeBytes = min_s * sizeof(char);
  int dataTypeBytes = min_s * schema->numAttr * sizeof(char);
  int typeLengthBytes = min_s * schema->numAttr * sizeof(char);
  int keyAttrBytes = min_s * schema->keySize * sizeof(char);

  int attLengthTotalBytes = 0;
  //Find bytes of attribute names
  for(int i = 0; i < schema -> numAttr; i++){
    int l = strlen(schema -> attrNames[i]) + 1;
    attLengthTotalBytes += (l * sizeof(char)); 
  }
    
  int length = numRecordPagesBytes + numAttrBytes + keySizeBytes + typeLengthBytes + keyAttrBytes + dataTypeBytes + attLengthTotalBytes;
  if(length>PAGE_SIZE)
  {
    return 33; // table fail -- too large?
  }
}

RC initTable(SM_FileHandle fh, SM_PageHandle ph, Schema *schema){
  ph = malloc(PAGE_SIZE);
  memset(ph, '\0', PAGE_SIZE);

  if(initHeader(ph, schema) != RC_OK){
    return 33;
  }

  writeBlock(0, &fh, ph);
  closePageFile(&fh);
  free(ph);
  
  return RC_OK;
}

RC initHeader(SM_PageHandle ph, Schema *schema){
  if(checkHeaderSize(MIN_S, schema) != RC_OK){
    return 33;
  }
  
  char * start;
  start = ph;
  //Write number of records
  start = writeInt(start,0, MIN_S);
  //Write number of attributes
  start = writeInt(start,schema->numAttr,MIN_S);
  //Write key size
  start = writeInt(start,schema->keySize,MIN_S);

  //Write data type
  for(int x = 0; x<schema->numAttr; x++){
    start = writeInt(start,schema->dataTypes[x],MIN_S);
  }
  //Write type length
  for(int x = 0; x<schema->numAttr; x++){
     start = writeInt(start,schema->typeLength[x],MIN_S);
  }
  //Write keys
  for(int x = 0; x<schema->keySize; x++){
    start = writeInt(start,schema->keyAttrs[x],MIN_S);
  }
  //Write attribute names
  for(int x = 0; x<schema->numAttr; x++){
    int length = strlen(schema->attrNames[x]);
    sprintf(start,"%s",schema->attrNames[x]); 
    start+=length+1; 
  }

  return RC_OK;
}

// table and manager

RC initRecordManager (void *mgmtData){
    return RC_OK;
}

RC shutdownRecordManager (){
    return RC_OK;
}

RC createTable (char *name, Schema *schema){
    if(createPageFile(name) != RC_OK){
      return 33;
    }

    SM_FileHandle fh;
    SM_PageHandle ph;
    openPageFile(name, &fh);
    if(initTable(fh, ph, schema) != RC_OK){
      return 33;
    }

    return RC_OK;
}











RC readHeader(SM_PageHandle pg, Schema * schema,int * numRecordPages){
  //Find number of record pages
  char * start = pg;
  int val;
  start = getIntFromString(start,&val,MIN_S);
  *numRecordPages = val;
  
  start = getIntFromString(start,&val,MIN_S);
  schema->numAttr = val; 
  
  start = getIntFromString(start,&val,MIN_S);
  schema->keySize = val;
  
  schema->dataTypes = malloc(sizeof(int)*schema->numAttr);
  schema->typeLength = malloc(sizeof(int)*schema->numAttr);
  schema->keyAttrs = malloc(sizeof(int)*schema->keySize);
  schema->attrNames = malloc(sizeof(char*)*schema->numAttr);
  
  for(int x = 0; x<schema->numAttr; x++){
    start = getIntFromString(start,&val,MIN_S);
    schema->dataTypes[x]=val;
  }
  
  for(int x = 0; x<schema->numAttr; x++){
    start = getIntFromString(start,&val,MIN_S);
    schema->typeLength[x]=val;
  }
  
  for(int x = 0; x<schema->keySize; x++){
    start = getIntFromString(start,&val,MIN_S);
    schema->keyAttrs[x]=val;
  }

  for(int x = 0; x<schema->numAttr; x++){
    schema->attrNames[x] = malloc(sizeof(char)*(strlen(start)+1));
    strcpy(schema->attrNames[x],start);
    start+=(strlen(schema->attrNames[x])+1)*sizeof(char); 
  }
  return RC_OK;
}

char * getIntFromString(char * start, int * value, int size)
{
  *value = atoi(start);
  start+=size*sizeof(char);
  return start;
}
  

RC openTable(RM_TableData *rel, char *name)
{
  rel->name = name;
  BM_BufferPool * bm = MAKE_POOL();
  BM_PageHandle * h  = MAKE_PAGE_HANDLE();
  RC status = initBufferPool(bm,name,10,RS_FIFO,NULL);
  if(status!=RC_OK){
    printf("ERROR: Table could not be opened!\n");
    return status;
  }
  int pageStart = 0;
  //Get number of record pages from buffer pool
  pinPage(bm,h,0);
  SM_PageHandle headerData = h->data;
  int numRecordPages;
  Schema * schema = malloc(sizeof(Schema));
  //Read header data into schema and numRecordPages
  readHeader(headerData,schema,&numRecordPages);
  //Assigning rel its schema
  rel->schema = schema;
  unpinPage(bm,h);
  RM_PageDirectory *pd = malloc(sizeof(RM_PageDirectory));
  pd->numRecordPages = numRecordPages;
  int *pageFullArray = malloc(sizeof(int)*numRecordPages);
  //Calculate where record pages start
  pageStart = 1;//To start
  RM_RecordPage **recordPageList = malloc(sizeof(RM_RecordPage*)*numRecordPages);
  int totalTuples = 0;
  for(int x = 0; x<numRecordPages; x++){
    pinPage(bm,h,x+pageStart);
    recordPageList[x] = malloc(sizeof(RM_RecordPage));
    if(initRecordPage(PAGE_SIZE,getRecordSize(rel->schema)+sizeof(char),x+pageStart,recordPageList[x],h->data)!=RC_OK){ //+sizeof(char) is for additional status char
      return 33; // table fail
    }
    totalTuples+=recordPageList[x]->numTuples;
    if(recordPageList[x]->freeSpace>=getRecordSize(rel->schema)){
      pageFullArray[x]=0;
    }
    else{
      pageFullArray[x]=1;
    }
    unpinPage(bm,h);
  }
  //Initialize page directory
  pd->totalTuples = totalTuples;
  pd->recPageArray = recordPageList;
  pd->pageFullArray = pageFullArray;
  pd->bp = bm;
  rel->mgmtData = pd;
  free(h); 

  return RC_OK;
}

RC initRecordPage(int pageSize,int recordSize,int pageNo, RM_RecordPage * rp, SM_PageHandle ph){ 
  int tuples = atoi(ph); 
  rp->numTuples = tuples;
  rp->recSize = recordSize;
  rp->pageNo = pageNo;
  rp->freeSpace = pageSize-(recordSize*tuples)-(sizeof(char)*MIN_S);
  if(rp->freeSpace<0){
    return 33; // table fail
  }
  return RC_OK;
}

RC closeTable(RM_TableData *rel)
{
  RM_PageDirectory * pd = rel->mgmtData;
  writeToHeader(rel);
  shutdownBufferPool(pd->bp);
  RM_RecordPage **rp = pd->recPageArray;
  //Free record pages
  for(int x = 0; x<pd->numRecordPages; x++){
    free(rp[x]);
  }
  free(rp);
  //Free page directory contents
  free(pd->pageFullArray);
  free(pd->bp);
  //Free page directory
  free(pd);
  //Free Schema
  freeSchema(rel->schema);
  return RC_OK;
}

void writeToHeader(RM_TableData *rel){
  //Assuming schema cannot change
  RM_PageDirectory * pd = rel->mgmtData;
  BM_PageHandle * h  = MAKE_PAGE_HANDLE();
  pinPage(pd->bp,h,0); //Pin header page
  writeInt(h->data, pd->numRecordPages, MIN_S*sizeof(char));
  markDirty(pd->bp,h);
  unpinPage(pd->bp,h);
  free(h);
}

RC deleteTable(char *name)
{
  destroyPageFile(name);
  return RC_OK;
}

int getNumTuples(RM_TableData *rel)
{
  RM_PageDirectory * pd = rel->mgmtData;
  return pd->totalTuples;
}

//Handling Records in the Table
RC insertRecord(RM_TableData *rel, Record *record)
{
  //Get page directory struct from table data
  RM_PageDirectory * pd = rel->mgmtData;
  //Get the buffer pool for the pagefile
  BM_BufferPool * bp = pd->bp;
  //Create a page handle that will handle the pages from the file
  BM_PageHandle * h  = MAKE_PAGE_HANDLE();
  //find page to insert record
  int freePage = findFreePage(rel,0);
  //If there are no free pages, create a new page and reallocate the data in page directory to accomodate additional page
  if(freePage==-1){
    //Increase number of record pages in file header
    pd->numRecordPages++;
    pinPage(bp,h,0); //0 because there is only 1 header page
    writeInt(h->data,pd->numRecordPages,MIN_S*sizeof(char));
    markDirty(bp,h);
    unpinPage(bp,h);
    //Pin new page 
    pinPage(bp,h,pd->numRecordPages); //This only works if the header is the 0th page only
    //Clear the page because it is currently filled with all \0 chars
    clear(h->data,PAGE_SIZE);
    //Re-allocate memory for increased size
    void * tmp1;
    void * tmp2;
    tmp1 = realloc(pd->pageFullArray,sizeof(int)*(pd->numRecordPages));
    tmp2 = realloc(pd->recPageArray,sizeof(RM_RecordPage*)*(pd->numRecordPages));
    if(tmp1==NULL || tmp2 == NULL){
      printf("ERROR: Memory could not be reallocated\n");
      return 33; // insertion fail 
    }
    pd->pageFullArray = tmp1;
    pd->recPageArray = tmp2;
    //Set new page to not be full 
    pd->pageFullArray[pd->numRecordPages-1] = 0;
    writeInt(h->data,0, MIN_S*sizeof(char)); //Write 0 for # of tuplesinto record page header
    markDirty(bp,h);
    int rSize = getRecordSize(rel->schema)+sizeof(char); //Additional sizeof(char) for status char
    pd->recPageArray[pd->numRecordPages-1] = malloc(sizeof(RM_RecordPage));
    initRecordPage(PAGE_SIZE,rSize,pd->numRecordPages-1,pd->recPageArray[pd->numRecordPages-1],h->data);
    freePage = pd->numRecordPages-1;
    unpinPage(bp,h);
  }
  
  //Load free page
  RM_RecordPage *rp;
  rp = pd->recPageArray[freePage];
  pinPage(bp,h,freePage+1); //+1 is because the first page of the file is the header page
  RID newId;
  newId.page = freePage; 
  //Update numOfTuples
  rp->numTuples++;
  writeInt(h->data,rp->numTuples,MIN_S*sizeof(char));
  //Create a pointer that points to the first status char of the record page
  char * start = h->data+MIN_S*sizeof(char);
  int startBytes = MIN_S*sizeof(char);
  int slotCount = 0;
  char firstElement;
  while(startBytes<PAGE_SIZE){
    firstElement = start[0];
    //If the current record slot is \0, then an empty spot for the record has been found
   if(firstElement!='O'){
      memset(start,'O',sizeof(char));
      memcpy(start+sizeof(char),record->data,getRecordSize(rel->schema));
      //assign slot # to inserted record
      newId.slot = slotCount;
      break;
    }
    //Skip to next record spot
    start+=getRecordSize(rel->schema)+sizeof(char);
    startBytes += getRecordSize(rel->schema)*sizeof(char)+sizeof(char);
    slotCount++;
  }
  record->id = newId;
  pd->totalTuples++;
  markDirty(bp,h);
  unpinPage(bp,h);
  rp->freeSpace-=(getRecordSize(rel->schema)+sizeof(char));
  //Update pageFullArray if necessary
  if(rp->freeSpace<(getRecordSize(rel->schema)+sizeof(char))){
    pd->pageFullArray[freePage] = 1;
  }
  free(h);
  return RC_OK;
}

char * writeInt(char * start, int value, int bytes){
  clear(start,bytes);
  sprintf(start,"%d",value);
  return start+bytes;
}

int findFreePage(RM_TableData *rel,int pageStart)
{
  RM_PageDirectory * pd = rel->mgmtData;
  for(int x = pageStart; x<pd->numRecordPages; x++){
    if(pd->pageFullArray[x]==0){
      return x;
    }
  }
  return -1;
}

void clear(char * s,int bytes){
  memset(s,0,bytes);
}
    
RC deleteRecord(RM_TableData *rel, RID id)
{
  //Get page directory struct from table data
  RM_PageDirectory * pd = rel->mgmtData;
  //Get the buffer pool for the pagefile
  BM_BufferPool * bp = pd->bp;
  //Create a page handle that will handle the pages from the file
  BM_PageHandle * h  = MAKE_PAGE_HANDLE();
  pinPage(bp,h,id.page+1); //+1 is because header page is page 0
  //Get record page that contains record to delete
  RM_RecordPage *rp = pd->recPageArray[id.page];
  //get pointer to the record to be deleted
  char * start = locateRecord(rel,id);
  //Verify record exists
  if(start[0]!='O'){
    return 33; // no record exists
  }
  clear(start,getRecordSize(rel->schema)*sizeof(char)+sizeof(char));
  //Update record page
  rp->numTuples--;
  writeInt(h->data,rp->numTuples,MIN_S*sizeof(char));
  markDirty(bp,h);
  rp->freeSpace+=getRecordSize(rel->schema)*sizeof(char)+sizeof(char);
  //Because a full record has been deleted, there is now room for another record
  pd->pageFullArray[id.page] = 0;
  //Decrease tuples in page directory 
  pd->totalTuples--;
  unpinPage(bp,h);
  free(h); 
  return RC_OK;
}

RC updateRecord(RM_TableData *rel, Record *record)
{
  //Get page directory struct from table data
  RM_PageDirectory * pd = rel->mgmtData;
  //Get the buffer pool for the pagefile
  BM_BufferPool * bp = pd->bp;
  //Create a page handle that will handle the pages from the file
  BM_PageHandle * h  = MAKE_PAGE_HANDLE();
  pinPage(bp,h,record->id.page+1); //+1 is because header page is page 0
  //Locate and clear record to be updated
  char * start;
  start = locateRecord(rel,record->id);
  if(start[0]!='O'){
    return 33; // no record exists
  }
  clear(start,getRecordSize(rel->schema)*sizeof(char)+sizeof(char));
  //Update record
  memset(start,'O',sizeof(char));
  memcpy(start+sizeof(char),record->data,getRecordSize(rel->schema)*sizeof(char));
  markDirty(bp,h);
  unpinPage(bp,h);
  free(h);
  return RC_OK;
}

RC getRecord(RM_TableData *rel, RID id, Record *record)
{
  //Locate record
  char * start;
  start = locateRecord(rel,id);
  if(start[0]!='O'){
    return 33; // no record exists
  }
  record->id = id;
  memcpy(record->data,start+sizeof(char),getRecordSize(rel->schema));
  strcat(record->data,"\0"); //Append null byte to end
  return RC_OK;
}

//Returns pointer to the start of record identified by RID
char * locateRecord(RM_TableData *rel, RID id){
  //Get page directory struct from table data
  RM_PageDirectory * pd = rel->mgmtData;
  //Get the buffer pool for the pagefile
  BM_BufferPool * bp = pd->bp;
  //Create a page handle that will handle the pages from the file
  BM_PageHandle * h  = MAKE_PAGE_HANDLE();
  pinPage(bp,h,id.page+1); //+1 is because header page is page 0
  char * location = h->data+MIN_S*sizeof(char)+id.slot*(getRecordSize(rel->schema)+sizeof(char));
  unpinPage(bp,h);
  free(h);
  return location;
}
  

//Scans
RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{
  RM_PageDirectory *pd = rel->mgmtData;
  if(pd->numRecordPages==0){
    printf("ERROR: No records to scan\n");
    return RC_RM_NO_MORE_TUPLES; 
  }
  RM_ScanHelper *sh;
  //Initialize scan helper
  sh = malloc(sizeof(RM_ScanHelper));
  sh->totalRecordPages = pd->numRecordPages;
  sh->currentTuple = 0;
  sh->currentPage = 0;
  sh->condition = cond;
  //Initialize scan handler
  scan->rel = rel;
  scan->mgmtData = sh;
  return RC_OK;
}

RC next(RM_ScanHandle *scan, Record *record)
{
  RM_TableData * rel = scan->rel;
  RM_PageDirectory * pd = rel->mgmtData;
  RM_ScanHelper * sh = scan->mgmtData;
  RID currentID;
  RM_RecordPage *currentRecordPage;

  Value *val;
  while(sh->currentPage<sh->totalRecordPages){
    currentID.page = sh->currentPage;
    currentRecordPage = pd->recPageArray[sh->currentPage];
    while(sh->currentTuple<currentRecordPage->numTuples){
      currentID.slot = sh->currentTuple;
      if(getRecord(rel,currentID,record)==RC_OK){
        if(sh->condition==NULL){
          sh->currentTuple++;
          return RC_OK;
        }
        //Verify if expression can be evaluated
        RC check = evalExpr(record,rel->schema,sh->condition,&val);
        if(check!=RC_OK){
          return check;
        }
        if(val->v.boolV==true){
          sh->currentTuple++;
          return RC_OK;
        }
      }
      sh->currentTuple++;
    }
    sh->currentTuple = 0; //Reset tuple
    sh->currentPage++;
  } 
  return RC_RM_NO_MORE_TUPLES;
}

RC closeScan(RM_ScanHandle *scan)
{
  free(scan->mgmtData);
  return RC_OK;
}

//Dealing with Schemas
int getRecordSize(Schema *schema)
{
  int size = 0, i;

  for (i = 0; i < schema->numAttr; ++i)
  {
    switch (schema->dataTypes[i])
    {
    case DT_INT:
      size += sizeof(int);
      break;
    case DT_FLOAT:
      size += sizeof(float);
      break;
    case DT_BOOL:
      size += sizeof(bool);
      break;
    case DT_STRING:
      size += schema->typeLength[i]*sizeof(char); 
      break;
    default:
      break;
    }
  }
  return size;
}

Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys)
{
  Schema *schema = (Schema *)malloc(sizeof(Schema));

  schema->numAttr = numAttr;
  schema->attrNames = attrNames;
  schema->dataTypes = dataTypes;
  schema->typeLength = typeLength;
  schema->keySize = keySize;
  schema->keyAttrs = keys;

  return schema;
}

RC freeSchema(Schema *schema)
{
  free(schema->dataTypes);
  free(schema->typeLength);
  free(schema->keyAttrs);
  for(int i = 0; i<schema->numAttr; i++){
    free(schema->attrNames[i]);
  }
  free(schema->attrNames);
  free(schema);
  return RC_OK;
}

//Dealing with records and attribute values
RC createRecord(Record **record, Schema *schema){

  int recSize = getRecordSize(schema);
  *record = malloc(sizeof(Record));
  (*record)->data = malloc(recSize*sizeof(char));
  (*record)->id.page = -1;
  (*record)->id.slot = -1;
  return RC_OK;
}

RC freeRecord(Record *record){
  free(record->data);
  free(record);
  return RC_OK; 
}

RC getAttr(Record *record, Schema *schema, int attrNum, Value **value){
  
  int attOffset=0;
  calcOffset(schema,attrNum,&attOffset);
  char * start = (record->data) +attOffset;
  int numBytes = schema->typeLength[attrNum];

  *value = malloc(sizeof(Value));
  (*value)->dt = schema->dataTypes[attrNum];

  if((*value)->dt == DT_INT){
    memcpy(&((*value)->v.intV),start,(numBytes+1)*sizeof(int));
  }
  else if((*value)->dt == DT_STRING){
    (*value)->v.stringV = malloc((numBytes+1)*sizeof(char));
    memcpy((*value)->v.stringV,start,(numBytes+1)*sizeof(char));
    (*value)->v.stringV[numBytes] = '\0';
  }
  else if((*value)->dt == DT_FLOAT){
    memcpy(&((*value)->v.floatV),start,(numBytes+1)*sizeof(float));
  }
  else if((*value)->dt== DT_BOOL){
    memcpy(&((*value)->v.boolV),start,(numBytes+1)*sizeof(bool));
  }
  else{
    printf("ERROR: NOT AN ACCEPTED TYPE\n");
    return RC_RM_UNKOWN_DATATYPE;
  }
 
  return RC_OK;
}

RC setAttr(Record *record, Schema *schema, int attrNum, Value *value){
  
  int attOffset = 0;
  calcOffset(schema,attrNum,&attOffset);
  char * start = (record->data) +attOffset;
  int numBytes = schema->typeLength[attrNum];

  if(value->dt == DT_INT){
    memcpy(start,&(value->v.intV),(numBytes+1)*sizeof(int)); 
  }
  else if(value->dt == DT_STRING){
    memcpy(start,value->v.stringV,(numBytes)*sizeof(char));
  }
  else if(value->dt == DT_FLOAT){
    memcpy(start,&(value->v.floatV),(numBytes+1)*sizeof(float));
  }
  else if(value->dt == DT_BOOL){
    memcpy(start,&(value->v.boolV),(numBytes+1)*sizeof(bool));
  }
  else{
    printf("ERROR: NOT AN ACCEPTED TYPE\n");
    return RC_RM_UNKOWN_DATATYPE;
  }
    
  return RC_OK;
}

RC calcOffset (Schema *schema, int attrNum, int *result){
	int offset = 0;
	int attrPos = 0;

	for(attrPos = 0; attrPos < attrNum; attrPos++)
		switch (schema->dataTypes[attrPos])
		{
		case DT_STRING:
			offset += schema->typeLength[attrPos];
			break;
		case DT_INT:
			offset += sizeof(int);
			break;
		case DT_FLOAT:
			offset += sizeof(float);
			break;
		case DT_BOOL:
			offset += sizeof(bool);
			break;
		}

	*result = offset;
	return RC_OK;
}
