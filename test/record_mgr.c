#include <stdlib.h>
#include <string.h>
#include "record_mgr.h"
#include "tables.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "assist.h"


TableMgmt_info tblmgmt_info;
RM_SCAN_MGMT rmScanMgmt;
SM_FileHandle fh;
SM_PageHandle ph;

//Initialize Storage manager
RC initRecordManager (void *mgmtData){
    initStorageManager();
    printf("Init Record Manager: Successed ...\n"); 
	return RC_OK;
}


//free the memory
RC shutdownRecordManager (){ 
    shutdownBufferPool(&tblmgmt_info.bufferPool); 
    free(ph);
    printf("Shut down Record Manager: Successed ...\n");
    return RC_OK;
}

//sub-function
void write(char data[1024], Schema *schema, char *name) {
    if (!schema) {
        printf("Write: Failed ...\n");
        return;
    }    
    if (!name) {
        printf("Write: Failed ...\n");
        return;
    }  
    strcat(data, name);
    strcat(data, "|");
    sprintf(data+ strlen(data),"%d[",(*schema).numAttr);
    for(int i=0; i<(*schema).numAttr; i++){
        sprintf(data+ strlen(data),"(%s:%d~%d)",(*schema).attrNames[i],(*schema).dataTypes[i],(*schema).typeLength[i]);
    }
    sprintf(data+ strlen(data),"]%d{",(*schema).keySize);
    for(int i=0; i<(*schema).keySize; i++){
        sprintf(data+ strlen(data),"%d",(*schema).keyAttrs[i]);
        if(i<((*schema).keySize-1))
            strcat(data,":");
    }
    strcat(data,"}");
}

RC createTable (char *name, Schema *schema){
    if (!schema) {
        printf("Create Table: Failed ...\n");
        return RC_ERROR;
    }    
    if (!name) {
        printf("Create Table: Failed ...\n");
        return RC_ERROR;
    }      
    createPageFile(name);
    ph = malloc(PAGE_SIZE);
    char tableMetadata[PAGE_SIZE];
    memset(tableMetadata,'\0',PAGE_SIZE);
    write(tableMetadata, schema, name);
    tblmgmt_info.firstFreeLoc.page =1;
    tblmgmt_info.firstFreeLoc.slot =0;
    tblmgmt_info.totalRecordInTable =0;
    sprintf(tableMetadata+ strlen(tableMetadata),"$%d:%d$",tblmgmt_info.firstFreeLoc.page,tblmgmt_info.firstFreeLoc.slot);
    sprintf(tableMetadata+ strlen(tableMetadata),"?%d?",tblmgmt_info.totalRecordInTable);
    memmove(ph,tableMetadata,PAGE_SIZE);

    openPageFile(name, &fh);
    writeBlock(0, &fh, ph);
    free(ph);
    printf("Create Table: Successed ...\n");
    return RC_OK;
}
    

//sub-function
int* extractKeyDt(char *data,int keyNum){
    int num = keyNum == 0 ? 2 : keyNum;
    int* values =(int *) malloc(sizeof(int) * num);
    char *val = (char *) malloc(sizeof(int)*2);
    memset(val,'\0',sizeof(int)*2);
    int i=0; 
    int j=0;
    for(int k=0; data[k]!='\0'; k++){
        if(data[k]==':' ){ 
            values[j]=atoi(val); 
            memset(val,'\0',sizeof(int)*2); 
            i=0; 
            j++; 
        }
        else val[i++] = data[k];
    }
    if (keyNum == 0) {
        values[1]=atoi(val);
    } else {
        values[keyNum-1] =atoi(val);        
    }
    return  values;
}


//sub-function
char * readAttributeData(char *scmData, int key){  
    int num = key == 0 ? 100: 50 ;
    char *atrData = (char *) malloc(sizeof(char)*num);
    memset(atrData,'\0',sizeof(char)*num);
    int i=0; 
    int j=0;
    switch (key) {
    case 0:
        while(scmData[i] != '[') i++; i++;
        for(j=0;scmData[i] != ']'; j++) 
            atrData[j] = scmData[i++];
        break;
    case 1:
        while(scmData[i] != '{') i++; 
        i++;
        for(j=0;scmData[i] != '}'; j++) 
            atrData[j] = scmData[i++];        
        break;
    case 2:
        while(scmData[i] != '$') i++; i++;
        for(j=0;scmData[i] != '$'; j++) 
            atrData[j] = scmData[i++];
        break; 
    }
    atrData[j]='\0';
    return atrData;
}


//sub-function
int readTotalAttributes(char *scmData, int key){
    int num = key == 2 ? 10 : 2;
    char *strNumAtr = (char *) malloc(sizeof(int)*num);
    memset(strNumAtr,'\0',sizeof(int)*num);
    int i=0;
    int j=0;

    switch (key) {
    case 0:
        for(i=0; scmData[i] != '|'; i++){
        }
        i+= 1;
        while(scmData[i] != '['){
            strNumAtr[j++]=scmData[i++];
        }        
        break;
    case 1:
        for(i=0; scmData[i] != ']'; i++){
        }
        i+=1;
        while(scmData[i] != '{'){
            strNumAtr[j++]=scmData[i++];
        }
        break;
    case 2:
        while(scmData[i] != '?') 
            i++; 
        i += 1;
        for(j=0;scmData[i] != '?'; j++) 
            strNumAtr[j] = scmData[i++];
        break;
    }
    strNumAtr[j]='\0';
    return atoi(strNumAtr);
}


//sub-function
int * getAtributes(char *scmData, int numAtr, int num){
    int *dt_type=(int *) malloc(sizeof(int) *numAtr);
    for(int i=0; i<numAtr; i++){
        char *atrData = (char *) malloc(sizeof(char)*30);
        int count=0; int k=0; int j=0;
        while(count<=i) if(scmData[k++] == '(') count++;
        for(j=0;scmData[k] != ')'; j++) atrData[j] = scmData[k++];
        atrData[j]='\0';
        char *dtTp = (char *) malloc(sizeof(int)*2);
        memset(dtTp,'\0',sizeof(char)*10);
        int l; int m;
        if (num == 0) {
            for(l =0 ; atrData[l]!=':'; l++){} l++;
            for(m=0; atrData[l]!='~'; m++) dtTp[m]=atrData[l++];
        } else {
            for(l =0 ; atrData[l]!='~'; l++){} 
            l++;
            for(m=0; atrData[l]!='\0'; m++) 
                dtTp[m]=atrData[l++];
        }
        dtTp[m]='\0';
        dt_type[i]  = atoi(dtTp);
    }
    return dt_type;
}


//sub-function
char ** getAtrributesNames(char *scmData, int numAtr){
    char ** attributesName = (char **) malloc(sizeof(char)*numAtr);
    for(int i=0; i<numAtr; i++){
        char *atrData = (char *) malloc(sizeof(char)*30);
        int count=0; int k=0; int j=0;
        while(count<=i) if(scmData[k++] == '(') count++;
        for(j=0;scmData[k] != ')'; j++) atrData[j] = scmData[k++];
        atrData[j]='\0';
        char *atrDt = atrData;
        char *name = (char *) malloc(sizeof(char)*10);
        memset(name,'\0',sizeof(char)*10);
        int l;
        for(l=0; atrDt[l]!=':'; l++) name[l] = atrDt[l];
        name[l]='\0';
        attributesName[i] = malloc(sizeof(char) * strlen(name));
        strcpy(attributesName[i],name);
        free(name);
        free(atrDt);
    }
    return attributesName;
}



// Using buffer manager we pin page 0 and read data from page 0. Data parsed by schemaReadFromFile method and value initialied in rel
RC openTable (RM_TableData *rel, char *name){
    if (!name) {
        printf("Open Table: Failed ...\n");
        return RC_ERROR;
    }
    if (!rel) {
        printf("Open Table: Failed ...\n");
        return RC_ERROR;
    }
    
    ph = malloc(PAGE_SIZE);
    initBufferPool(&tblmgmt_info.bufferPool, name, 3, RS_FIFO, NULL);
    pinPage(&tblmgmt_info.bufferPool, &tblmgmt_info.pageHandle, 0);

    char metadata[PAGE_SIZE];
    strcpy(metadata,tblmgmt_info.pageHandle.data);
    char *tbleName = (char *) malloc(sizeof(char)*20);
    memset(tbleName,'\0',sizeof(char)*20);
    for(int i=0; metadata[i] != '|'; i++){
        tbleName[i]=metadata[i];
        tbleName[i+1]='\0';
    }
    int totalAtribute = readTotalAttributes(metadata, 0);
    char *atrMetadata =readAttributeData(metadata, 0);
    char **cpNames = (char **) malloc(sizeof(char*) * totalAtribute);
    DataType *cpDt = (DataType *) malloc(sizeof(DataType) * totalAtribute);
    int *cpSizes = (int *) malloc(sizeof(int) * totalAtribute);
    int *cpKeys = (int *) malloc(sizeof(int)*readTotalAttributes(metadata,1));
    char *cpSchemaName = (char *) malloc(sizeof(char)*20);
    memset(cpSchemaName,'\0',sizeof(char)*20);
    for(int i = 0; i < totalAtribute; i++) {
        cpNames[i] = (char *) malloc(sizeof(char) * 10);
        strcpy(cpNames[i], getAtrributesNames(atrMetadata,totalAtribute)[i]);
    }
    memcpy(cpDt, getAtributes(atrMetadata,totalAtribute, 0), sizeof(DataType) * totalAtribute);
    memcpy(cpSizes, getAtributes(atrMetadata,totalAtribute, 1), sizeof(int) * totalAtribute);
    memcpy(cpKeys, extractKeyDt(readAttributeData(metadata,1),readTotalAttributes(metadata,1)), sizeof(int) * readTotalAttributes(metadata,1));
    memcpy(cpSchemaName,tbleName,strlen(tbleName));
    (*rel).schema=createSchema(totalAtribute, cpNames, cpDt, cpSizes, readTotalAttributes(metadata,1), cpKeys);;
    (*rel).name =cpSchemaName;
    tblmgmt_info.rm_tbl_data = rel;
    tblmgmt_info.sizeOfRec =  getRecordSize((*rel).schema) + 1;   
    tblmgmt_info.blkFctr = (PAGE_SIZE / tblmgmt_info.sizeOfRec);
    tblmgmt_info.firstFreeLoc.page =extractKeyDt(readAttributeData(metadata,2), 0)[0];
    tblmgmt_info.firstFreeLoc.slot =extractKeyDt(readAttributeData(metadata,2), 0)[1];
    tblmgmt_info.totalRecordInTable = readTotalAttributes(metadata, 2);
    unpinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    printf("Open Table: Successed ...\n");
    return RC_OK;
}


// write all information back to page 0. This would override exsiting data.
RC closeTable (RM_TableData *rel){
    if (!rel) {
        printf("Close Table: Failed ...\n");
        return RC_ERROR;
    }
    char tableMetadata[PAGE_SIZE];
    memset(tableMetadata,'\0',PAGE_SIZE);
    write(tableMetadata, (*rel).schema, (*rel).name);
    sprintf(tableMetadata+ strlen(tableMetadata),"$%d:%d$",tblmgmt_info.firstFreeLoc.page,tblmgmt_info.firstFreeLoc.slot);
    sprintf(tableMetadata+ strlen(tableMetadata),"?%d?",tblmgmt_info.totalRecordInTable);
    pinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle,0);
    memmove(tblmgmt_info.pageHandle.data,tableMetadata,PAGE_SIZE);
    markDirty(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    unpinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    shutdownBufferPool(&tblmgmt_info.bufferPool);
    printf("Close Table: Successed ...\n");
    return RC_OK;
}


// delete the table and all data associated with it.
RC deleteTable (char *name){
    if (!name) {
        printf("Delete Table: Failed ...\n");
        return RC_ERROR;
    }
    destroyPageFile(name);
    printf("Delete Table: Successed ...\n");
    return RC_OK;
}


// Returns total numbers of records in table
int getNumTuples (RM_TableData *rel){
    if (!rel) {
        printf("Get NumTuples: Failed ...\n");
        return RC_ERROR;
    }
    printf("Get NumTuples: Successed ...\n");
    return tblmgmt_info.totalRecordInTable;
}


/*
    This will insert the record passed in input parameter. Record will be inserted at avialable page and slot.
    Next available page and slot will be updated after record inserted into file.
    Next availble page amd slot has mentioned in  firstFreeLoc of RID declared in TableMgmt_info
*/
RC insertRecord (RM_TableData *rel, Record *record){
    if (!record) {
        printf("Insert Record: Failed ...\n");
        return RC_ERROR;
    }   
    if (!rel) {
        printf("Insert Record: Failed ...\n");
        return RC_ERROR;
    }
    pinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle,tblmgmt_info.firstFreeLoc.page);
    (*record).data[tblmgmt_info.sizeOfRec-1]='$';
    memcpy(tblmgmt_info.pageHandle.data+(tblmgmt_info.firstFreeLoc.slot * tblmgmt_info.sizeOfRec), (*record).data, tblmgmt_info.sizeOfRec);
    markDirty(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    unpinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    (*record).id.page = tblmgmt_info.firstFreeLoc.page;  // storing page number for record
    (*record).id.slot = tblmgmt_info.firstFreeLoc.slot;  // storing slot number for record
    tblmgmt_info.totalRecordInTable++;
    if(tblmgmt_info.firstFreeLoc.slot ==(tblmgmt_info.blkFctr-1)){
        tblmgmt_info.firstFreeLoc.page++;
        tblmgmt_info.firstFreeLoc.slot = 0;
    }else{
        tblmgmt_info.firstFreeLoc.slot++;
    }
    printf("Insert Record: Successed ...\n");
    return RC_OK;
}


// This method will delete the record passed in input parameter id
RC deleteRecord (RM_TableData *rel, RID id){
    if (!rel) {
        printf("Delete Record: Failed ...\n");
        return RC_ERROR;
    }  
    pinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle,id.page);
    memset(tblmgmt_info.pageHandle.data+(id.slot * tblmgmt_info.sizeOfRec), '\0', tblmgmt_info.sizeOfRec);  // setting values to null
    tblmgmt_info.totalRecordInTable--;  // reducing number of record by 1 after deleting record
    markDirty(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    unpinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    printf("Delete Record: Successed ...\n");
    return RC_OK;
}


// Update record will  update the perticular record at page and slot metioned in record
RC updateRecord (RM_TableData *rel, Record *record){
    if (!record) {
        printf("Update Recorded: Failed ...\n");
        return RC_ERROR;
    }    
    if (!rel) {
        printf("Update Recorded: Failed ...\n");
        return RC_ERROR;
    }      
    pinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle,(*record).id.page);
    memcpy(tblmgmt_info.pageHandle.data+((*record).id.slot * tblmgmt_info.sizeOfRec), (*record).data, tblmgmt_info.sizeOfRec-1);  
    markDirty(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    unpinPage(&tblmgmt_info.bufferPool,&tblmgmt_info.pageHandle);
    printf("Update record: Successed ...\n");
    return RC_OK;
}
    
    
// This method will return record at perticular page number and slot number mention in input parameter id
RC getRecord (RM_TableData *rel, RID id, Record *record){
    if (!record) {
        printf("Get Record: Failed ...\n");
        return RC_ERROR;
    }    
    if (!rel) {
        printf("Get Record: Failed ...\n");
        return RC_ERROR;
    }   
    pinPage(&tblmgmt_info.bufferPool, &tblmgmt_info.pageHandle,id.page);
    memcpy((*record).data, tblmgmt_info.pageHandle.data+ (id.slot * tblmgmt_info.sizeOfRec) , tblmgmt_info.sizeOfRec); 
    (*record).data[tblmgmt_info.sizeOfRec-1]='\0';
    (*record).id.page = id.page;
    (*record).id.slot = id.slot;
    unpinPage(&tblmgmt_info.bufferPool, &tblmgmt_info.pageHandle);
    printf("Get Record: Successed ...\n");
    return RC_OK;
}


// Initialized all the parameter related to scan function.
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond){
    if (!rel) {
        printf("start scan: Failed ...\n");
        return RC_ERROR;
    }      
    if (!scan) {
        printf("start scan: Failed ...\n");
        return RC_ERROR;
    }  
    if (!cond) {
        printf("start scan: Failed ...\n");
        return RC_ERROR;
    }    
    (*scan).rel = rel;
    rmScanMgmt.cond=cond;
    rmScanMgmt.recID.page=1; // records starts from page 1
    rmScanMgmt.recID.slot=0; // slot starts from 0
    return RC_OK;
}


// This will will scan every record. after reading record it checks the condition agained the scan passed as parameter
RC next (RM_ScanHandle *scan, Record *record){
    if (!scan) {
        printf("next: Failed ...\n");
        return RC_ERROR;
    }  
    if (!record) {
        printf("next: Failed ...\n");
        return RC_ERROR;
    }    
    Value *queryExpResult = (Value *) malloc(sizeof(Value));
    while(rmScanMgmt.count<tblmgmt_info.totalRecordInTable){
        getRecord((*scan).rel,rmScanMgmt.recID,record);
        rmScanMgmt.count = rmScanMgmt.count+1;   // scanning start from current count to total no of records / increment counter by 1
        evalExpr(record, (*((*scan).rel)).schema, rmScanMgmt.cond, &queryExpResult);
        if((*queryExpResult).v.boolV ==1){
            rmScanMgmt.recID.slot= rmScanMgmt.recID.slot + 1; 
            return RC_OK;
        }
        rmScanMgmt.recID.slot = rmScanMgmt.recID.slot + 1;
    }
    rmScanMgmt.recID.slot=0; // slot starts from 0
    rmScanMgmt.count = 0;
    return  RC_RM_NO_MORE_TUPLES;
}


// this method will close the scan and reset all information in RM_SCAN_MGMT
RC closeScan (RM_ScanHandle *scan){
    if (!scan) {
        printf("Close Scan: Failed ...\n");
        return RC_ERROR;
    }  
    return RC_OK;
}


// This will return size of record based on  data type, length of each field
int getRecordSize (Schema *schema){
    if (!schema) {
        printf("Get Record Size: Failed ...\n");
        return RC_ERROR;
    }  
    int recordSize = 0;
    for(int i=0; i<(*schema).numAttr; i++){
        if((*schema).dataTypes[i] == DT_INT) 
            recordSize += sizeof(int);
        else if((*schema).dataTypes[i] == DT_STRING) 
            recordSize += (sizeof(char) * (*schema).typeLength[i]);
        else if((*schema).dataTypes[i] == DT_FLOAT) 
            recordSize += sizeof(float);
        else 
            recordSize += sizeof(bool);
    }
    return recordSize;
}


// create the schema with given input parameter
Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys){
    if (!numAttr) {
        printf("Create Schema: Failed ...\n");
        return NULL;
    }
    if (!attrNames) {
        printf("Create Schema: Failed ...\n");
        return NULL;
    }   
    if (!dataTypes) {
        printf("Create Schema: Failed ...\n");
        return NULL;
    }  
    if (!typeLength) {
        printf("Create Schema: Failed ...\n");
        return NULL;
    }   
    if (!keySize) {
        printf("Create Schema: Failed ...\n");
        return NULL;
    }  
    if (!keys) {
        printf("Create Schema: Failed ...\n");
        return NULL;
    }      
    Schema *schema = (Schema * ) malloc(sizeof(Schema));
    (*schema).numAttr = numAttr;
    (*schema).attrNames = attrNames;
    (*schema).dataTypes = dataTypes;
    (*schema).typeLength = typeLength;
    (*schema).keySize = keySize;
    (*schema).keyAttrs = keys;
    tblmgmt_info.sizeOfRec = getRecordSize(schema);
    tblmgmt_info.totalRecordInTable = 0;
    return schema;
}


// Free the mememory related to schema
RC freeSchema (Schema *schema){
    if (!schema) {
        printf("Free Schema: Failed ...\n");
        return RC_ERROR;
    }  
    free(schema);
    return RC_OK;
}


// Create the new record with all null '\0' values
RC createRecord (Record **record, Schema *schema){
    if (!schema) {
        printf("Create Record Size: Failed ...\n");
        return RC_ERROR;
    }      
    if (!record) {
        printf("Create Record Size: Failed ...\n");
        return RC_ERROR;
    }  
    Record *newRec = (Record *) malloc (sizeof(Record));
    (*newRec).data = (char *)malloc(sizeof(char) * tblmgmt_info.sizeOfRec);
    memset((*newRec).data,'\0',sizeof(char) * tblmgmt_info.sizeOfRec);
    (*newRec).id.page =-1;           //set to -1 bcz it has not inserted into table/page/slot
    *record = newRec;
    return RC_OK;
}


// Free the memory related to record
RC freeRecord (Record *record){
    if (!record) {
        printf("Free Record: Failed ...\n");
        return RC_ERROR;
    }  
    free((*record).data);
    free(record);
    return RC_OK;
}


// return value of attribute pointed by attrnum in value
RC getAttr (Record *record, Schema *schema, int attrNum, Value **value){
    int offset = getAtrOffsetInRec(schema,attrNum);
    char *subString ;
    if ((*schema).dataTypes[attrNum] == DT_INT){
        memcpy(subString= malloc(sizeof(int)+1), (*record).data+offset, sizeof(int));
        subString[sizeof(int)]='\0';          // set last byet to '\0'
        MAKE_VALUE(*value, DT_INT, atoi(subString));
    } else if ((*schema).dataTypes[attrNum] == DT_STRING){
        memcpy(subString= malloc((sizeof(char)*(*schema).typeLength[attrNum])+1), (*record).data+offset, sizeof(char)*(*schema).typeLength[attrNum]);
        subString[sizeof(char)*(*schema).typeLength[attrNum]]='\0';       // set last byet to '\0'
        MAKE_STRING_VALUE(*value, subString);
    } else if ((*schema).dataTypes[attrNum] == DT_FLOAT){
        memcpy(subString= malloc(sizeof(float)+1), (*record).data+offset, sizeof(float));
        subString[sizeof(float)]='\0';          // set last byet to '\0'
        MAKE_VALUE(*value, DT_FLOAT, atof(subString));
    } else if ((*schema).dataTypes[attrNum] == DT_BOOL){
        memcpy(subString= malloc(sizeof(bool)+1), (*record).data+offset, sizeof(bool)); MAKE_VALUE(*value, DT_BOOL, 0);
    }
    free(subString);
    return RC_OK;
}


//Set value of percular attribute given in attrNum
RC setAttr (Record *record, Schema *schema, int attrNum, Value *value){
    int offset = getAtrOffsetInRec(schema,attrNum);
    char intStr[sizeof(int)+1];
    char intStrTemp[sizeof(int)+1];
    memset(intStr,'0',sizeof(char)*4);
    if ((*schema).dataTypes[attrNum] == DT_INT) {
        strRepInt(3,(*value).v.intV,intStr); 
        sprintf((*record).data + getAtrOffsetInRec(schema,attrNum), "%s", intStr);
    } else if ((*schema).dataTypes[attrNum] == DT_STRING) {
        sprintf((*record).data + getAtrOffsetInRec(schema,attrNum), "%s", (*value).v.stringV);
    } else if ((*schema).dataTypes[attrNum] == DT_FLOAT) {
        sprintf((*record).data + getAtrOffsetInRec(schema,attrNum),"%f" ,(*value).v.floatV);
    } else if ((*schema).dataTypes[attrNum] == DT_BOOL) {
        strRepInt(1,(*value).v.boolV,intStr); 
        sprintf((*record).data + getAtrOffsetInRec(schema,attrNum),"%s" ,intStr);
    }
    return RC_OK;
}

//Sub-function
void strRepInt(int j,int val,  char *intStr){
    int last = j;
    while (val > 0 && j >= 0) { 
        intStr[j--] += val % 10; 
        val /= 10; 
    }
    last += 1;
    intStr[last] = '\0';
}


// returns offset/string posing of perticular attribute
int getAtrOffsetInRec(Schema *schema, int atrnum){
    int offset=0;
    for(int pos=0; pos<atrnum; pos++){
        if((*schema).dataTypes[pos]== DT_INT) 
            offset = offset + sizeof(int);
        else if((*schema).dataTypes[pos]==DT_STRING) 
            offset = offset + (sizeof(char) *  (*schema).typeLength[pos]);
        else if((*schema).dataTypes[pos]==DT_FLOAT) 
            offset = offset + sizeof(float);
        else if((*schema).dataTypes[pos]==DT_BOOL) 
            offset = offset  + sizeof(bool);
    }
    return offset;
}