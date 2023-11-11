# roo_collections
Small, flat, memory-conscious hashtable for microcontrollers (e.g. Arduino projects).

Flat hashtable (and hash set, hash map) minimize the use of heap. All items are kept in a single array, which tends to be ~2x the minimum size needed to store all the elements. It is ideal for write-once, frequently accessed hash containers.
