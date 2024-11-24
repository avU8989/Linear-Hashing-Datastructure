#ifndef ADS_SET_H
#define ADS_SET_H

#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>

template <typename Key, size_t N = 3>
class ADS_set {
public:
    class Iterator;
    using value_type = Key;
    using key_type = Key;
    using reference = key_type &;
    using const_reference = const key_type &;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using key_compare = std::less<key_type>;    // B+-Tree
    using key_equal = std::equal_to<key_type>;  // Hashing
    using hasher = std::hash<key_type>;         // Hashing

private:
enum class Mode {free, used, freeagain};
struct element{
    key_type key; 
    Mode mode{Mode :: free};
};

struct bucket{
    element* block{nullptr};
    bucket* overflow{nullptr};
    size_type current{0};

    bucket() :block{new element[N]}, overflow{nullptr}, current{0}{
    }


    bucket(bucket&& obj) : block{obj.block}, overflow{obj.overflow}, current{obj.current}{
        obj.block = nullptr;
        obj.overflow = nullptr;
        obj.current = 0;
    }

    size_type erase(const key_type& key){
        for(size_type i{0}; i < current; ++i){
            if(key_equal{}(key, block[i].key)){
                if (i != current--){
                    block[i] = block[current];
                    block[current].key = {};
                    block[current].mode = Mode::freeagain;
                }
                return 1;
            }
        }
        return 0;
    }

    element* find(const key_type &key) const {
        if(block == nullptr || current == 0){
            return nullptr;
        }

        for (size_t i {0}; i < N; ++i){
            if(block[i].mode == Mode::used){
                if (key_equal{}(key, block[i].key)) 
                    return &block[i]; // values+i  
            }
        }
            
    return nullptr;
    }

  element* append(const key_type& key){
       return current == N ? nullptr : &(block[current++].key = key, block[current++].mode = Mode::used);
    }

    void swap(bucket& other){
        using std::swap;

        swap(block, other.block);
        swap(overflow, other.overflow);
        swap(current, other.current);
    }
    bucket& operator=(bucket& obj){
       bucket tmp{obj};
       swap(tmp);

       return *this;
    }

    ~bucket() {
        delete[] block;
        delete overflow;
    }
};

bucket** table{nullptr};
size_type curr_size{0};
size_type table_size{0};
size_type nts{0}; //next to split
size_type d{1}; //rundennummer

void split();
void insert_(const key_type& key);
element* find_(const key_type& key) const;
void rehash_split(size_type idx);
void rehash_p(size_type idx);
void rehash_round();
bucket* search_bucket(const key_type& key) const;
void insert_nosplit(const key_type& key);
void allocate_overflow(const key_type& key);
void insert_overflow(const key_type& key);

size_type find_idx (const key_type& key) const{
    size_type idx = h(key);
    if(idx < nts){
        idx = h1(key);
    }
    return idx;
}

size_type h(const key_type& key) const{
    return hasher{}(key)%(1<<d);
}

size_type h1(const key_type& key) const{
    return hasher{}(key)%(1<<(d+1));
}

public:
    ADS_set() : curr_size{0}, nts{0}, d{1}{
        table_size = (1<<d);
        table = new bucket*[table_size];
        
        for(size_type i{0}; i < (1<<d)+nts; ++i){
            table[i] = new bucket;  
            }
    }
    // PH1
    ADS_set(std::initializer_list<key_type> ilist): ADS_set{} {
        insert(ilist);
    }                      // PH1

    
    template<typename InputIt> ADS_set(InputIt first, InputIt last): ADS_set{}{
        insert(first, last);
    }     // PH1

    ADS_set(const ADS_set &other): ADS_set() {
        for(const auto& k : other){
            insert_(k);
        }
/*
        for(size_type i{0}; i < other.table_size; ++i){
            table[i] = other.table[i];
        }
        */
    }
  
    ~ADS_set(){
        for(size_type i{0}; i < (1<<d)+nts; i++){
            if(table[i] != nullptr){
                delete table[i];
                table[i] = nullptr;
            }
        }
        delete[] table;
    }

    ADS_set &operator=(const ADS_set &other){
        if(this == &other){
            return *this;
        }
        ADS_set tmp{other};
        swap(tmp);
        return *this;

    }
    ADS_set &operator=(std::initializer_list<key_type> ilist){
        ADS_set tmp{ilist};
        swap(tmp);
        return *this;
    }

    size_type size() const{
        return curr_size;
    }
    // PH1
    bool empty() const{
        return !curr_size;
    }                                                  // PH1

    void insert(std::initializer_list<key_type> ilist){
        insert(std::begin(ilist), std::end(ilist));
    }                  // PH1

    std::pair<iterator,bool> insert(const key_type &key){
        if(auto p{find_(key)}){
            size_type idx{find_idx(key)};
            return {iterator{table, p, idx, static_cast<size_type>((1<<d)+nts)}, false};
        }
        else{
            insert_(key);
            size_type idx{find_idx(key)};
            return {iterator{table, find_(key), idx, static_cast<size_type>((1<<d)+nts)}, true};
        }
    }


    template<typename InputIt> void insert(InputIt first, InputIt last); // PH1
    
    void clear(){
        ADS_set tmp{};
        swap(tmp);
    }
  
    size_type erase(const key_type &key){
        bucket* help = search_bucket(key);

        if(help == nullptr || help->current == 0){
            return 0;
        }

        if(help != nullptr){
            help->erase(key);
            --curr_size;
            return 1;
        }
        
        
        return 0;
    }

    size_type count(const key_type &key) const{
        return !!find_(key);
    } 
                             // PH1
    iterator find(const key_type &key) const{
        if(auto p{find_(key)}){
            size_type idx{find_idx(key)};
            return iterator{table, p, idx, static_cast<size_type>((1<<d)+nts)};
        }

        return end();
    }

    void swap(ADS_set &other){
        using std::swap;

        swap(table, other.table);
        swap(curr_size, other.curr_size);
        swap(d, other.d);
        swap(nts, other.nts);

    }

    const_iterator begin() const{
        return const_iterator{table, static_cast<size_type>((1<<d)+nts)};
    }

    const_iterator end() const{
        return const_iterator{};
    }

    void dump(std::ostream &o = std::cerr) const{
        o << "curr_size = " << curr_size << ", table_size = " << table_size << "\n";
        o << "blocksize = " << N  << ", next_to_split = " << nts << ", rundennummer = " << d << "\n";

        for(size_type i{0}; i < ((1<<d)+nts); i++){
            o << i << ": ";

        if(table[i] == nullptr){
            o << "NULL";
        }   
        
        if(table[i] != nullptr){
                    bucket* help = table[i];
                    while(help != nullptr){
                        o << "{current = " << help->current << "} ";
                        for(size_type k{0};k<N;++k){ 
                                if(help->block[k].mode == Mode ::used){
                                    o << "(Mode::used), ";    
                                    o << help->block[k].key << " , ";
                                }
                                if(help->block[k].mode == Mode ::free){
                                    o << "(Mode::free), ";
                                }
                                if(help->block[k].mode == Mode ::freeagain){
                                    o << "(Mode::freeagain), ";
                                }

                            }
                    help = help->overflow;
                    if(help != nullptr){
                         o << "overflow: ";
                    }  
                }
        }
      
            o << "\n";
        }
    }
    
    friend bool operator==(const ADS_set &lhs, const ADS_set &rhs){
        if(lhs.curr_size != rhs.curr_size){
            return false;
        }

        for (const auto &k: rhs){
            if(!lhs.count(k)){
                return false;
            }
        }

        return true;
    }
    friend bool operator!=(const ADS_set &lhs, const ADS_set &rhs){
         return !(lhs == rhs);
    }
};

template<typename Key, size_t N>
void ADS_set<Key, N> :: insert_(const key_type& key){
    size_type idx = find_idx(key);
    if(table[idx] == nullptr){
        table[idx] = new bucket;
        table[idx]->block[table[idx]->current].key = key;
        table[idx]->block[table[idx]->current].mode = Mode :: used;
        ++table[idx]->current;
        }
        
        if(table[idx] != nullptr){
            if(table[idx]->current == 0 || table[idx]->current != N){
                    table[idx]->block[table[idx]->current].key = key;
                    table[idx]->block[table[idx]->current].mode = Mode :: used;
                    ++table[idx]->current;
            }
            else{
                if(table[idx]->overflow != nullptr){
                    bucket* help = table[idx];
                    while(help->overflow != nullptr && help->current == N){
                        help = help->overflow;
                    }           
                    if(help == nullptr){
                        allocate_overflow(key);
                    }
                    else{
                        insert_overflow(key);
                    }

                    if(help->current == N){
                        split();
                        idx = find_idx(key);
                    }
                }
                else{
                    split();
                    idx = find_idx(key);
                    if(table[idx]->current != N){
                        table[idx]->block[table[idx]->current].key = key;
                        table[idx]->block[table[idx]->current].mode = Mode :: used;
                        ++table[idx]->current;
                    }
                    else{
                        if(table[idx]->overflow == nullptr){
                            allocate_overflow(key);
                        }
                    }
                }
            }
        }
        ++curr_size;        
}


template<typename Key, size_t N>
void ADS_set<Key, N> :: insert_nosplit(const key_type& key){
size_type idx {find_idx(key)};

    if(table[idx] == nullptr){
        table[idx] = new bucket;
        table[idx]->block[table[idx]->current].key = key;
        table[idx]->block[table[idx]->current].mode = Mode :: used;
        ++table[idx]->current;
    }else{
        if(table[idx]->current != N || table[idx]->current == 0){
            table[idx]->block[table[idx]->current].key = key;
            table[idx]->block[table[idx]->current].mode = Mode :: used;
            ++table[idx]->current;
        }else{
            if(!find_(key)){
                if(table[idx]->overflow == nullptr){
                    allocate_overflow(key);
                }
                else{
                    insert_overflow(key);
                }
            }
        }
    }
}

template<typename Key, size_t N>
void ADS_set<Key, N> :: allocate_overflow(const key_type& key){
    size_type idx{find_idx(key)};
    table[idx]->overflow = new bucket;
    table[idx]->overflow->block[table[idx]->overflow->current].key = key;
    table[idx]->overflow->block[table[idx]->overflow->current].mode = Mode::used;
    ++table[idx]->overflow->current;
}

template<typename Key, size_t N>
void ADS_set<Key, N> :: insert_overflow(const key_type& key){
    size_type idx{find_idx(key)};
    if(table[idx]->overflow->current != N || table[idx]->current == 0){
        table[idx]->overflow->block[table[idx]->overflow->current].key = key;
        table[idx]->overflow->block[table[idx]->overflow->current].mode = Mode :: used;
        ++table[idx]->overflow->current;
        }
        else{
            bucket* new_bucket = new bucket;
            new_bucket->block[new_bucket->current].key = key;
            new_bucket->block[new_bucket->current].mode = Mode::used;
            new_bucket->overflow = table[idx]->overflow;
            ++new_bucket->current;
            table[idx]->overflow = new_bucket;
            }
        
}



template<typename Key, size_t N>
void ADS_set<Key, N> :: split(){
        size_type round = 1<<d;
        size_type old_nts{nts};
        ++nts;  

        if(table_size > (1<<d)+nts){
            //table is big enough
            rehash_p(old_nts);
        }else{
            //table is not big enough
            rehash_split(old_nts);
        }
       
        if(nts == round){
            ++d;
            nts = 0;
            rehash_round();            
        }
}

template<typename Key, size_t N>
void ADS_set<Key, N> :: rehash_round(){
    table_size = table_size*2;
    size_type round = 1<<d;
    bucket** new_table{new bucket*[(1<<(d+1))+1]};
    for(size_type i{0};i < round;++i){
        new_table[i] = table[i];
    }
    delete[] table;
    table = new_table;
}

template<typename Key, size_t N>
void ADS_set<Key, N> :: rehash_p(size_type old_nts){
    bucket* tmp = table[old_nts];
    bucket* help = table[old_nts];
    table[old_nts] = new bucket;
    table[(1<<d)+old_nts] = new bucket;
    while(tmp != nullptr){
        for(size_type i{0}; i < N; ++i){
            if(tmp->block[i].mode == Mode::used){
                insert_nosplit(tmp->block[i].key);
            }
        }
        tmp = tmp->overflow;
    }
    delete help;
}


template<typename Key, size_t N>
void ADS_set<Key, N> :: rehash_split(size_type old_nts){
    bucket** old_table{table};
    ++table_size;
    bucket** new_table{new bucket*[table_size]};    
    bucket* tmp = old_table[old_nts];
    for(size_type i{0};i < ((1<<d)+old_nts);++i){
           if(i == old_nts){
               new_table[i] = new bucket;
           }else{
               new_table[i] = table[i];            
           }
    }
    table = new_table;
    table[(1<<d)+old_nts] = new bucket;

        while(tmp != nullptr){
            for(size_type j{0}; j < N; ++j){
                if(tmp->block[j].mode == Mode::used){
                    insert_nosplit(tmp->block[j].key);
                }
            }
                tmp = tmp->overflow;
  
        }
        delete old_table[old_nts];
        delete[] old_table;

}

template<typename Key, size_t N>
template<typename InputIt> void ADS_set<Key, N>::insert(InputIt first, InputIt last){
    for(auto it{first}; it != last; ++it){
        if(!find_(*it)){
            insert_(*it);
        }
    }
}

template<typename Key, size_t N>
typename ADS_set<Key,N>::element* ADS_set<Key,N>::find_(const key_type& key) const{
    size_type idx = find_idx(key);

    if(table[idx] == nullptr || table[idx]->current == 0){
        return nullptr;
    }

    bucket* help = table[idx];
        while(help != nullptr){
            for(size_type i{0}; i < N; i++){
                if(help->block[i].mode == Mode::used && key_equal{}(help->block[i].key, key)){
                    return &help->block[i];
                }
            }
            help = help->overflow;
        }
    
    return nullptr;
}

template<typename Key, size_t N>
typename ADS_set<Key,N>::bucket* ADS_set<Key,N>::search_bucket(const key_type& key) const{
    size_type idx{find_idx(key)};
    bucket* tmp = table[idx];

    if(tmp == nullptr){
        return nullptr;
    }

    while(tmp != nullptr){
        if(tmp->find(key)){
            return tmp;
        }
        tmp = tmp->overflow;
    }
    return nullptr;
}

template <typename Key, size_t N>
class ADS_set<Key,N>::Iterator {
public:
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using reference = const value_type &;
  using pointer = const value_type *;
  using iterator_category = std::forward_iterator_tag;
private:
element* p{nullptr};
size_type idx{0};
size_type i{0};
size_type table_size{0};
bucket* b{nullptr};
bucket** table;


/* 
if (std::equal(v_it, v.end(), c_it, const_c->end(), std::equal_to<Key>{})){
                std::cout << "end equal" <<"\n";
               }
               */
void skip(){

    //SKIPPED NUR NULLPTR SKIPPED ELEMENTE DIE FREE ODER FREEAGAIN SIND SKIPPED LEERE FELDER



/*
    while(p->mode != Mode::used && idx < table_size){
        if(!p){
            p = &table[idx]->block[0];
        }
        if(p == &table[idx]->block[N-1]){
            if(table[idx]->overflow == nullptr){
                ++idx; 
                p = &table[idx]->block[0];           
            }else{
                p = &table[idx]->overflow->block[0];
            }
        }else{
            ++p;
        }
    }
  */  
  //begin() funktioniert
    for(; idx < table_size;++idx){
        if(!b){
            b = table[idx];
        }
        for(; b != nullptr; b = b->overflow){
            for(; i < b->current;++i){
                if(!p){
                    if(b->block[i].mode != Mode::used){
                        p = &b->block[i++];
                    }else{
                        p = &b->block[i];
                        return;
                    }
                }
                if(p && p < b->block + b->current){
                    return;
                }
                p = nullptr;
            }
            i = 0;
          }
          b = nullptr;
    }
}
public:
  explicit Iterator(bucket** table, size_type table_size) : p{nullptr}, idx{0}, i{0}, table_size{table_size}, b{nullptr}, table{table}{
    skip();
  }
  Iterator(bucket** table, element* p, size_type idx, size_type table_size ):  p{p}, idx{idx}, i{0}, table_size{table_size}, b{nullptr}, table{table} {
  }
  Iterator(): p{nullptr}, idx{0}, i{0}, table_size{0}, b{nullptr}, table{nullptr} {
  }

  reference operator*() const{
      return p->key;
  }
  pointer operator->() const{
      return &p->key;
  }
  Iterator &operator++(){
      //FALLS KETTE VORHANDEN IM BUCKET SUCHEN UND ELEMENT UPDATEN 
      ++p;
      skip();
      return *this;
  }
  Iterator operator++(int){
      auto rc{*this};
      ++*this;
      return rc;
  }
  friend bool operator==(const Iterator &lhs, const Iterator &rhs){
      return lhs.p == rhs.p;
  }
  friend bool operator!=(const Iterator &lhs, const Iterator &rhs){
      return !(lhs == rhs);
  }
};

template <typename Key, size_t N>
void swap(ADS_set<Key,N> &lhs, ADS_set<Key,N> &rhs) { lhs.swap(rhs); }

#endif // ADS_SET_H