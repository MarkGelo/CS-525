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

}

RC shutdownRecordManager (){

}

RC createTable (char *name, Schema *schema){

}

RC openTable (RM_TableData *rel, char *name){

}

RC closeTable (RM_TableData *rel){

}

RC deleteTable (char *name){

}

int getNumTuples (RM_TableData *rel){

}

// handling records in a table
RC insertRecord (RM_TableData *rel, Record *record){

}

RC deleteRecord (RM_TableData *rel, RID id){

}

RC updateRecord (RM_TableData *rel, Record *record){

}

RC getRecord (RM_TableData *rel, RID id, Record *record){

}

// scans
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond){

}

RC next (RM_ScanHandle *scan, Record *record){

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
    free(schema)

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

    return RC_OK
}