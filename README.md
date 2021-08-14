# 9319-Web-Data-compression

Due to assignment constraints, all these code are constructed to be as memory efficient as possible.
Runtime memory is always assume to be <16MB, for a maximum size of file given.

# Assignment 1
Data Entropy & LZW encoder bit calculation<br/>
Given a binary file, calculate certain qualities, huffman encoding bits and LZW-complete sizes.<br/>

Architecture<br/>
Construct Trie to quickly navigate and compare different subtrees.<br/>
Grade: 30/30<br/>
<br/>
# Assignment 2
BWT backward search<br/>
Given a BWT encoded file, use BWT backward search to quickly determine matches, duplicated lines and decode the file into an output file.<br/>
Grade: 50/50
