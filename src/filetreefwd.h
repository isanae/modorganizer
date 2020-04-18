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
using FileIndex = unsigned int;

class Provider;
class VirtualProvider;


class lowercase_wstring_view : public std::wstring_view
{
public:
  using std::wstring_view::wstring_view;

  lowercase_wstring_view(const std::wstring& s)
    : std::wstring_view(s.c_str(), s.size())
  {
  }
};

} // namespace

#endif // MODORGANIZER_FILETREEFWD_INCLUDED
