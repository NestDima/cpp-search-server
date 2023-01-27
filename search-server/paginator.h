#pragma once

#include <vector>

template<typename Iterator>
    class IteratorRange {
    public:
        IteratorRange(const Iterator begin, const Iterator end)
        : page_(begin, end)
        {}
        auto begin() const {
            return page_.first;
        }
        auto end() const {
            return page_.second;
        }
        size_t size() const {
            return (distance(begin, end - 1));
        }

    private:
        std::pair<Iterator, Iterator> page_;
    };

template<typename Iterator>
std::ostream& operator <<(std::ostream& os, const IteratorRange<Iterator>& page ) {
    for (auto it = page.begin(); it != page.end(); ++it) {
        os << *it;
    }
    return os;
}

template <typename Iterator>
    class Paginator {
    public:
        Paginator(Iterator begin, Iterator end, int size) : page_size_(size){
            Iterator begining = begin;
            Iterator ending = begin;
            while(distance(ending, end) > size) {
                advance(ending, size);
                pages_.push_back(IteratorRange(begining, ending));
                begining = ending;
            }
            if (begining < end) {
                pages_.push_back(IteratorRange(ending, end));
            }
        }

    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
    int size() const {
        return page_size_;
    }

    private:
        int page_size_;
        std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}