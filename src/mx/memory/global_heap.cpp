#include "global_heap.h"

Genode::Heap mx::memory::GlobalHeap::_heap {system::Environment::env->ram(), system::Environment::env->rm()};