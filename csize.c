#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

int cmp_int(const void* a, const void* b) {
	return ( *(int*)a - *(int*)b );
}

// Trie node
struct node{
	short cCap;				// capacity allocated for characters, at most 512, although in theory 256 is the maximum
	short tCap;				// capacity allocated for nodes
	short cSize;				// real size of characters, init as 1
	short tSize;				// real size of children nodes, init as 1

	char *chars;				// character list
	struct node** children;	// children node pointers list
};
typedef struct node tNode;

tNode *newNode() {
	// new node
	tNode *nwNode = malloc(sizeof(tNode));
	if (nwNode == NULL) {
		printf("New node ALLOC FAILED\n");
		return NULL;
	}
	// new chars and child node pointers, init with capacity 1
	nwNode->chars = malloc(sizeof(char));
	nwNode->children = malloc(sizeof(tNode*));
	if (nwNode->chars == NULL || nwNode->children == NULL)
		return NULL;
	nwNode->cCap = 1;
	nwNode->tCap = 1;
	nwNode->cSize = 0;
	nwNode->tSize = 0;

	return nwNode;
}

void pushChar( tNode *t, char c) {
	if (t->cCap == t->cSize) {		// full, reallocate
		char *new_ptr = (char *) realloc(t->chars, t->cCap * 2 * sizeof(char));  // allocate 2 times the old size; this should not exceed 256
		if (new_ptr != NULL) {
			t->cCap *= 2;
			t->chars = new_ptr;
		}
		else {
			free(t->chars);
			printf("MEMORY ALLOC FAILED\n");
		}
	}
	t->chars[t->cSize] = c;
	t->cSize ++;
	return;
}

// push new node to a node
void pushNode( tNode *t, tNode *nw) {
	if (t->tCap == t->tSize) {		// full, reallocate
		tNode **new_ptr = (tNode **) realloc(t->children, (t->tCap + 2) * sizeof(tNode*)); // allocate old size + 2; this should not exceed 256
		if (new_ptr != NULL) {
			t->tCap += 2;
			t->children = new_ptr;
		}
		else {
			free(t->children);
			printf("MEMORY ALLOC FAILED\n");
		}
	}
	t->children[t->tSize] = nw;
	t->tSize ++;
	return;
}

void cInsert(tNode *node, char c) {			// only insert char without creating new node
	pushChar(node, c);
	return; 
}

tNode *tInsert( tNode *node, char c) {			// Insert a new character as a node

	int toSwap = node -> tSize;		// push new character first
	pushChar(node, c);

	char tmp = node -> chars[toSwap];			// swap characters
	node -> chars[toSwap] = c;
	node -> chars[node->cSize - 1] = tmp;

	tNode *nwNode = newNode();					// push new children
	if (nwNode == NULL)
		printf("make new node FAILED\n");
	pushNode(node, nwNode);
	return nwNode;								// return inserted node
}

tNode *tPromote(tNode *node, int index) {		// Promote an existing character to a node, swapping it to an index closer to 0
	int toSwap = node -> tSize;					// Mark the index of char to swap

	char tmp = node -> chars[toSwap];			// swap characters
	node -> chars[toSwap] = node -> chars[index];
	node -> chars[index] = tmp;

	tNode *nwNode = newNode();
	pushNode(node, nwNode);
	return nwNode;
}

int cFind(tNode *node, char c) {			// return index of char, -1 if not found
	for(short i=0; i < node->cSize; i++)
		if (node -> chars[i] == c)
			return i;
	return -1;
}

tNode *tFind(tNode *node, char c) {			// find node in children by character
	for (short i = 0; i < node->tSize; i++) {
		if (node -> chars[i] == c)
			return node -> children[i];
	}
	return NULL;
}

// Find a character from head and return node
tNode *findFromHead(tNode *head, char c) {
	tNode *ptr = tFind(head, c);
	if (ptr == NULL)
		return NULL;
	return ptr;
}

// insert string of 2 characters to the tree
bool insertFromHead(tNode *head, char *s) {
	tNode *ptr = findFromHead(head, s[0]);
	if (ptr != NULL) {
		tInsert(ptr, s[1]);
		return true;
	}
	return false;
}

int freeAll(tNode *head) {
	int count = 0;
	for (short i = 0; i < head->tSize; i++) {
		count += freeAll(head->children[i]);
	}
	free(head->chars);
	free(head->children);
	free(head);
	return count + 1;
}

int main(int argc, char** argv){

	int base = 12;		// default is 12
	char filename[256];

	// Basic checking and putting arguments in place
	if (argc != 2) {
		if (argc != 4)
			return 0;
		else {
			if (strcmp(argv[1], "-s") != 0)
				return 0;
			base = atoi(argv[2]);
			strcpy(filename, argv[3]);
		}
	}
	else {
		strcpy(filename, argv[1]);
	}

	int LZW_count = 0;
	double LZW_avg = 0;
	int dict_size = 0;
	tNode *head = newNode();

	
	for (int i=0;i<256;i++) {
		tInsert(head, i);
		dict_size ++;
	}
		
	int dict_limit = 1 << base;	// shift base for dict_limit
	double entropy = 0;        // For Shanon's entropy
	double huffman_avg = 0;    // For calculating Huffman Tree
	long huffman_count = 0;

	char *p = malloc(sizeof(char));	// for reading
	int pCap = 1;					// capacity for dynamic allocation
	int len = 0;					// current length

	int buckets[256] = {};

	tNode *tmp = NULL;
	FILE *file;
	file = fopen(filename, "rb");	// read as bytes

	int c = 0;
	int res;
	bool REF = 0;		// For marking if the current level is a node or only a character
	int count = 0;

	while (fread(&c, 1, 1, file) != 0) {
		buckets[c]++;
		count++;

		// Check if expand p capacity is needed
		if (len == pCap) {
			char* ptr = realloc(p, sizeof(char) * pCap * 2);	// expand by *2
			if (ptr == NULL)
				printf("MEMORY ALLOC FAILED\n");
			p = ptr;
			pCap *= 2;											// update capacity
		}
		p[len] = c;
		len++;

		if (len > 1) {
			if (len == 2) {
				tmp = findFromHead(head, p[0]);
				tmp = tFind(tmp, p[1]);
				if (tmp != NULL)
					continue;
				if (dict_size < dict_limit) {
					insertFromHead(head, p);
					dict_size ++;
				}
			}
			else {
				if ( !REF ) {								// We are on right level
					res = cFind(tmp, c);					// Try get c
					if (res >= 0) {							// Found
						if (tmp->tSize > res)	{			// There is a node
							tmp = tmp->children[res];		
							continue;						// Read next character
						}
						REF = true;						// Signal that char is found but there is no node, may need to make one in next round
						continue;
					}
					if (dict_size < dict_limit) {
						cInsert(tmp, c);				// not found, insert c into node
						dict_size ++;
					}
				}
				else if (dict_size < dict_limit) {			// Wrong level and need to create new node
					tNode *a = tPromote(tmp, res);			// Promote second last character as node
					cInsert(a, c);							// Insert last character to new node
					dict_size ++;
				}
			}
			p[0] = c;
			len = 1;
			LZW_count ++;
			REF = false;
		}
	}
	if (len > 0)
		LZW_count ++;

	fclose(file);

	// Quick sort for huffman tree calculation
	qsort(buckets, 256, sizeof(int), cmp_int);

	// Shanon's Entropy
	int i = 0;
	while(i < 256) {
		if (buckets[i] > 0) {
			double prob = buckets[i] / (double) count;
			entropy += (prob * log2(1.0 / prob));
		}
		i++;
	}

	// Loops for Huffman Tree Calculation
	i = 0;
	while(i < 256) {
		if (buckets[i] > 0) {
			if (i != 255) {
				buckets[i+1] = buckets[i] + buckets[i+1];	// Merge buckets i and i+1
				buckets[i] = 0;								// Set smaller bucket as 0
				huffman_count += buckets[i+1];				// Add to count

				// bubble the new count up to keep the buckets sorted
				int j = i+1;
				while (j<255) {
					if (buckets[j] > buckets[j+1]) {
						// swap
						int tmp = buckets[j];
						buckets[j] = buckets[j+1];
						buckets[j+1] = tmp;

						j++;
					}
					else
						break;						// Finished if only one bucket is left
				}
			}
		}
		i++;
	}

	// free memory
	freeAll(head);
	free(p);


	// calculate and print
	huffman_avg = huffman_count / (double) count;
	LZW_avg = LZW_count * base / (double) count;
	
	printf("%.2f\n", entropy);
	printf("%.2f\n", huffman_avg);
	printf("%.2f\n", LZW_avg);

	return 0;
}