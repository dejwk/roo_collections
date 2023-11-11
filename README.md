# roo_collections
Small, flat, memory-conscious hashtable for microcontrollers (e.g. Arduino projects).

Small flat hashtable (and hash set, hash map) minimize the use of heap. All items are kept in a single array, which tends to be ~2x the minimum size needed to store all the elements. It is ideal for small, frequently accessed hash containers, particularly when element deletion is infrequent.

Hashtable authors obsess about performance. These hashtables have excellent performance at small sizes (up to ~5000 elements in a Linux benchmark), and decent performance outside of that range - but more importantly, their implementation is *simple* - they will be easy on your microcontroller image sizes.