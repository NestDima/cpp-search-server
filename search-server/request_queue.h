#pragma once

#include <deque>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        ++current_time_;
        const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        RemoveOld();
        uint64_t res = result.size();
        requests_.push_back({current_time_, res});
        if (0 == res) {
            ++requests_without_resault_;
        }
        return {};
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
    void RemoveOld();
private:
    struct QueryResult {
        uint64_t time;
        uint64_t results;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int requests_without_resault_;
    uint64_t current_time_;
};