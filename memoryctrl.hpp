#ifndef MEMORYCTRL_H
#define MEMORYCTRL_H

#include <new>
#include <memory>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>

namespace memctrl {

enum class ErrorType {
  no_error,
  out_of_range,
  null_ponter
};

class Error {
  ErrorType err_type;
public:
  Error(ErrorType err_type = ErrorType::no_error) noexcept : err_type(err_type) {}
  ErrorType operator=(ErrorType err_type) noexcept {return this->err_type = err_type;}
  const char* what() const noexcept {
    switch (err_type) {
      case ErrorType::no_error: return "";
      case ErrorType::out_of_range: return "Out of range";
      case ErrorType::null_ponter: return "Null pointer";
    }
  }
  operator ErrorType() const noexcept {return err_type;}
  operator bool() const noexcept {return err_type != ErrorType::no_error;}
};

class BufferController {
  uint8_t* data = nullptr;
  size_t size = 0;
  size_t capacity = 0;

  // Needed for calculate capacity
  static size_t getNearestPow2(size_t num) noexcept {
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num |= num >> 32;
    num++;
    return num;
    // Another option to implement the function:
    //  *reinterpret_cast<double*>(&num) = num;
    //  return 1 << (((*(reinterpret_cast<uint32_t*>(&num) + 1) & 0x7FF00000) >> 20) - 1022);
  }

public:

  typedef uint8_t byte;
  typedef uint8_t* iterator;
  typedef const uint8_t* const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  BufferController() noexcept = default;

  BufferController(size_t size) noexcept
    : data(new uint8_t[getNearestPow2(size)]),
      size(size),
      capacity(getNearestPow2(size)) {}

  BufferController(void* buffer, size_t size) noexcept
    : data(new uint8_t[getNearestPow2(size)]),
      size(size),
      capacity(getNearestPow2(size)) {memcpy(data, buffer, size);}

  BufferController(BufferController& other) noexcept
    : data(new uint8_t[other.size]),
      size(other.size),
      capacity(getNearestPow2(other.capacity)) {}

  BufferController(BufferController&& other) noexcept
    : data(other.data),
      size(other.size),
      capacity(other.capacity) {other.data = nullptr;}

  template<typename T>
  BufferController(std::initializer_list<T> data_list)
    : data(new uint8_t[getNearestPow2(data_list.size() * sizeof (T))]),
      size(data_list.size() * sizeof (T)),
      capacity(getNearestPow2(data_list.size() * sizeof (T))) {
    T* it = begin<T>();
    for(auto& element : data_list) {
      *it = std::move(element);
      it += sizeof (T);
    }
  }

  BufferController(std::initializer_list<BufferController> data_list) {
    for(auto& element : data_list) capacity += element.size;
    data = new uint8_t[capacity = getNearestPow2(capacity)];
    for(auto& element : data_list) pushBack(std::move(element), static_cast<Error*>(nullptr));
  }

  ~BufferController() {clear();}

  static BufferController move(const void* buffer, size_t size) noexcept {
    BufferController ctrl;
    ctrl.data = (uint8_t*)buffer;
    ctrl.size = size;
    ctrl.capacity = size;
    return ctrl;
  }

  bool isEmpty() const noexcept {return !size;}
  bool isCapacityEmpty() const noexcept {return !data || !capacity;}
  void* getData() const noexcept {return data;}

  size_t getSize() const noexcept {return size;}
  size_t getCapacity() const noexcept {return capacity;}
  template<typename T>
  size_t getCount() const noexcept {return size/sizeof(T);}
  template<typename T>
  size_t getCapacity() const noexcept {return capacity/sizeof (T);}

  void clear() noexcept {if(data) delete[] data; size = 0; capacity = 0;}

  void resize(size_t new_size) noexcept {
    if(size == new_size) return;
    else if(size >= new_size || capacity >= new_size) {
      size = new_size;
      return;
    } else {
      capacity = getNearestPow2(new_size);
      data = (uint8_t*)realloc(data, capacity);
      size = new_size;
      return;
    }
  }

  template<typename T>
  void resize(size_t count) noexcept {return resize(count * sizeof(T));}

  void reserve(size_t new_capacity) noexcept {
    if(capacity >= new_capacity) return;
    capacity = getNearestPow2(new_capacity);
    data = (uint8_t*)realloc(data, capacity);
    if(capacity < size) size = capacity;
  }

  template<typename T>
  void reserve(size_t count) noexcept {return reserve(count * sizeof (T));}

  void shrinkToFit() noexcept {
    if(size == capacity) return;
    capacity = size;
    data = (uint8_t*)realloc(data, capacity);
  }

  void subSizeBack(size_t sub) noexcept {return resize(size - sub);}

  void subSizeFront(size_t sub) noexcept {
    memmove(data, data + sub, size - sub);
    return resize(size - sub);
  }

  void subSizeFrom(size_t at, size_t sub) noexcept {
    memmove(data + at, data + at + sub, size - sub - at);
    return resize(size - sub);
  }

  iterator addSizeToBack(size_t add) noexcept {
    size_t old_size = size;
    resize(size + add);
    return data + old_size;
  }

  iterator addSizeToFront(size_t add) noexcept {
    size_t old_size = size;
    addSizeToBack(add);
    memmove(data + add, data, old_size);
    return data;
  }

  iterator addSizeTo(size_t to, size_t add) noexcept {
    size_t old_size = size;
    addSizeToBack(add);
    iterator it = data + to;
    memmove(it + add, it, old_size - add);
    return it;
  }

  iterator pushBack(const void* data, size_t size, Error* err = nullptr) noexcept {
    if(!data) {
      if(err) *err = ErrorType::null_ponter;
      return end();
    }
    auto data_it = addSizeToBack(size);
    memmove(data_it, data, size);
    return data_it;
  }

  iterator insert(size_t to, const void* data, size_t size, Error* err = nullptr) noexcept {
    if(!data) {
      if(err) *err = ErrorType::null_ponter;
      return end();
    }
    if(to > size) {
      if(err) *err = ErrorType::out_of_range;
      return end();
    }
    auto data_it = addSizeTo(to, size);
    memmove(data_it, data, size);
    return data_it;
  }

  iterator pushFront(const void* data, size_t size, Error* err = nullptr) noexcept {
    if(!data) {
      if(err) *err = ErrorType::null_ponter;
      return end();
    }
    auto data_it = addSizeToFront(size);
    memmove(data_it, data, size);
    return data_it;
  }

  iterator pushBack(const BufferController& other, Error* err = nullptr) noexcept {
    return pushBack(other.data, other.size, err);
  }

  iterator pushBack(BufferController&& other, Error* err = nullptr) noexcept {
    return pushBack(other.data, other.size, err);
  }

  iterator insert(size_t to, const BufferController& other, Error* err = nullptr) noexcept {
    return insert(to, other.data, other.size, err);
  }

  iterator insert(size_t to, BufferController&& other, Error* err = nullptr) noexcept {
    return insert(to, other.data, other.size, err);
  }

  iterator pushFront(const BufferController& other, Error* err = nullptr) noexcept {
    return pushFront(other.data, other.size, err);
  }

  iterator pushFront(BufferController&& other, Error* err = nullptr) noexcept {
    return pushFront(other.data, other.size, err);
  }

  template<typename T, typename... Args>
  T* emplaceBack(Args&&... args) noexcept {
    return reinterpret_cast<T*>(new (addSizeToBack(sizeof (T))) T(std::forward<Args>(args)...));
  }

  template<typename T, typename... Args>
  T* emplaceAt(size_t index, size_t shift, Args&&... args, Error* err = nullptr) noexcept {
    if((index * sizeof (T) + shift) > size) {
      if(err) *err = ErrorType::out_of_range;
      return end<T>();
    }
    return reinterpret_cast<T*>(new (addSizeTo(index * sizeof (T) + shift, sizeof (T))) T(std::forward<Args>(args)...));
  }

  template<typename T, typename... Args>
  T* emplaceFront(Args&&... args) noexcept {
    return reinterpret_cast<T*>(new (addSizeToFront(sizeof (T))) T(std::forward<Args>(args)...));
  }

  template<typename T>
  void destruct(size_t at, size_t shift, size_t count = 1, Error* err = nullptr) noexcept {
    if((at * sizeof (T) + shift + count * sizeof (T)) >= size) {
      if(err) *err = ErrorType::out_of_range;
      return;
    }
    for(T* it = reinterpret_cast<T*>(data + shift) + at,
        * end = reinterpret_cast<T*>(data + shift) + at + count;
        it != end; ++it)
      it->~T();
  }

  template<typename T>
  void destructFirst() noexcept {
    return destruct<T>(0, 0, 1, nullptr);
  }

  template<typename T>
  void destructLast() noexcept {
    return destruct<T>(getCount<T>() - 1, 0, 1, nullptr);
  }

  template<typename T>
  void destructAll() noexcept {
    return destruct<T>(0, 0, getCount<T>(), nullptr);
  }

  template<typename T>
  T* pushBack(const T& value) noexcept {return reinterpret_cast<T*>(pushBack(&value, sizeof (T)));}

  template<typename T>
  T* pushBack(T&& value) noexcept {return emplaceBack<T>(std::move(value));}

  template<typename T>
  T* insert(size_t index, size_t shift, const T& value, Error* err = nullptr) noexcept {
    return reinterpret_cast<T*>(insert(index * sizeof (T) + shift, &value, sizeof (T), err));
  }

  template<typename T>
  T* insert(size_t index, size_t shift, T&& value, Error* err = nullptr) noexcept {
    return emplaceAt<T>(index, shift, std::move(value), err);
  }

  template<typename T>
  T* pushFront(const T& value) noexcept {return reinterpret_cast<T*>(pushFront(&value, sizeof (T)));}

  template<typename T>
  T* pushFront(T&& value) noexcept {return emplaceFront<T>(std::move(value));}

  void remove(size_t at, size_t count = 1, Error* err = nullptr) noexcept {
    if(!at && count == size) return resize(0);
    if(at + count > size) {
      if(err) *err = ErrorType::out_of_range;
      return;
    }
    memmove(data + at, data + at + size, size - count - at);
    subSizeBack(at);
  }

  template<typename T>
  void remove(size_t at, size_t shift, size_t count = 1, Error* err = nullptr) noexcept {
    return remove(at * sizeof(T) + shift, count, err);
  }

  byte& get(size_t at, Error* err = nullptr) const noexcept {
    if(at >= size) {
      if(err) *err = ErrorType::out_of_range;
      return data[size - 1];
    }
    return data[at];
  }

  template<typename T>
  T& get(size_t at, size_t shift, Error* err = nullptr) const noexcept {
    return *reinterpret_cast<T*>(&get(at * sizeof (T) + shift, err));
  }

  byte& first(Error* err = nullptr) const noexcept {
    if(!size) {
      if(err) *err = ErrorType::null_ponter;
      return get(0);
    }
    return get(0);
  }

  template<typename T>
  T& first(Error* err = nullptr) const noexcept {
    if(!size) {
      if(err) *err = ErrorType::null_ponter;
      return get<T>(0,0);
    }
    return get<T>(0,0);
  }

  byte& last(Error* err = nullptr) const noexcept {
    if(!size) {
      if(err) *err = ErrorType::null_ponter;
      return get(0);
    }
    return get(size - 1);
  }

  template<typename T>
  T& last(Error* err = nullptr) const noexcept {
    if(!size) {
      if(err) *err = ErrorType::null_ponter;
      return get<T>(0,0);
    }
    return get<T>(getCount<T>() - 1, 0);
  }

  BufferController takeBack(size_t size, Error* err) noexcept {
    if(!this->size) {
      if(err) *err = ErrorType::null_ponter;
      return BufferController();
    }
    if(size > this->size) {
      if(err) *err = ErrorType::out_of_range;
      return BufferController();
    }
    BufferController new_data(size);
    memcpy(new_data.data, end() - size, size);
    subSizeBack(size);
    return new_data;
  }

  BufferController takeFront(size_t size, Error* err) noexcept {
    if(!this->size) {
      if(err) *err = ErrorType::null_ponter;
      return BufferController();
    }
    if(size > this->size) {
      if(err) *err = ErrorType::out_of_range;
      return BufferController();
    }
    BufferController new_data(size);
    memcpy(new_data.data, data, size);
    subSizeFront(size);
    return new_data;
  }

  BufferController takeFrom(size_t at, size_t size, Error* err) noexcept {
    if(!this->size) {
      if(err) *err = ErrorType::null_ponter;
      return BufferController();
    }
    if(at + size > this->size) {
      if(err) *err = ErrorType::out_of_range;
      return BufferController();
    }
    BufferController new_data(size);
    memcpy(new_data.data, data + at, size);
    subSizeFrom(at, size);
    return new_data;
  }

  iterator begin() const noexcept {return data;}
  iterator end() const noexcept {return data + size;}

  iterator cbegin() const noexcept {return data;}
  iterator cend() const noexcept {return data + size;}

  reverse_iterator rbegin() const noexcept {return reverse_iterator(data + size - 1);}
  reverse_iterator rend() const noexcept {return reverse_iterator(data - 1);}

  const_reverse_iterator crbegin() const noexcept {return const_reverse_iterator(data + size - 1);}
  const_reverse_iterator crend() const noexcept {return const_reverse_iterator(data - 1);}

  template<typename T>
  T* begin() const noexcept {return reinterpret_cast<T*>(data);}
  template<typename T>
  T* end() const noexcept {return reinterpret_cast<T*>(data) + getCount<T>();}

  template<typename T>
  const T* cbegin() const noexcept {return reinterpret_cast<T*>(data);}
  template<typename T>
  const T* cend() const noexcept {return reinterpret_cast<T*>(data) + getCount<T>();}

  template<typename T>
  std::reverse_iterator<T*> rbegin() const noexcept {
    return std::reverse_iterator<T*>(reinterpret_cast<T*>(reinterpret_cast<T*>(data) + getCount<T>() - 1));
  }
  template<typename T>
  std::reverse_iterator<T*> rend() const noexcept {
    return std::reverse_iterator<T*>(reinterpret_cast<T*>(reinterpret_cast<T*>(data) - 1));
  }

  template<typename T>
  std::reverse_iterator<const T*> crbegin() const noexcept {
    return std::reverse_iterator<const T*>(reinterpret_cast<T*>(reinterpret_cast<T*>(data) + getCount<T>() - 1));
  }
  template<typename T>
  std::reverse_iterator<const T*> crend() const noexcept {
    return std::reverse_iterator<const T*>(reinterpret_cast<T*>(reinterpret_cast<T*>(data) - 1));
  }

  byte& operator[](size_t index) const noexcept {return get(index);}

  BufferController& operator+=(const BufferController& other) noexcept {
    pushBack(other);
    return *this;
  }

  BufferController operator+=(BufferController&& other) noexcept {
    pushBack(std::move(other));
    return *this;
  }

  BufferController operator+(const BufferController& other) noexcept {
    BufferController tmp(*this);
    tmp.pushBack(other);
    return tmp;
  }

  BufferController operator+(BufferController&& other) noexcept {
    BufferController tmp(*this);
    tmp.pushBack(std::move(other));
    return tmp;
  }

  BufferController& operator=(const BufferController& other) noexcept {
    resize(0);
    pushBack(other);
    return *this;
  }

  BufferController operator=(BufferController&& other) noexcept {
    resize(0);
    pushBack(std::move(other));
    return *this;
  }

  bool operator==(const BufferController& other) noexcept {
    if(size != other.size) return false;
    for(iterator f_it = begin(),
                 s_it = other.begin(),
                 f_end = end();
        f_it != f_end; (++f_it, ++s_it))
      if(*f_it != *s_it) return false;
    return true;
  }

  bool operator==(BufferController&& other) noexcept {
    if(size != other.size) return false;
    for(iterator f_it = begin(),
                 s_it = other.begin(),
                 f_end = end();
        f_it != f_end; (++f_it, ++s_it))
      if(*f_it != *s_it) return false;
    return true;
  }

  bool operator!=(const BufferController& other) noexcept {return !(*this == other);}

  bool operator!=(BufferController&& other) noexcept {return !(*this == std::move(other));}

};


template<>
BufferController* BufferController::pushBack<BufferController>(const BufferController& value) noexcept {
  pushBack(value, static_cast<Error*>(nullptr));
  return this;
}

template<>
BufferController* BufferController::pushBack(BufferController&& value) noexcept {
  pushBack(std::move(value), static_cast<Error*>(nullptr));
  return this;
}

template<>
BufferController* BufferController::insert(size_t index, size_t shift, const BufferController& value, Error* err) noexcept {
  insert(index + shift, value.data, value.size, err);
  return this;
}

template<>
BufferController* BufferController::insert(size_t index, size_t shift, BufferController&& value, Error* err) noexcept {
  insert(index + shift, value.data, value.size, err);
  return this;
}

template<>
BufferController* BufferController::pushFront(const BufferController& value) noexcept {
  pushFront(value, static_cast<Error*>(nullptr));
  return this;
}

template<>
BufferController* BufferController::pushFront(BufferController&& value) noexcept {
  pushFront(std::move(value), static_cast<Error*>(nullptr));
  return this;
}

template<>
const BufferController* BufferController::pushBack(const BufferController&& value) noexcept {
  pushBack(std::move(value), static_cast<Error*>(nullptr));
  return this;
}

template<>
const BufferController* BufferController::insert(size_t index, size_t shift, const BufferController&& value, Error* err) noexcept {
  insert(index + shift, value.data, value.size, err);
  return this;
}

template<>
const BufferController* BufferController::pushFront(const BufferController&& value) noexcept {
  pushFront(std::move(value), static_cast<Error*>(nullptr));
  return this;
}


template<typename T>
class TypedInterface {
  BufferController& buffer;
public:

  typedef T Type;
  typedef T* iterator;
  typedef const T* const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  TypedInterface(BufferController& buffer) noexcept : buffer(buffer) {}

  void resize(size_t count) {return buffer.resize<T>(count);}
  void reserve(size_t count) {return buffer.reserve<T>(count);}

  size_t getCount() const noexcept {return buffer.getCount<T>();}

  template<typename... Args>
  T* emplaceBack(Args&&... args) noexcept {
    return buffer.emplaceBack(args...);
  }

  template<typename... Args>
  T* emplaceAt(size_t index, size_t shift, Args&&... args, Error* err = nullptr) noexcept {
    return buffer.emplaceAt(index, shift, args..., err);
  }

  template<typename... Args>
  T* emplaceFront(Args&&... args) noexcept {
    return buffer.emplaceFront(args...);
  }

  void destruct(size_t at, size_t shift, size_t count = 1, Error* err = nullptr) noexcept {
    return buffer.destruct<T>(at, shift, count, err);
  }

  T* pushBack(const T& value) noexcept {return buffer.pushBack<T>(&value, sizeof (T));}

  T* pushBack(T&& value) noexcept {return buffer.emplaceBack<T>(std::move(value));}

  T* insert(size_t index, size_t shift, const T& value, Error* err = nullptr) noexcept {
    return buffer.insert(index, shift, value, err);
  }

  T* insert(size_t index, size_t shift, T&& value, Error* err = nullptr) noexcept {
    return buffer.emplaceAt<T>(index, shift, std::move(value), err);
  }

  T* pushFront(const T& value) noexcept {return buffer.pushBack(value);}

  T* pushFront(T&& value) noexcept {return buffer.emplaceFront<T>(std::move(value));}

  iterator begin() const noexcept {return buffer.begin<T>();}
  iterator end() const noexcept {return buffer.end<T>();}

  const_iterator cbegin() const noexcept {return buffer.cbegin<T>();}
  const_iterator cend() const noexcept {return buffer.cend<T>();}

  reverse_iterator rbegin() const noexcept {return buffer.rbegin<T>();}
  reverse_iterator rend() const noexcept {return buffer.rend<T>();}

  const_reverse_iterator crbegin() const noexcept {return buffer.crbegin<T>();}
  const_reverse_iterator crend() const noexcept {return buffer.crend<T>();}
};

}

#endif // MEMORYCTRL_H
