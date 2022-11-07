#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "record_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"

typedef struct TableManager {
    BM_PageHandle pageHandle;
    BM_BufferPool bufferPool;
    int tuplesCountNumber;
    int freePagePointer;
} TableManager;

typedef struct ScanManager {
    BM_PageHandle pageHandle;
    Expr *condition;
    RID recordID;
    int scanCountNumber;
} ScanManager;

const int MAX_NUMBER_OF_PAGES = 100;
const int ATTRIBUTE_SIZE = 15;

TableManager *tableManager;

//this function is created by us to find a free slot in a page
int findFreeSlot(char *data, int recordSize) {
    int i, totalSlots = PAGE_SIZE / recordSize;
    for (i = 0; i < totalSlots; i++)
        if (data[i * recordSize] != '$')
            return i;
    return -1;
}

//initialize the record manager
extern RC initRecordManager(void *mgmtData) {
    initStorageManager();
    return RC_OK;
}

//shut down the record manager
extern RC shutdownRecordManager() {
    tableManager = NULL;
    free(tableManager);
    return RC_OK;
}

//create a table and initialize the info page, store the info to the information pages
extern RC createTable(char *name, Schema *schema) {
    tableManager = (TableManager *) malloc(sizeof(TableManager));
    //initialize the bufferPool using LRU strategy
    initBufferPool(&tableManager->bufferPool, name, MAX_NUMBER_OF_PAGES, RS_LRU, NULL);
    char data[PAGE_SIZE];
    char *pageHandle = data;
    //store the info
    for (int k = 0; k < schema->numAttr; k++) {
        if (k == 0) {
            *(int *) pageHandle = -1;
            pageHandle = pageHandle + sizeof(int);
            *(int *) pageHandle = 1;
            pageHandle = pageHandle + sizeof(int);
            *(int *) pageHandle = schema->numAttr;
            pageHandle = pageHandle + sizeof(int);
            *(int *) pageHandle = schema->keySize;
            pageHandle = pageHandle + sizeof(int);
        }
        char *str = schema->attrNames[k];
        strncpy(pageHandle, str, ATTRIBUTE_SIZE);
        pageHandle = pageHandle + ATTRIBUTE_SIZE;
        *(int *) pageHandle = (int) schema->dataTypes[k];
        pageHandle = pageHandle + sizeof(int);
        *(int *) pageHandle = (int) schema->typeLength[k];
        pageHandle = pageHandle + sizeof(int);

    }
    SM_FileHandle fileHandle;
    createPageFile(name);
    openPageFile(name, &fileHandle);
    writeBlock(0, &fileHandle, data);
    closePageFile(&fileHandle);
    return RC_OK;
}

//open the table
extern RC openTable(RM_TableData *rel, char *name) {
    //store the entry of the buffer manager
    rel->mgmtData = tableManager;
    //set the table structure
    rel->name = name;
    //pin the page
    pinPage(&tableManager->bufferPool, &tableManager->pageHandle, 0);
    char *pointer = tableManager->pageHandle.data;
    //get the info
    tableManager->tuplesCountNumber = *(int *) pointer;
    pointer += sizeof(int);
    tableManager->freePagePointer = *(int *) pointer;
    pointer += sizeof(int);
    int attributeCount = *(int *) pointer;
    pointer += sizeof(int);
    Schema *schema = (Schema *) malloc(sizeof(Schema));
    //set the schema's parameters
    schema->attrNames = (char **) malloc(sizeof(char *) * attributeCount);
    schema->numAttr = attributeCount;
    schema->dataTypes = (DataType *) malloc(sizeof(DataType) * attributeCount);
    schema->typeLength = (int *) malloc(sizeof(int) * attributeCount);
    for (int k = 0; k < schema->numAttr; k++) {
        schema->attrNames[k] = (char *) malloc(ATTRIBUTE_SIZE);
        strncpy(schema->attrNames[k], pointer, ATTRIBUTE_SIZE);
        pointer += ATTRIBUTE_SIZE;
        schema->dataTypes[k] = *(int *) pointer;
        pointer += sizeof(int);
        schema->typeLength[k] = *(int *) pointer;
        pointer += sizeof(int);
    }
    unpinPage(&tableManager->bufferPool, &tableManager->pageHandle);
    //write the page in the buffer pool to the disk
    forcePage(&tableManager->bufferPool, &tableManager->pageHandle);
    rel->schema = schema;
    return RC_OK;
}

//close the table using the entry rel
extern RC closeTable(RM_TableData *rel) {
    TableManager *tableManager = rel->mgmtData;
    shutdownBufferPool(&tableManager->bufferPool);
    return RC_OK;
}

//delete a table from memory
extern RC deleteTable(char *name) {
    destroyPageFile(name);
    return RC_OK;
}

//get the record length stored in info page
extern int getNumTuples(RM_TableData *rel) {
    TableManager *tableManager = rel->mgmtData;
    return tableManager->tuplesCountNumber;
}

//insert record
extern RC insertRecord(RM_TableData *rel, Record *record) {

    TableManager *tableManager = rel->mgmtData;
    //get and set the record info
    RID *recordID = &record->id;
    int recordSize = getRecordSize(rel->schema);
    recordID->page = tableManager->freePagePointer;
    pinPage(&tableManager->bufferPool, &tableManager->pageHandle, recordID->page);
    char *data = tableManager->pageHandle.data;
    //if the pinned page has no space, then just use the next page
    while (findFreeSlot(data, recordSize) == -1) {
        unpinPage(&tableManager->bufferPool, &tableManager->pageHandle);
        recordID->page++;
        pinPage(&tableManager->bufferPool, &tableManager->pageHandle, recordID->page);
        data = tableManager->pageHandle.data;
    }
    //search the free slot in the page to store the record
    recordID->slot = findFreeSlot(data, recordSize);
    //mark that this page has been changed
    markDirty(&tableManager->bufferPool, &tableManager->pageHandle);
    char *slotPointer = data + (recordID->slot * recordSize);
    *slotPointer = '$';
    slotPointer += 1;
    //copy the record's data
    memcpy(slotPointer, record->data + 1, recordSize - 1);
    unpinPage(&tableManager->bufferPool, &tableManager->pageHandle);
    tableManager->tuplesCountNumber += 1;
    pinPage(&tableManager->bufferPool, &tableManager->pageHandle, 0);
    return RC_OK;
}

//delete a record
extern RC deleteRecord(RM_TableData *rel, RID id) {
    TableManager *tableManager = rel->mgmtData;
    //pin the page that we will delete the record on
    pinPage(&tableManager->bufferPool, &tableManager->pageHandle, id.page);
    //get and set the info of the page
    tableManager->freePagePointer = id.page;
    char *data = tableManager->pageHandle.data;
    data = data + (id.slot * getRecordSize(rel->schema));
    //'#' denotes that the record is deleted
    *data = '#';
    markDirty(&tableManager->bufferPool, &tableManager->pageHandle);
    unpinPage(&tableManager->bufferPool, &tableManager->pageHandle);
    return RC_OK;
}

//update a record
extern RC updateRecord(RM_TableData *rel, Record *record) {
    TableManager *tableManager = rel->mgmtData;
    pinPage(&tableManager->bufferPool, &tableManager->pageHandle, record->id.page);
    //get and set the info of the record
    RID id = record->id;
    char *data = tableManager->pageHandle.data + (id.slot * getRecordSize(rel->schema));
    *data = '$';
    data += 1;
    memcpy(data, record->data + 1, getRecordSize(rel->schema) - 1);
    //mark the page has been changed
    markDirty(&tableManager->bufferPool, &tableManager->pageHandle);
    unpinPage(&tableManager->bufferPool, &tableManager->pageHandle);
    return RC_OK;
}

//get a record from certain table with certain id
extern RC getRecord(RM_TableData *rel, RID id, Record *record) {
    TableManager *tableManager = rel->mgmtData;
    pinPage(&tableManager->bufferPool, &tableManager->pageHandle, id.page);
    char *pointer = tableManager->pageHandle.data + (id.slot * getRecordSize(rel->schema));
    //record found
    if (*pointer == '$') {
        //set the info of the record
        record->id = id;
        char *data = record->data;
        data += 1;
        //copy the data
        memcpy(data, pointer + 1, getRecordSize(rel->schema) - 1);
    }
    unpinPage(&tableManager->bufferPool, &tableManager->pageHandle);
    return RC_OK;
}

//initialize a scanner with a given table and condition
extern RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond) {
    //open the table
    openTable(rel, "ScanTable");
    ScanManager *scanManager = (ScanManager *) malloc(sizeof(ScanManager));
    scan->mgmtData = scanManager;
    //set the parameters
    scanManager->recordID.slot = 0;
    scanManager->recordID.page = 1;
    scanManager->condition = cond;
    scanManager->scanCountNumber = 0;
    TableManager *tableManager = rel->mgmtData;
    tableManager->tuplesCountNumber = ATTRIBUTE_SIZE;
    scan->rel = rel;
    return RC_OK;
}

//return the next eligible record
extern RC next(RM_ScanHandle *scan, Record *record) {
    int recordSize, totalSlots, scanCount;
    Schema *schema;
    //get all the info
    schema = scan->rel->schema;
    recordSize = getRecordSize(schema);
    totalSlots = PAGE_SIZE / recordSize;
    TableManager *tableManager;
    tableManager = scan->rel->mgmtData;
    ScanManager *scanManager;
    scanManager = scan->mgmtData;
    Value *result;
    result = (Value *) malloc(sizeof(Value));
    for (scanCount = scanManager->scanCountNumber; scanCount <= tableManager->tuplesCountNumber; scanCount++) {
        //if the scan hasn't been completed
        if (scanCount > 0) {
            scanManager->recordID.slot++;
            if (scanManager->recordID.slot >= totalSlots) {
                scanManager->recordID.slot = 0;
                scanManager->recordID.page++;
            }
        } else {
            //complete
            scanManager->recordID.page = 1;
            scanManager->recordID.slot = 0;
        }
        pinPage(&tableManager->bufferPool, &scanManager->pageHandle, scanManager->recordID.page);
        char *data = scanManager->pageHandle.data;
        data = data + (scanManager->recordID.slot * recordSize);
        record->id.page = scanManager->recordID.page;
        record->id.slot = scanManager->recordID.slot;
        char *pointer = record->data;
        *pointer = '#';
        pointer++;
        memcpy(pointer, data + 1, recordSize - 1);
        scanManager->scanCountNumber++;
        evalExpr(record, schema, scanManager->condition, &result);
        if (result->v.boolV == TRUE) {
            unpinPage(&tableManager->bufferPool, &scanManager->pageHandle);
            return RC_OK;
        }
    }
    //no more tuples to scan
    return RC_RM_NO_MORE_TUPLES;
}

//close the scanner
extern RC closeScan(RM_ScanHandle *scan) {
    ScanManager *scanManager = scan->mgmtData;
    TableManager *tableManager = scan->rel->mgmtData;
    //check if the scan is complete
    if (scanManager->scanCountNumber > 0) {
        unpinPage(&tableManager->bufferPool, &scanManager->pageHandle);
        scanManager->recordID.page = 1;
        scanManager->scanCountNumber = 0;
        scanManager->recordID.slot = 0;
    }
    //reset the parameters
    scan->mgmtData = NULL;
    free(scan->mgmtData);
    return RC_OK;
}

//get the record size
extern int getRecordSize(Schema *schema) {
    int size = 0;
    for (int i = 0; i < schema->numAttr; i++) {
        //different type of the attribute
        if (schema->dataTypes[i] == DT_STRING)
            size = size + schema->typeLength[i];
        else if (schema->dataTypes[i] == DT_INT)
            size = size + sizeof(int);
        else if (schema->dataTypes[i] == DT_FLOAT)
            size = size + sizeof(float);
        else if (schema->dataTypes[i] == DT_BOOL)
            size = size + sizeof(bool);
    }
    size++;
    return size;
}

//create a new schema
extern Schema *
createSchema(int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys) {
    Schema *schema = (Schema *) malloc(sizeof(Schema));
    //initialize all the parameters
    schema->attrNames = attrNames;
    schema->numAttr = numAttr;
    schema->typeLength = typeLength;
    schema->dataTypes = dataTypes;
    schema->keyAttrs = keys;
    schema->keySize = keySize;
    return schema;
}

//remove schema and unset the memory
extern RC freeSchema(Schema *schema) {
    free(schema);
    return RC_OK;
}

//create an empty record
extern RC createRecord(Record **record, Schema *schema) {
    //allocate memory
    Record *newRecord = (Record *) malloc(sizeof(Record));
    newRecord->data = (char *) malloc(getRecordSize(schema));
    //initialize the info
    newRecord->id.page = newRecord->id.slot = -1;
    char *pointer = newRecord->data;
    //empty record
    *pointer = '#';
    pointer += 1;
    *pointer = '\0';
    *record = newRecord;
    return RC_OK;
}

//get attribute offset
RC attrOffset(Schema *schema, int attrNum, int *result) {
    *result = 1;
    for (int i = 0; i < attrNum; i++) {
        if (schema->dataTypes[i] == DT_STRING)
            *result = *result + schema->typeLength[i];
        else if (schema->dataTypes[i] == DT_INT)
            *result = *result + sizeof(int);
        else if (schema->dataTypes[i] == DT_FLOAT)
            *result = *result + sizeof(float);
        else if (schema->dataTypes[i] == DT_BOOL)
            *result = *result + sizeof(bool);
    }
    return RC_OK;
}

//free a certain record
extern RC freeRecord(Record *record) {
    free(record);
    return RC_OK;
}

//get the attribute from a specified record
extern RC getAttr(Record *record, Schema *schema, int attrNum, Value **value) {
    int position = 0;
    //get and set the info of the record
    attrOffset(schema, attrNum, &position);
    Value *attribute = (Value *) malloc(sizeof(Value));
    char *pointer = record->data;
    pointer = pointer + position;
    if (attrNum == 1) {
        schema->dataTypes[attrNum] = 1;
    }
    if (schema->dataTypes[attrNum] == DT_STRING) {
        //get attribute value of an attribute of type STRING
        attribute->dt = DT_STRING;
        attribute->v.stringV = (char *) malloc(schema->typeLength[attrNum] + 1);
        strncpy(attribute->v.stringV, pointer, schema->typeLength[attrNum]);
        attribute->v.stringV[schema->typeLength[attrNum]] = '\0';
    } else if (schema->dataTypes[attrNum] == DT_INT) {
        //get attribute value of an attribute of type INT
        attribute->dt = DT_INT;
        int result = 0;
        memcpy(&result, pointer, sizeof(int));
        attribute->v.intV = result;
    } else if (schema->dataTypes[attrNum] == DT_FLOAT) {
        //get attribute value of an attribute of type FLOAT
        attribute->dt = DT_FLOAT;
        float result;
        memcpy(&result, pointer, sizeof(float));
        attribute->v.floatV = result;
    } else if (schema->dataTypes[attrNum] == DT_BOOL) {
        //get attribute value of an attribute of type BOOLEAN
        attribute->dt = DT_BOOL;
        bool result;
        memcpy(&result, pointer, sizeof(bool));
        attribute->v.boolV = result;
    }
    *value = attribute;
    return RC_OK;
}

//update the value of a given record of a given schema's attrNum attribute to a given value
extern RC setAttr(Record *record, Schema *schema, int attrNum, Value *value) {
    int position = 0;
    //get the info
    attrOffset(schema, attrNum, &position);
    char *pointer = record->data;
    pointer = pointer + position;
    if (schema->dataTypes[attrNum] == DT_STRING) {
        //set attribute value of an attribute of type STRING
        strncpy(pointer, value->v.stringV, schema->typeLength[attrNum]);
    } else if (schema->dataTypes[attrNum] == DT_INT) {
        //set attribute value of an attribute of type INT
        *(int *) pointer = value->v.intV;
    } else if (schema->dataTypes[attrNum] == DT_FLOAT) {
        //set attribute value of an attribute of type FLOAT
        *(float *) pointer = value->v.floatV;
    } else if (schema->dataTypes[attrNum] == DT_BOOL) {
        //set attribute value of an attribute of type BOOLEAN
        *(bool *) pointer = value->v.boolV;
    }
    return RC_OK;
}
