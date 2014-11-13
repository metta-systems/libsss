## Lifetimes of the objects in the SSS library

`host` - Outlives all the objects. they all keep a shared_ptr to the host object, effectively making it live as long as there are any objects using it. (**@todo** watch out for reference cycles!)

`base_stream::parent` - Streams may outlive their parents, that's why the parent pointer is a weak_ptr. At some point the parent stream may be gone.

http://programmers.stackexchange.com/questions/133302/stdshared-ptr-as-a-last-resort
http://msdn.microsoft.com/en-us/library/vstudio/hh279669.aspx
http://www.umich.edu/~eecs381/handouts/C++11_smart_ptrs.pdf

Use `std::make_shared<T>()`
