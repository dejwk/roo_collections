# roo_collections
Small, flat, fast, memory-conscious hash collections (hashtable, hashmap, hashset) for microcontrollers (e.g. in Arduino projects). Tested on ESP32 (Arduino and esp-idf) and Raspberry Pi Pico.

These collections keep the items in a single array, which tends to be ~2x the minimum size needed to store all the elements. It is ideal for small, frequently accessed hash containers, particularly when element deletion is infrequent.

These nice properties are achieved by sacrificing element address stability. That is, an address of an element added to the hashtable gets invalidated by subsequent unrelated inserts or deletions (as elements may be moved around during rehashing).

Hashtable authors obsess about performance. These hashtables have excellent performance at small sizes (up to ~5000 elements in a Linux benchmark), and decent performance outside of that range - but more importantly, their implementation is *simple* - they will be easy on your microcontroller image sizes.

These collections play nicely with strings. You can use strings as both keys and values. If your strings are constants (e.g. defined as compile-time `const char*` constants), you can use `roo::string_view` which ensures that no copies are made. If your lookup key is of a different string type
than the storage key, e.g. if you use `std::string` keys but use `const char*` or `roo::string_view` for the lookup, no dynamic allocation is done; the internal comparator functions work with heterogeneous types. Additionally, the library contains a templated SmallString implementation
which can be used to store small fixed-size strings inline (without using heap).
