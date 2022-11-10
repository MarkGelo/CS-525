## Incomplete, so can't run tests, but I finished most of the functions

Didn't find the time to complete this assignment but most of the functions I have finished and should be correct. For the functions that I didn't really implement, I just added some pseudocode of how it should work. 


**Functions**

**createTable**: To create table, have to create a page file and initialize it with values such as the header. The header includes the num records, num attr, keysize, datatype, typelenght, key attributes, attr names. These must fit in one page so it checks those. Then using the schema, it initializes it in the header, writes it in the page file and then closes it. Later on, when the manager opens the table then, it will initialize all those in structures for easier use.

**openTable**: To open the table, we make buffer pool and pagehandle, and then initialize the buffer pool, pinning the header with all the information we need. We then initialize the schema so we can add it to our tableData. To initialize the schema, we read the header with all the information already done there. To do this, we do some pointer magic, and just remember the way we added stuff to the header. Once we finish the schema, we then initialize all our structures that we will be using such as the PageMgr. the PageMgr stores the buffer pool, page handle, the records, and most importantly if there is free space, using an array. Once all of these are initialized and ready, the table is open and this function has done its job.

**closeTable**: Closing the table requires shutting down buffer pol/manager and frees all the memory used up by the table. To do this first, we need to make sure all the data is saved so we forcePage, write back to disk the header, just incase it was changed in memory, but not in disk. After that we can shutdown the buffer pool and then start freeing up memory. We first free the records by iterating over all the records and then free the array used for that, the bp, ph, the array used to determine free space and then we can free the schema. To free the schema, we can just use the function freeSchema we made -> Just free the arrays, dataTypes, typeLength, keyAttrs and for attrNames, have to iterate over them first, freeing attrNames[i] and then you can free attrNames. Then free the whole schema.

**deleteTable**: This assumes that closeTable is called first before this, since the only paramter this has is the name of the table, aka name of the page file too. So all this does is destroy the page file with that name, thus deleteting the table. To make sure nothing is lost and everything is good, closeTable must be called before this function.

**getNumTuples**: Similar to previous assignments, I just made a variable to store the number of tuples so when this function is called, its as simple as returning that variable.

**insertRecord**: Using the array we stored in the variable, specifically used for finding free space, we can interate over it to see a space for this current record. If space is found, it checks if it fits the size and if not it tries to keep finding space. Then the record structure is updated, such as the page and slot according to where it is inserted. The page is the page the record was inserted at and slot is the slot, or the index it was inserted in that specific page. If no page is found then we must increase the page in the file. To do this, we have to reallocate memory for the record arrays as well as the free space array.

**deleteRecord**: Locates the record from the table using the RID vars, such as rid page and slot. Once it is found then this function clears that location and updates all the things, so the manager knows this location is free. If the location specified doesnt have this record, then throws an error.

**updateRecord**: Locates the record from the table using the RID vars, such as rid page and slot. Once it is found then this function updates that location and updates all the things. If the location specified doesnt have this record, then throws an error.

**getRecord**: Similar to delete and update, but instead just gets the record. Since we know the size of the records, we can just memcpy. So if it finds the record, we then fill up record -> data and the id so, we successfully get the record.

**startScan**: Just initializes the scan, fills up the information in the scan handle and the helping scan manager structure.

**next**: Returns the next record that matches the scan condition. This uses the evalExpr given by the expr.c. Scans through each record page and updates where it currently is as it scans. If it matches then returns immediately.

**closeScan**: Just close the scan so can free the free(scan -> mgmtData) and then free(scan).

**getRecordSize**: To get record size using schema, we can use numAttr and iterate over. As you iterate, you check the datatype and increment the size of the type, or in the case of the string, you can use typeLength array. Once you iterate over, and add up all the size of attr, that is the record size.

**createSchema**: You just create the schema based on the parameters given. It's just creating schema using malloc, then the vars are initialized using the parameters then just return the created schema.

**freeSchema**: Just free the arrays, dataTypes, typeLength, keyAttrs and for attrNames, have to iterate over them first, freeing attrNames[i] and then you can free attrNames. Then free the whole schema.

**createRecord**: For creating a record, we just initialize the values of the record data, RID, using id.page and id.slot. I decided the initial values of page and slot will be 0s, and the data is initialized using malloc and getting the record size using the schema. The record size of the schema is found by iterating over the attributes in the schema, looking at the data types and calculating the whole size.

**freeRecord**: Just free the data first, using free(record -> data), and then you can free the whole record var, free(record).

**getAttr**: To get to the correct location of the attribute, we have to iterate over all the attributes, check there types and add to the starting location of the data. The starting location is found using record -> data, and then the offset is found by incrementing by the size of each attribute until we are at the right attribute, using attrNum. Now that we are at the correct location of our data, we can easily get the attribute. However, to return the value to the user, we have to set the correct vals, so the dt, data type to the correct one. And also then set v.intV or the other, to the current val. This way, the user will have a Value struct that has the correct values that we got. In case of getting the string, just make sure that it has an addition 0 byte at the end.

**setAttr**: To get to the correct location of the attribute, we have to iterate over all the attributes, check there types and add to the starting location of the data. The starting location is found using record -> data, and then the offset is found by incrementing by the size of each attribute until we are at the right attribute, using attrNum. Now that we are at the correct location of our data, the attribute to change, we just change it using value -> v.floatV or intV or stringV, etc. If it is type string, then we use strncpy, which copies it, but needs the length which can be found using schema -> typeLength[attrNum]
