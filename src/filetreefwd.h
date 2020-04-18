#ifndef MODORGANIZER_FILETREEFWD_INCLUDED
#define MODORGANIZER_FILETREEFWD_INCLUDED

#include <memory>

namespace filetree
{

class Tree;
class Model;
class Item;
using ItemPtr = std::unique_ptr<Item>;

class Directory;
class File;

class Provider;
class VirtualProvider;

} // namespace

#endif // MODORGANIZER_FILETREEFWD_INCLUDED
