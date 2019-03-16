#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        return UpdatePtr(&it->second.get(), value);
    }
    return AddNode(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        return false;
    }

    return AddNode(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        return UpdatePtr(&it->second.get(), value);
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }

    return DeleteIt(it);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }

    value = it->second.get().value;
    return true;
}

bool SimpleLRU::FreeSpace(size_t size) {
    if (_max_size < size) {
        return false;
    }
    while (_max_size - _cur_size < size) {
        DeleteOldest();
    }
    return true;
}


bool SimpleLRU::AddNode(const std::string &key, const std::string &value) {
    size_t node_size = key.size() + value.size();
    if (!FreeSpace(node_size)) {
        return false;
    }

    std::unique_ptr<lru_node> node(new lru_node{key, value});
    _cur_size += node_size;

    if (_lru_head) {
        node->prev = _lru_tail;
        _lru_tail->next.swap(node);
        _lru_tail = _lru_tail->next.get();
    } else {
        _lru_head.swap(node);
        _lru_tail = _lru_head.get();
    }

    return _lru_index.emplace(_lru_tail->key, *_lru_tail).second;
}

bool SimpleLRU::DeleteOldest() {
    if (!_lru_head) {
        return false;
    }
    _lru_index.erase(_lru_head->key);
    _cur_size -= _lru_head->key.size() + _lru_head->value.size();
    if (_lru_head->next) {
        std::unique_ptr<lru_node> tmp(nullptr);
        tmp.swap(_lru_head->next);
        _lru_head.swap(tmp);
        _lru_head->prev = nullptr;
        tmp.reset(nullptr);
    } else {
        _lru_head.reset();
        _lru_tail = nullptr;
    }
    return true;
}

bool SimpleLRU::DeleteIt(std::map<std::reference_wrapper<const std::string>,
                                      std::reference_wrapper<lru_node>,
                                      std::less<std::string>>::iterator it)  {
    lru_node *node = &it->second.get();
    _lru_index.erase(it);
    if (!node->prev) {
        DeleteOldest();
    } else {
        _cur_size -= node->key.size() + node->value.size();
        if (node->next) {
            node->prev->next.swap(node->next);
            node->next->prev = node->prev;
            delete node;
        } else {
            lru_node *tmp = _lru_tail;
            _lru_tail = _lru_tail->prev;
            delete tmp;
        }
    }
    return true;
}

void SimpleLRU::Move2Tail(lru_node *node) {
    if (node == _lru_tail) {
        return;
    }
    std::unique_ptr<lru_node> tmp(nullptr);
    if (node->prev) {
        tmp.swap(node->next);
        node->prev->next.swap(tmp);
        node->next->prev = node->prev;
    } else {
        tmp.swap(_lru_head->next);
        _lru_head.swap(tmp);
        _lru_head->prev = nullptr;
        tmp->prev = _lru_tail;

    }
    _lru_tail->next.swap(tmp);
    _lru_tail = node;
}

bool SimpleLRU::UpdatePtr(lru_node *node, const std::string &value) {
    int dsize = value.size() - node->value.size();
    if (dsize > 0) {
        if (!FreeSpace(dsize)) {
            return false;
        }
    }

    Move2Tail(node);

    _lru_tail->value = value;
    _cur_size += dsize;

    return true;
}

} // namespace Backend
} // namespace Afina
