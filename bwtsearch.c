#include <stdio.h>
#include <string.h>
#include <stdlib.h>     // exit()
#include <assert.h>
#include <unistd.h>
#include <math.h>

// Author: Victor Chan July 2021
// Principle: The goal of this assignment is to do BWT backward search by pre-computing the OCC table into an index file.
// In my design, the gap between snapshots of Occ table depends on the size of the bwt text file
// The ultimate goal is to fit the index file < 10 MB, so at reading time, I can always ready the entire index file into memory and keep total memory usage under 16MB.
// This way, I save any time needed to fread() the index file, and instead referencing OCC table directly from memory
// Index entries are written in fixed size 400KB blocks, since there is at most ~100 different characters in the text file whose count takes an integer record of 4 bytes.
// UPSIDE: I know exactly what location to look for in the indexfile to get closest Occ() value, also zero fread() is needed for index file throughout searching
// DOWNSIDE: For larger bwt files, say 100MB, I can only store 1/10 of what is there in the file, ie 400 bytes for every 4000 bytes scanned, to fit the 10MB limit
// This leads to a slower performance for large files due to increased characters to loop.

int SP_PARAM = 4000;            // Default sparseness for writing index entries, adjusted after reading the size of the bwt file
int HALF = 2000;                // Half of the sparse value to check location
int REC_SIZE = 400;             // Fixed rec size, unchanged


// compare function for quicksort
int compare(const void* a, const void* b) {
    return ( *(int*)a - *(int*)b );
}

// return the index no. of a character to store
// '\n' -> 0, ' ' -> 1 and so on
int K(char c) {
    if (c == 10)
        return 0;
    return c - 31;
}

// Reverse the K process, getting the character of the index
char revK(int i) {
    if (i)
        return (char) i + 31;
    return (char) 10;
}

// Write index file
void outToIndex(FILE *index, int *c) {
    fwrite( c, sizeof(int) , 100 , index );
    return;
}

// Main function to compute index file
void writeIndex(char *bwtFile, char *indexFile) {

    FILE *f = fopen(bwtFile, "rb");
    FILE *index = fopen(indexFile, "wb");

    fseek( f, 0, SEEK_END );
    int RawSize = ftell(f);         // get bwt Size
    fseek( f, 0, SEEK_SET );

    int limit = RawSize / 2;
    if (limit > 10 * 1024 * 1024)
        limit = 10*1024*1024;       // we target to generate an index file at most 10MB in size;

    double sparse_ratio = RawSize * 1.0 / limit;
    double sp = REC_SIZE * sparse_ratio;     // gap of OCC table
    SP_PARAM = ((int) ceil(sp / 100.0)) * 100;     // round up to nearest 100

    HALF = SP_PARAM / 2;
    // printf("SP_PARAM: %d\n", SP_PARAM);


    int MB5 = 4 * 1024 * 1024;      // 4MB buffer
    char *_5MB = malloc(MB5);

    size_t size  = sizeof(char) * MB5;
    size_t bytes_read = 1;
    size_t nmemb = 1 ;

    int counters[100] = {};
    int inc[100] = {};
    int total = 0;
    char c;
    int pos = 0;

    outToIndex(index, counters);    // output all zeroes at first

    while (bytes_read != 0) {

        bytes_read = fread(_5MB, nmemb, size, f);
        // printf("read: %d\n", bytes_read);

        int i = 0;
        while (i < bytes_read) {

            counters[ K(_5MB[i++]) ]++;
            total ++;

            if (total % SP_PARAM == 0) {
                outToIndex(index, counters);
            }
        }
    }

    // last out if there is new info
    if (total % SP_PARAM)
        outToIndex(index, counters);

    

    // write additional info to head bytes
    int linesTotal = counters[ K('\n') ];
    fwrite(&RawSize, sizeof(int), 1, index);
    fwrite(&linesTotal, sizeof(int), 1, index);
    fwrite(&SP_PARAM, sizeof(int), 1, index);


    // cleanup
    fclose(f);
    fclose(index);
    free(_5MB);

    return;
}


// Get Occ() with index file as an int array
int getOccBuff(int *index, FILE *bwt, int pos, char c, int bwtSize) {
    int seekPos;
    int tmp;
    int rem = pos % SP_PARAM;
    char toLoop[ HALF + 1 ];

    if ( rem <= HALF) {
        seekPos = ( pos / SP_PARAM ) * REC_SIZE / 4 + K(c);       // Start from lower position
        tmp = index[seekPos];                                     // Read from buffer directly

        fseek( bwt, ( pos - rem ), SEEK_SET );          // Search forward
        fread( toLoop, 1, rem, bwt );                   // read all characters until the character I need
        int i=0;

        while (i < rem) {
            if (toLoop[i++] == c)
                tmp ++;
        }
    }
    
    else {
        int backward;
        seekPos = ( pos / SP_PARAM + 1) * REC_SIZE / 4 + K(c);
        tmp = index[seekPos];  
        
        if ( bwtSize < (pos - rem + SP_PARAM) )
            backward = bwtSize - pos;
        else 
            backward = SP_PARAM - rem;

        fseek( bwt, pos, SEEK_SET);
        fread( toLoop, 1, backward, bwt);
        int i = backward - 1;

        while (i >= 0) {                                // Loop backward
            if (toLoop[i--] == c)
                tmp --;
        }
    }
    return tmp;
}

// Get last character starting from a position in bwt file
int getLastChar(int *index, FILE *bwt, int pos, char *c, int bwtSize) {
    int seekPos;
    int tmp;
    int rem = pos % SP_PARAM;
    char toLoop[ HALF + 1 ];

    if ( rem <= HALF) {

        fseek( bwt, ( pos - rem ), SEEK_SET );          
        fread( toLoop, 1, rem + 1, bwt );                   // read characters (including the character in question)
        *c = toLoop[rem];                                        // set c as character

        seekPos = ( pos / SP_PARAM ) * REC_SIZE / 4 + K(*c);
        tmp = index[seekPos];
        
        int i=0;
        while (i < rem) {
            if (toLoop[i++] == *c)
                tmp ++;
        }
    }
    
    else {
        int backward;
        
        if ( bwtSize < (pos - rem + SP_PARAM) )
            backward = bwtSize - pos;
        else 
            backward = SP_PARAM - rem;

        fseek( bwt, pos, SEEK_SET);
        fread( toLoop, 1, backward, bwt);
        *c = toLoop[0];                                      // set c as character
        seekPos = ( pos / SP_PARAM + 1) * REC_SIZE / 4 + K(*c);
        tmp = index[seekPos];
        
        int i = backward - 1;
        while (i >= 0) {                                // Loop backward
            if (toLoop[i--] == *c)
                tmp --;
        }
    }
    return tmp;
}

// Similar to getOccBuff(), but instead of getting Occ() for one character, get it for all characters
// Used for wild card searching in SearchForDup()
void getOccAllChar(int *index, FILE *bwt, int pos, int* update, int bwtSize) {
    int seekPos;
    int rem = pos % SP_PARAM;
    char toLoop[ HALF + 1 ];

    if ( rem <= HALF) {
        seekPos = ( pos / SP_PARAM ) * REC_SIZE / 4;       // Start from lower position
        memcpy(update, index+seekPos, 100 * sizeof(int));

        fseek( bwt, (pos - rem), SEEK_SET );          // Search forward
        fread( toLoop, 1, rem, bwt );                 // read all characters
        int i=0;

        while (i < rem)
            update[ K(toLoop[i++]) ] ++;
    }
    
    else {
        int backward;
        seekPos = ( pos / SP_PARAM + 1) * REC_SIZE / 4;
        memcpy(update, index+seekPos, 100 * sizeof(int));

        if ( bwtSize < (pos - rem + SP_PARAM) )
            backward = bwtSize - pos;
        else 
            backward = SP_PARAM - rem;

        fseek( bwt, pos, SEEK_SET);
        fread( toLoop, 1, backward, bwt);
        int i = backward - 1;

        while (i >= 0) {                                // Loop backward
            update[ K(toLoop[i--]) ] --;
        }
    }
    return;
}

// same as above function, but do updates for both start & end array, if they are in fact close to each other
// We use one fread() to cater both Occ instead of two fread(), saving read time by doing more computation
// This has proven to be worth it in the later stage of wild card search, when suffixes are getting longer, less lines possess the same suffix. Their position is often only <100 characters apart
void getOcc_Quick(int *index, FILE *bwt, int start, int end, int* u_start, int *u_end, int bwtSize) {
    int seekPos;
    int rem = start % SP_PARAM;
    char toLoop[ 4000 + SP_PARAM ];

    if ( rem > HALF )
        goto backward;

forward:
    seekPos = ( start / SP_PARAM ) * REC_SIZE / 4;
    memcpy(u_start, index+seekPos, 100 * sizeof(int));

    fseek(bwt, (start-rem), SEEK_SET);
    fread(toLoop, 1, rem + end - start, bwt);
    int i=0;
    while (i < rem)
        u_start[ K(toLoop[i++]) ] ++;

    // copy content
    memcpy(u_end, u_start, 100 * sizeof(int));

    // Keep looping for end
    while (i < rem + end - start)
        u_end[ K(toLoop[i++]) ] ++;
    return;

backward: ;
    int sizeToRead;
    short flag = 0;
    seekPos = ( start / SP_PARAM + 1) * REC_SIZE / 4;
    // printf("start: %d, end: %d\n", start, end);

    if ( bwtSize < (start - rem + SP_PARAM) )
        sizeToRead = bwtSize - start;
    else if ( end-start < SP_PARAM - rem )
        sizeToRead = SP_PARAM - rem;
    else {
        sizeToRead = end - start;
        flag = 1;                                   // flag to indicate checkpoint is between start and end pos
    }

    // printf("sizeToRead: %d\n", sizeToRead);
    fseek( bwt, start, SEEK_SET);
    fread( toLoop, 1, sizeToRead, bwt);

    if (!flag) {
        memcpy(u_end, index + seekPos, 100 * sizeof(int));
        int i = sizeToRead - 1;
        while (i >= end-start) {
            u_end[ K(toLoop[i--]) ] --;
        }
        // copy content
        memcpy(u_start, u_end, 100 * sizeof(int));

        // Keep looping for end
        while (i >= 0)
            u_start[ K(toLoop[i--]) ] --;

        return;
    }

    else {
        memcpy(u_end, index + seekPos, 100 * sizeof(int));
        memcpy(u_start, index + seekPos, 100 * sizeof(int));
        int i = SP_PARAM - rem;
        while ( i < sizeToRead )
            u_end[ K(toLoop[i++]) ] ++;

        i = SP_PARAM - rem - 1;
        while ( i >= 0 )
            u_start[ K(toLoop[i--]) ] --;
        // assert(1==0);
        return;
    }
}

// Get the entire line from a line number. Backward search from linebreak, then reverse characters
int getLineNo (int n, int *index, FILE *bwt, int bwtSize, char* write, int* inc, int nLine) {

    char c = '\0';
    int i = 0;
    int p, t;

    // special case for last line
    if ( n == nLine)
        p = 0;

    // normal case: get nth character in bwt file, which is the linebreak of nth line
    else
        p = n;

    while ( c != '\n' ) {
        t = getLastChar(index, bwt, p, &c, bwtSize);
        p = t + inc[K(c)];
        write[499-i] = c;
        i ++;
    };

    for(int j=0;j<=i;j++)
        write[j] = write[501-i+j];

    write[i-1] = '\n';
    write[i] = '\0';
    return i;
}

// helper function for checking Occ[100] arrays
short checkDiff( int *left, int *right ) {
    for(int i=1;i<100;i++)
        if (left[i] != right[i])
            return 1;
    return 0;
}

// help function for checking suffix
short matchString( char *stack, char *prefix, short slen, short plen) {
    for (short i=0;i<plen;i++) {
        if (stack[slen - 1 - i] != prefix[i])
            return 0;
    }
    return 1;
}

// DEBUG function
void outputQueue( char *queue, short ptr, char *line ) {
    for( int i=0;i<=ptr;i++ ) {
        if (queue[i] < 100)
            printf("%c", revK(queue[i]));
        else
            printf(">");
    }
    printf("\n");

    short lLen = strlen(line);
    for (int i=0;i<lLen;i++)
        printf("%c", line[lLen-1-i]);
    printf("\n");
}

// Wild card search to start finding duplicates from a prefix IN BATCH
// Starts with a list of candidates obtained from "-m" search, build a tree-like structure to search in a DFS manner
// Ends by finding either a linebreak character, or the match in prefix with the last substring tree expanded
// Easier to code using Recursion, but finally used while loop to save overhead for recursion
// In my testing, easily find > 300,000 matches within 10 seconds from 100MB file
int searchForDup( int start, int end, char *prefix, int * IND, FILE *bwt, int bwtSize, int *inc, int* lineNumbers, unsigned char mode) {


    int q_cap = 500 * 100;               // 50 KB alloc: at most 500 length * 100 characters
    int pos_alloc = 500 * 100 * sizeof(int) * 2;    // 400 KB alloc:  at most 500 length * 100 * 2 integers

    int *pos_store = (int*) malloc( pos_alloc );       // store start end position of character
    char *line = malloc( 501 );                 // storing the line so far
    char *queue = malloc( q_cap );
    memset(line, 0, 501);

    short plen = strlen(prefix);
    int total = end - start;
    int initCount[100] = {};
    int endCount[100] = {};
    short lPtr = -1;
    short qPtr = -1;

    int times = 0;

    // Get Occ() values for all characters
    getOccAllChar(IND, bwt, start, initCount, bwtSize);
    getOccAllChar(IND, bwt, end, endCount, bwtSize);

    // copying prefix to line
    for(int i=0;i<strlen(prefix);i++) {
        lPtr ++;
        line[i] = prefix[strlen(prefix) - 1 - i];
    }

    // adding start queue
    for(int i=1;i<100;i++) {
        if (endCount[i] > initCount[i]) {
            qPtr ++;
            queue[ qPtr ] = i;
            pos_store[ qPtr * 2 ] = inc[i] + initCount[i];
            pos_store[ qPtr * 2 + 1 ] = inc[i] + endCount[i];
        }
    }

    int up;
    int low;
    short halt = 0;

    // do Work until stack is empty
    while (qPtr >= 0) {

        // check instruction
        if ( queue[qPtr] < 100 ) {
            low = pos_store[ qPtr*2 ];
            up = pos_store[ qPtr*2+1 ];
            //outputQueue(queue, qPtr, line);
            // add to line
            lPtr ++;
            line[lPtr] = revK(queue[qPtr]);

            // Check for Duplicate
            if ( line[lPtr] == prefix[0] && matchString(line, prefix, lPtr+1, plen) ) {
                // outputQueue(queue, qPtr, line);
                total = total - (up - low);
                // printf("<===== DUP FOUND =====>\n\n");
                goto cleanup;
            }

            // Not match, expand current character
            // get Occ() values
            if (up - low > 4000) {
                getOccAllChar(IND, bwt, low, initCount, bwtSize);
                getOccAllChar(IND, bwt, up, endCount, bwtSize);
            }
            else
                getOcc_Quick(IND, bwt, low, up, initCount, endCount, bwtSize);


            // check if new level needed
            if (checkDiff(initCount, endCount)) {

                // new level preparation
                queue[qPtr] = 100;

                // adding to queue
                for(int i=1;i<100;i++) {
                    if (endCount[i] > initCount[i]) {
                        qPtr ++;
                        queue[qPtr] = i;
                        pos_store[ qPtr * 2 ] = inc[i] + initCount[i];
                        pos_store[ qPtr * 2 + 1 ] = inc[i] + endCount[i];
                        // printf("%c: %d-%d\n", revK(i), initCount[i], endCount[i]);
                    }
                }
                continue;
            }
            // line break
            if (!mode) {
                char c;
                int ln = getLastChar(IND, bwt, low, &c, bwtSize) + 1;
                lineNumbers[times] = ln;
            }
            times ++;
            goto cleanup;
        }

        // clean up and step back in queue
        cleanup:
            queue[qPtr] = '\0';
            line[lPtr] = '\0';
            lPtr --;
            qPtr --;
    }

    // printf("%d linebreaks\n", times);
    free(pos_store);
    free(line);
    free(queue);

    return total;
}

void decodeToFile(char *fileName, int* index, FILE* bwt, int size, int lineTotal, int *inc) {
    FILE *toWrite = fopen(fileName, "wb");
    int MB6 = 1 * 1024 * 1024;
    char *buff = malloc(MB6);       // 1MB buffer
    int i = 1;
    int full = 0;

    while ( i <= lineTotal ) {
        full += getLineNo (i, index, bwt, size, buff + full, inc, lineTotal);
        if (i == lineTotal || full > MB6 - 501) {
            printf("output: %d bytes\n", full);
            fwrite(buff, sizeof(char), full, toWrite);
            full = 0;
        }
        i++;
    }
    free(buff);
    fclose(toWrite);
    return;
}

int main(int argc, char** argv) {

    unsigned char mode = 0, existIndex = 0;
    char *queryString, *bwtFile, *indexFile;

    if (strlen(argv[1]) == 2) {
        if (!strcmp(argv[1], "-m")) {
            mode = 1;
        }
        else if (!strcmp(argv[1], "-n"))
            mode = 2;
        else if (!strcmp(argv[1], "-o"))
            mode = 3;
    }
    
    if (mode) {
        bwtFile = argv[2];
        indexFile = argv[3];
        queryString = argv[4];
    }
    else {
        bwtFile = argv[1];
        indexFile = argv[2];
        queryString = argv[3];
    }

    // Write new index if it does not exist yet
    if(!access(indexFile, F_OK))
        existIndex = 1;

    if (!existIndex)
        writeIndex(bwtFile, indexFile);


    // <=============== Start Reading ===============>

    int indexSize=0, lineTotal=0, bwtSize=0, sp=0;
    int inc[100];
    FILE *index = fopen(indexFile, "rb");
    fseek(index, -12, SEEK_END);
    
    fread(&bwtSize, sizeof(int), 1, index);         // read BWT Size from index
    fread(&lineTotal, sizeof(int), 1, index);       // read line no. from index
    fread(&sp, sizeof(int), 1, index);              // read Sparse param from index file
    // printf("bwtsize: %d, lineTotal: %d, SP_PARAM: %d\n", bwtSize, lineTotal, sp);
    SP_PARAM = sp;
    HALF = SP_PARAM / 2;

    fseek(index, 0, SEEK_END);
    indexSize = ftell(index);                       // get index file size
    fseek(index, indexSize - 412, SEEK_SET);
    fread(inc, sizeof(int), 100, index);
    // printf("indexSize: %d\n", indexSize);

    int sum = 0, last = 0;
    for(int i=0;i<100;i++) {
        sum += last;
        last = inc[i];
        inc[i] = sum;
    }

    fseek(index, 0, SEEK_SET);

    int *IND = malloc(indexSize);                   // allocated memory for storing entire index


    // read Index file into memory
    size_t bytes_read = 1;
    fseek( index, 0, SEEK_SET );
    int j = 0;
    int Fill;
    while (bytes_read != 0) {
        bytes_read = fread(IND + j/4, sizeof(int), indexSize / 4, index);
        IND[ j ] = Fill;
        j += bytes_read ;
    }
    // Debug
    /*
    for(int i=100;i>0;i--)
        printf("%d ", IND[ j-i ]);
    printf("\n");

    for (int i=0;i<100;i++)
        printf("%d ", inc[i]);
    printf("\n");
    */

    // Start doing work
    FILE *bwt = fopen(bwtFile, "rb");

    if (mode == 3) {
        decodeToFile(argv[4], IND, bwt, bwtSize, lineTotal, inc);
        goto End;
    }
    
    
    
    // search

    int x;
    int dup;
    
    short len = strlen(queryString);
    int start = inc[ K(queryString[len-1]) ];
    int end = inc[ K(queryString[len-1]) + 1];

    for (int i=len-2; i>=0; i--) {                  // loop from end character 
        int base = inc[ K(queryString[i]) ];
        start = base + getOccBuff(IND, bwt, start, queryString[i], bwtSize);
        end = base + getOccBuff(IND, bwt, end, queryString[i], bwtSize);
    }

    if (mode == 1) {
        printf("%d\n", end - start);
        goto End;
    }
    
    if (mode == 2) {
        int t;
        dup = searchForDup( start, end, queryString, IND, bwt, bwtSize, inc, &t, mode );
        printf("%d\n", dup);
        goto End;
    }

    // mode 0
    int lineNumbers[500] = {};
    dup = searchForDup( start, end, queryString, IND, bwt, bwtSize, inc, lineNumbers, mode );
    for (int i=0;i<500;i++)
        if ( lineNumbers[i] > lineTotal )
            lineNumbers[i] = 1;
    qsort( lineNumbers, 500, sizeof(int), compare);
    char line[500];

    for (int i=0;i<500;i++) {
        if (lineNumbers[i]) {
            getLineNo(lineNumbers[i], IND, bwt, bwtSize, line, inc, lineTotal);
            printf("%s", line);
        }
    }


End:
    // get Line by number
/*
    while (scanf("%d", &x)) {
        char line[500];
        getLineNo(x, IND, bwt, bwtSize, line, inc, lineTotal);
        printf("%s\n", line);
    }
*/

    free(IND);
    return 0;

}