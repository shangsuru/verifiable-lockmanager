#pragma once

#include <vector>

template <typename V>
class Entry {
 public:
  int key;
  V val;
  Entry(int k, const V &v) : val(v), key(k) {}
};

template <typename V>
class HashTable {
 public:
  /**
   * The hashtable implementation:
   * 1. Hash function:
   * Since only a numeric key is used, we just take modulo
   * the size of the hashtable to determine its index.
   *
   * 2. Collisions:
   * We resolve collisions using Chaining, that means, every entry is mapped to
   * a bucket. Collisions are stored in the same bucket. Internally the bucket
   * is implemented as a linked list.
   * @param size the number of buckets for the hashtable.
   */
  HashTable(int size);

  void set(int key, const V &val);

  /**
   * @returns nullptr, when the value couldn't be found
   */
  auto get(int key) -> V;

  auto contains(int key) -> bool;

  void erase(int key);

 private:
  std::vector<std::vector<Entry<V>>> table;
};

template <typename V>
HashTable<V>::HashTable(int size) {
  for (int i = 0; i < size; i++) {
    std::vector<Entry<V>> v;
    table.push_back(v);
  }
}

template <typename V>
void HashTable<V>::set(int key, const V &val) {
  Entry<V> new_entry(key, val);
  for (int i = 0; i < table[key % table.size()].size(); i++) {
    if (table[key % table.size()][i].key == key) {
      table[key % table.size()][i] = new_entry;
      return;
    }
  }
  // Key doesn't exist yet
  table[key % table.size()].push_back(new_entry);
}

template <typename V>
auto HashTable<V>::get(int key) -> V {
  for (int i = 0; i < table[key % table.size()].size(); i++) {
    if (table[key % table.size()][i].key == key) {
      return table[key % table.size()][i].val;
    }
  }

  return nullptr;
}

template <typename V>
auto HashTable<V>::contains(int key) -> bool {
  std::vector<Entry<V>> entries = table[key % table.size()];
  for (int i = 0; i < entries.size(); i++) {
    if (entries[i].key == key) {
      return true;
    }
  }
  return false;
}

template <typename V>
void HashTable<V>::erase(int key) {
  remove_if(table[key % table.size()].begin(), table[key % table.size()].end(),
            [key](auto e) { return e.key == key; });
}