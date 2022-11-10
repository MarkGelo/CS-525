#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dberror.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "record_mgr.h"

// table and manager
RC initRecordManager (void *mgmtData){
    printf("Initialized Record Manager\n");
    return RC_OK;
}

RC shutdownRecordManager (){
    printf("Shutdown Record Manager\n");
    return RC_OK;
}

RC createTable (char *name, Schema *schema){
    if (access(name, F_OK) == 0){ // file exists already
        return RC_WRITE_FAILED; // dont want to del an already existing file ... not sure if its fine to create new RC defs
    }

    if(createPageFile(name) != RC_OK){ // has to create page file
        return 33; // cannot create table so cant do record manager stuff
    }

    // create header
    // make sure enough size
    int totalSize = 0;
    int i;
    for(i = 0; i < schema -> numAttr; i++){
        totalSize += strlen(schema -> attrNames[i]) * sizeof(char);
    }
    // want to keep number of tuples, number of attr, key size of attr
    totalSize += sizeof(char) + sizeof(char) + sizeof(char);
    // also  key attr, datatypes, typelength. 
    totalSize += schema -> numAttr * sizeof(char) + schema -> numAttr * sizeof(char) + schema -> keySize * sizeof(char);

    if(totalSize > PAGE_SIZE){ // header cant fit in one page
        return 33;
    }

    SM_FileHandle fh;
    SM_PageHandle ph = malloc(PAGE_SIZE);
    //memset(ph, '\0', PAGE_SIZE); // "\0" is wrong
    if(openPageFile(name, &fh) != RC_OK){
        return 33;
    }

    // write to page the values
    // write number of tuples
    //memset(ph, 0, sizeof(int));
    sprintf(ph, "%d", 0);
    ph += sizeof(int);
    
    // write number of attrs
    //memset(ph, 0, sizeof(int))
    sprintf(ph, "%d", schema -> numAttr);
    ph += sizeof(int);

    // write key size
    //memset(ph, 0, sizeof())
    sprintf(ph, "%d", schema -> keySize);
    ph += sizeof(int);

    // write data types
    for(i = 0; i < schema -> numAttr; i++){
        sprintf(ph, "%d", schema -> dataTypes[i]);
        ph += sizeof(int);
    }

    // write typelength
    for(i = 0; i < schema -> numAttr; i++){
        sprintf(ph, "%d", schema -> typeLength[i]);
        ph += sizeof(int);
    }

    // write key attr
    for(i = 0; i < schema -> numAttr; i++){
        sprintf(ph, "%d", schema -> keyAttrs[i]);
        ph += sizeof(int);
    }

    // write attr names
    for(i = 0; i < schema -> numAttr; i++){
        sprintf(ph, "%s", schema -> attrNames[i]);
        ph += strlen(schema -> attrNames[i]);
    }

    if(writeBlock(0, &fh, ph) != RC_OK){
        return 33;
    }
    if(closePageFile(&fh) != RC_OK){
        return 33;
    }

    free(ph);

    return RC_OK;
}

RC openTable (RM_TableData *rel, char *name){
    BM_BufferPool *bp = MAKE_POOL();
    BM_PageHandle *ph = MAKE_PAGE_HANDLE();

    if(initBufferPool(bp, name, 10, RS_LRU, NULL) != RC_OK){
        return 33;
    }

    if(pinPage(bp, ph, 0) != RC_OK){ // pin header
        return 33;
    }

    SM_PageHandle header = ph -> data;
    Schema *schema = malloc(sizeof(Schema));
    int i;

    // read header and initialize schema
    
    /* get values using pointer magic
    int cur_loc = header;
    int numTuples = getTuples(cur_loc);
    cur_loc += sizeof(char);

    schema -> numAttr = getNumAttr(cur_loc);
    cur_loc += sizeof(char);

    schema -> keySize = getVal(cur_loc);
    cur_loc += sizeof(char);

    schema -> dataTypes = malloc(sizeof(char) * schema -> numAttr);
    for(i = 0; i < schema -> numAttr; i++){
        schema -> dataTypes[i] = getVal(cur_loc);
        cur_loc += sizeof(char);
    }

    schema -> typeLength = malloc(sizeof(char) * schema -> numAttr);
    for(i = 0; i < schema -> numAttr; i++){
        schema -> typeLength[i] = getVal(cur_loc);
        cur_loc += sizeof(char);
    }

    schema -> keyAttrs = malloc(sizeof(char) * schema -> numAttr);
    for(i = 0; i < schema -> numAttr; i++){
        schema -> keyAttrs[i] = getVal(cur_loc);
        cur_loc += sizeof(char);
    }

    schema -> attrNames = malloc(sizeof(char) * schema -> numAttr);
    for(i = 0; i < schema -> numAttr; i++){
        schema -> attrNames[i] = getVal(cur_loc, "string");
        cur_loc += strlen(schema -> attrNames[i]) * sizeof(char);
    }

    // finished initializing schema

    rel -> schema = schema;
    unpinPage(bp, h);

    RM_PageMgr *pm = malloc(sizeof(RM_PageMgr));
    // init page mgr with these

    int numTuples;
	int numRecordPages;
	int *pageFullArray;
	BM_PageHandle *ph;
	RM_RecordPage **recordPages;
	BM_BufferPool * bp;

    // make array for records
    for(i = 0; i < numRecords; i++){
        pinPage(bp, ph, loc);

        // init records
        recordPages[loc] -> numTuples = tuples
            
            int numberOfTuples;
            RID rid; // page and slot
            int recordSize;
            int free;

        if freeSpace(i){
            full[i] = 0
        }else{
            full[i] = 1
        }
    }

    // finish up page mgr

    pm -> numTuples = tuples;
    pm -> ph = ph;
    pm -> bp = bp
        int numTuples;
        int numRecordPages;
        int *full;
        BM_PageHandle *ph;
        RM_RecordPage **recordPages;
        BM_BufferPool * bp;
    */

    return RC_OK;
}

RC closeTable (RM_TableData *rel){
    // if closing table, have to write to make sure nothing is lost
    // free the records
    /*

        RM_TableData *rel;
        void *mgmtData;

    RM_PageMgr *pm = rel -> mgmtData;

    forcePage(pm -> bp, 0);
    shutdownBufferPool(pm -> bp);

        int numTuples;
        int numRecordPages;
        int *full;
        BM_PageHandle *ph;
        RM_RecordPage **recordPages;
        BM_BufferPool *bp;

    int i;
    for(i = 0; i < pm -> numRecordPages; i++){
        free(pm -> recordPages[i])
    }

    free(pm -> recordPages)

    // free everything else

    free(full) // this is the array used to find free space
    free(ph)
    free(bp)

    also free schema

    freeSchema(rel -> schema)
    */

    return RC_OK;
}

RC deleteTable (char *name){
    // since this only gives the name, assumes that closeTable is done first before this so nothing is lost and everything is good

    if(destroyPageFile(name) != RC_OK){
        return 33; // fail in deleting table
    }

    return RC_OK;
}

int getNumTuples (RM_TableData *rel){
    return rel -> mgmtData -> numTuples;
}

// handling records in a table
RC insertRecord (RM_TableData *rel, Record *record){
    return RC_OK;
}

RC deleteRecord (RM_TableData *rel, RID id){
    return RC_OK;
}

RC updateRecord (RM_TableData *rel, Record *record){
    return RC_OK;
}

RC getRecord (RM_TableData *rel, RID id, Record *record){
    return RC_OK;
}

// scans
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond){
    return RC_OK;
}

RC next (RM_ScanHandle *scan, Record *record){
    return RC_OK;
}

RC closeScan (RM_ScanHandle *scan){
    // close scan so can just free scan
    free(scan -> mgmtData);
    free(scan);

    return RC_OK;
}

// dealing with schemas
int getRecordSize (Schema *schema){
    // get record size using schema, datatypes, and numAttr
    int recordSize = 0;
    int i;
    for(i = 0; i < schema -> numAttr; i++){
        if(schema -> dataTypes[i] == DT_INT){
            recordSize += sizeof(int);
        }else if(schema -> dataTypes[i] == DT_STRING){
            recordSize += schema -> typeLength[i];
        }else if(schema -> dataTypes[i] == DT_FLOAT){
            recordSize += sizeof(float);
        }else if(schema -> dataTypes[i] == DT_BOOL){
            recordSize += sizeof(bool);
        }
    }

    return recordSize;
}

Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys){
    // just create the schema based on the parameters
    Schema *schema = malloc(sizeof(Schema));

    schema -> numAttr = numAttr;
    schema -> attrNames = attrNames;
    schema -> dataTypes = dataTypes;
    schema -> typeLength = typeLength;
    schema -> keySize = keySize;
    schema -> keyAttrs = keys;

    return schema;
}

RC freeSchema (Schema *schema){
    // schema has numAttr, **attrNames, *dataTypes, *typeLength, *keyAttrs, keySize
    // free arrays
    free(schema -> dataTypes);
    free(schema -> typeLength);
    free(schema -> keyAttrs);
    // free attrNames
    int i;
    for(i = 0; i < schema -> numAttr; i++){
        free(schema -> attrNames[i]);
    }
    free(schema -> attrNames);
    free(schema);

    return RC_OK;
}

// dealing with records and attribute values
RC createRecord (Record **record, Schema *schema){
    *record = malloc(sizeof(Record));

    // get size of schema // similar to getting the offset
    int recordSize = 0;
    int i;
    for(i = 0; i < schema -> numAttr; i++){
        if(schema -> dataTypes[i] == DT_INT){
            recordSize += sizeof(int);
        }else if(schema -> dataTypes[i] == DT_STRING){
            recordSize += schema -> typeLength[i];
        }else if(schema -> dataTypes[i] == DT_FLOAT){
            recordSize += sizeof(float);
        }else if(schema -> dataTypes[i] == DT_BOOL){
            recordSize += sizeof(bool);
        }
    }
    
    // record size is based on schema, so getRecordSize first
    (*record) -> data = malloc(recordSize);
    // init values are 0
    (*record) -> id.page = 0;
    (*record) -> id.slot = 0;

    return RC_OK;
}

RC freeRecord (Record *record){
    // Record just has RID id and char* data.
    // no need to free RID
    free(record -> data);
    free(record);
    return RC_OK;
}

RC getAttr (Record *record, Schema *schema, int attrNum, Value **value){
    // find location using offset
    char* loc = record -> data;

    // calculate offset, to get to correct location
    int offset = 0;
    int i;
    for(i = 0; i < attrNum; i++){
        if(schema -> dataTypes[i] == DT_INT){
            offset += sizeof(int);
        }else if(schema -> dataTypes[i] == DT_STRING){
            offset += schema -> typeLength[i];
        }else if(schema -> dataTypes[i] == DT_FLOAT){
            offset += sizeof(float);
        }else if(schema -> dataTypes[i] == DT_BOOL){
            offset += sizeof(bool);
        }
    }

    loc += offset; // now at correct location

    *value = malloc(sizeof(Value)); // make new Value which will be "returned" to user

    // based on type, its different to get the attribute
    if(schema -> dataTypes[attrNum] == DT_INT){
        (*value) -> dt = DT_INT;
        int cur_val = *loc;
        (*value) -> v.intV = cur_val;
    }else if(schema -> dataTypes[attrNum] == DT_STRING){
        // string so extra stuff
        (*value) -> dt = DT_STRING;
        (*value) -> v.stringV = malloc(schema -> typeLength[attrNum] + 1); // + 1 since end has to end in \0
        (*value) -> v.stringV[schema -> typeLength[attrNum]] = '\0';
        strncpy((*value) -> v.stringV, loc, schema -> typeLength[attrNum]); // copies, "gets" the string value
    }else if(schema -> dataTypes[attrNum] == DT_FLOAT){
        (*value) -> dt = DT_FLOAT;
        float cur_val = *loc;
        (*value) -> v.floatV = cur_val;
    }else if(schema -> dataTypes[attrNum] == DT_BOOL){
        (*value) -> dt = DT_BOOL;
        bool cur_val = *loc;
        (*value) -> v.boolV = cur_val;
    }

    return RC_OK;
}

RC setAttr (Record *record, Schema *schema, int attrNum, Value *value){
    // find location using offset
    char* loc = record -> data;

    // calculate offset, to get to correct location
    int offset = 0;
    int i;
    for(i = 0; i < attrNum; i++){
        if(schema -> dataTypes[i] == DT_INT){
            offset += sizeof(int);
        }else if(schema -> dataTypes[i] == DT_STRING){
            offset += schema -> typeLength[i];
        }else if(schema -> dataTypes[i] == DT_FLOAT){
            offset += sizeof(float);
        }else if(schema -> dataTypes[i] == DT_BOOL){
            offset += sizeof(bool);
        }
    }

    loc += offset; // now at correct location

    // setting new attribute
    // assumes type doesnt change
    if(schema -> dataTypes[attrNum] == DT_INT){
        *loc = value -> v.intV;
    }else if(schema -> dataTypes[attrNum] == DT_STRING){
        // copies the attr from value, to "destination" which is the correct location
        // gets the "size" from type length
        strncpy(loc, value -> v.stringV, schema -> typeLength[attrNum]);
    }else if(schema -> dataTypes[attrNum] == DT_FLOAT){
        *loc = value -> v.floatV;
    }else if(schema -> dataTypes[attrNum] == DT_BOOL){
        *loc = value -> v.boolV;
    }

    return RC_OK;
}