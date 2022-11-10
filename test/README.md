## Incomplete, so can't run tests, but I finished most of the functions

Didn't find the time to complete this assignment but most of the functions I have finished and should be correct. For the functions, that I didn't really implement, I just added some pseudocode of how it should work. 

The only functions I didnt implement are: **FUNCTIONS ....** **FUNCTION 1**


**Functions I have completed**

**createTable**: TODO

**openTable**: TODO

**closeTable**: TODO

**deleteTable**: TODO

**getNumTuples**: TODO

**insertRecord**: TODO

**deleteRecord**: TODO

**updateRecord**: TODO

**getRecord**: TODO

**startScan**: TODO

**next**: TODO

**closeScan**: Just close the scan so can free the free(scan -> mgmtData) and then free(scan).

**getRecordSize**: To get record size using schema, we can use numAttr and iterate over. As you iterate, you check the datatype and increment the size of the type, or in the case of the string, you can use typeLength array. Once you iterate over, and add up all the size of attr, that is the record size.

**createSchema**: You just create the schema based on the parameters given. It's just creating schema using malloc, then the vars are initialized using the parameters then just return the created schema.

**freeSchema**: Just free the arrays, dataTypes, typeLength, keyAttrs and for attrNames, have to iterate over them first, freeing attrNames[i] and then you can free attrNames. Then free the whole schema.

**createRecord**: For creating a record, we just initialize the values of the record data, RID, using id.page and id.slot. I decided the initial values of page and slot will be 0s, and the data is initialized using malloc and getting the record size using the schema. The record size of the schema is found by iterating over the attributes in the schema, looking at the data types and calculating the whole size.

**freeRecord**: Just free the data first, using free(record -> data), and then you can free the whole record var, free(record).

**getAttr**: To get to the correct location of the attribute, we have to iterate over all the attributes, check there types and add to the starting location of the data. The starting location is found using record -> data, and then the offset is found by incrementing by the size of each attribute until we are at the right attribute, using attrNum. Now that we are at the correct location of our data, we can easily get the attribute. However, to return the value to the user, we have to set the correct vals, so the dt, data type to the correct one. And also then set v.intV or the other, to the current val. This way, the user will have a Value struct that has the correct values that we got. In case of getting the string, just make sure that it has an addition 0 byte at the end.

**setAttr**: To get to the correct location of the attribute, we have to iterate over all the attributes, check there types and add to the starting location of the data. The starting location is found using record -> data, and then the offset is found by incrementing by the size of each attribute until we are at the right attribute, using attrNum. Now that we are at the correct location of our data, the attribute to change, we just change it using value -> v.floatV or intV or stringV, etc. If it is type string, then we use strncpy, which copies it, but needs the length which can be found using schema -> typeLength[attrNum]
