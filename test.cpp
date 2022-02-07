#include <iostream>
#include "memoryctrl.hpp"

using namespace std;


int main() {
  using namespace memctrl;

  BufferController buffer;
  buffer.reserve<int>(128);
  std::clog << "Size: " << buffer.getSize() << "\n"
               "Capacity: " << buffer.getCapacity() << '\n';
  for(int i = 0; i < 512; ++i) {
    buffer.pushBack(i);
    std::clog << "Size: " << buffer.getSize() << "\n"
                 "Capacity: " << buffer.getCapacity() << '\n';
  }

  buffer += buffer;
  std::clog << "Size: " << buffer.getSize() << "\n"
               "Capacity: " << buffer.getCapacity() << '\n';

  for(auto& element : TypedInterface<int>(buffer)) {
    std::clog << element << " ";
  }
  std::clog << std::endl;

  return 0;
}
